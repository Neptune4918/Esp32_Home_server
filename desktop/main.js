const { app, BrowserWindow, Tray, Menu, ipcMain, screen, Notification } = require('electron');
const path = require('path');
const fs = require('fs');
const http = require('http');
const { autoUpdater } = require('electron-updater');

const configPath = path.join(app.getPath('userData'), 'config.json');

// Windows enforces a minimum top-level window width/height (SM_CXMIN/SM_CYMIN)
// even for frameless BrowserWindows, which in testing on this machine clamped
// anything smaller than ~64px back up to 64px. HOTZONE_SIZE is picked above
// that floor anyway since the user wants a visibly bigger/wider bubble.
const HOTZONE_SIZE = 90;   // px width of the hover strip when collapsed
const PANEL_WIDTH = 300;   // px width of the fully expanded panel
const PANEL_GAP = 10;      // px gap between the floating panel and the true screen edge
const VERTICAL_SPAN = 0.5; // bubble/panel occupy the middle 50% of the screen height (25%-75%)

// The reveal/hide itself is a pure CSS opacity+transform transition in the
// renderer (GPU-composited, real 60fps) — the native window is only ever
// resized ONCE per transition, at a moment when the content is fully
// invisible (opacity 0), so there is no visible "jump" and no risk of the
// old scrollbar-flash bug (which happened because the window shrank frame by
// frame while the panel was still visibly `display:flex`). This also avoids
// needing any click-through/setIgnoreMouseEvents trickery: the window is
// always exactly the size of whatever should currently be interactive.
const REVEAL_MS = 320; // must match the CSS transition duration in panel.css

const defaultConfig = {
  address: 'esp32-home.local',
  edge: 'right', // 'left' | 'right'
  autoHideDelayMs: 3000,
  widgets: {
    comfort: false,
    trend: false,
    wifi: false,
    lastUpload: false,
  },
};

function loadConfig() {
  try {
    const raw = fs.readFileSync(configPath, 'utf-8');
    const parsed = JSON.parse(raw);
    return {
      ...defaultConfig,
      ...parsed,
      widgets: { ...defaultConfig.widgets, ...(parsed.widgets || {}) },
    };
  } catch (e) {
    return { ...defaultConfig };
  }
}

function saveConfig(config) {
  fs.writeFileSync(configPath, JSON.stringify(config, null, 2), 'utf-8');
}

let config = loadConfig();
let panelWindow;
let tray;
let expanded = false;
let pollTimer = null;
let collapseTimer = null; // pending native shrink after the collapse CSS animation finishes

function getDisplayWorkArea() {
  return screen.getPrimaryDisplay().workArea;
}

// widthPx: target window width. gapPx: distance between the window's outer
// edge and the true screen edge (0 when collapsed/flush, PANEL_GAP when the
// panel is floating). Both bubble and panel share the same 25%-75% vertical
// span of the screen.
function boundsFor(widthPx, gapPx) {
  const wa = getDisplayWorkArea();
  const height = Math.round(wa.height * VERTICAL_SPAN);
  const y = wa.y + Math.round((wa.height - height) / 2);
  const gap = gapPx || 0;
  const x = config.edge === 'left'
    ? wa.x + gap
    : wa.x + wa.width - widthPx - gap;
  return { x, y, width: widthPx, height };
}

function expandPanel() {
  if (expanded) return;
  expanded = true;
  if (collapseTimer) {
    clearTimeout(collapseTimer);
    collapseTimer = null;
  }
  // Grow the window to the panel's exact size FIRST, while it's still fully
  // transparent/invisible (opacity 0) — then trigger the CSS reveal. The
  // resize itself is invisible because there's nothing rendered yet at the
  // new size.
  panelWindow.setBounds(boundsFor(PANEL_WIDTH, PANEL_GAP));
  panelWindow.webContents.send('panel:state', 'expanded');
}

function collapsePanel() {
  if (!expanded) return;
  expanded = false;
  // Tell the renderer to start the fade/slide-out CSS transition first...
  panelWindow.webContents.send('panel:state', 'collapsed');
  // ...and only shrink the native window back down to the hotzone size once
  // that transition has finished, so the panel is fully invisible by the
  // time the resize happens (no visible clipping/flash).
  if (collapseTimer) clearTimeout(collapseTimer);
  collapseTimer = setTimeout(() => {
    panelWindow.setBounds(boundsFor(HOTZONE_SIZE, 0));
    collapseTimer = null;
  }, REVEAL_MS);
}

function repositionForEdgeChange() {
  if (!panelWindow) return;
  if (collapseTimer) {
    clearTimeout(collapseTimer);
    collapseTimer = null;
  }
  const width = expanded ? PANEL_WIDTH : HOTZONE_SIZE;
  const gap = expanded ? PANEL_GAP : 0;
  panelWindow.setBounds(boundsFor(width, gap));
}

function buildUrl(address, routePath) {
  const base = /^https?:\/\//i.test(address) ? address : 'http://' + address;
  return base + routePath;
}

function fetchDashboardData(address) {
  return new Promise((resolve, reject) => {
    const req = http.get(buildUrl(address, '/data'), { timeout: 5000 }, (res) => {
      let body = '';
      res.on('data', (chunk) => (body += chunk));
      res.on('end', () => {
        try {
          resolve(JSON.parse(body));
        } catch (e) {
          reject(e);
        }
      });
    });
    req.on('timeout', () => req.destroy(new Error('timeout')));
    req.on('error', reject);
  });
}

function triggerMeasure(address) {
  return new Promise((resolve, reject) => {
    // /measure does a local-only BME280 read on the ESP32 (no ThingSpeak
    // upload) and replies with a redirect, same as the web dashboard's
    // "Measure now" button. We just need the read to happen, then we
    // re-poll /data to pick up the fresh values.
    const req = http.get(buildUrl(address, '/measure'), { timeout: 5000 }, (res) => {
      res.resume(); // drain response
      res.on('end', resolve);
    });
    req.on('timeout', () => req.destroy(new Error('timeout')));
    req.on('error', reject);
  });
}

function pollData() {
  fetchDashboardData(config.address)
    .then((data) => {
      if (panelWindow) panelWindow.webContents.send('data:update', data);
    })
    .catch((err) => {
      if (panelWindow) panelWindow.webContents.send('data:error', String(err.message || err));
    });
}

function measureNow() {
  triggerMeasure(config.address)
    .then(() => pollData())
    .catch((err) => {
      if (panelWindow) panelWindow.webContents.send('data:error', String(err.message || err));
    });
}

function startPolling() {
  if (pollTimer) clearInterval(pollTimer);
  pollData();
  pollTimer = setInterval(pollData, 15000);
}

function createPanelWindow() {
  const initial = boundsFor(HOTZONE_SIZE, 0);
  panelWindow = new BrowserWindow({
    width: initial.width,
    height: initial.height,
    x: initial.x,
    y: initial.y,
    frame: false,
    transparent: true,
    hasShadow: false,
    resizable: false,
    movable: false,
    minimizable: false,
    maximizable: false,
    fullscreenable: false,
    skipTaskbar: true,
    alwaysOnTop: true,
    show: true,
    backgroundColor: '#00000000',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
    },
  });

  panelWindow.setAlwaysOnTop(true, 'screen-saver');
  panelWindow.setVisibleOnAllWorkspaces(true, { visibleOnFullScreen: true });
  panelWindow.loadFile(path.join(__dirname, 'panel.html'));
}

function createTray() {
  tray = new Tray(path.join(__dirname, 'assets', 'tray-icon.ico'));
  tray.setToolTip('ESP32 Home Dashboard');
  buildTrayMenu();
  tray.on('click', () => {
    if (expanded) collapsePanel();
    else expandPanel();
  });
}

// Rebuilt whenever update state changes so the "Restart to update" item can
// appear/disappear without recreating the Tray itself.
function buildTrayMenu() {
  if (!tray) return;
  const items = [
    { label: 'Show panel', click: () => expandPanel() },
    { label: 'Hide panel', click: () => collapsePanel() },
    { type: 'separator' },
  ];
  if (updateReady) {
    items.push(
      { label: 'Restart to install update', click: () => autoUpdater.quitAndInstall() },
      { type: 'separator' },
    );
  }
  items.push({ label: 'Quit', click: () => app.quit() });
  tray.setContextMenu(Menu.buildFromTemplate(items));
}

// --- Auto-update ---
// Updates are published as GitHub Releases (see package.json "build.publish"
// and "npm run release"). electron-updater checks that feed, downloads a new
// installer in the background if one is newer, and we let the user pick when
// to actually restart (via the tray menu) rather than forcing a restart.
let updateReady = false;

function setupAutoUpdates() {
  // Skip entirely in dev mode (`npm start`): there's no packaged installer
  // to update, and checkForUpdates() would just log a harmless but noisy
  // "not packaged" error.
  if (!app.isPackaged) return;

  autoUpdater.autoDownload = true;
  autoUpdater.autoInstallOnAppQuit = true; // installs silently on next quit even if not clicked

  autoUpdater.on('update-downloaded', (info) => {
    updateReady = true;
    buildTrayMenu();
    if (Notification.isSupported()) {
      new Notification({
        title: 'ESP32 Home Dashboard update ready',
        body: `Version ${info.version} downloaded. Right-click the tray icon to restart and install, or it will install next time you quit.`,
      }).show();
    }
  });

  autoUpdater.on('error', (err) => {
    // Non-fatal: the app keeps running on the current version. Logged only
    // for troubleshooting (e.g. via the "npm start" console) — never shown
    // to the user, since a failed background update check shouldn't be
    // alarming.
    console.error('[auto-update] error:', err == null ? err : err.message || err);
  });

  autoUpdater.checkForUpdates();
  // Also re-check periodically since this is a long-running tray app that
  // may stay open for days without a restart.
  setInterval(() => autoUpdater.checkForUpdates(), 6 * 60 * 60 * 1000); // every 6h
}

ipcMain.on('panel:expand', () => expandPanel());
ipcMain.on('panel:collapse', () => collapsePanel());
ipcMain.on('panel:quit', () => app.quit());
ipcMain.on('panel:measureNow', () => measureNow());

ipcMain.handle('config:get', () => config);

ipcMain.handle('config:save', (_event, newConfig) => {
  const edgeChanged = newConfig.edge !== config.edge;
  const addressChanged = newConfig.address !== config.address;
  config = {
    ...config,
    ...newConfig,
    widgets: { ...config.widgets, ...(newConfig.widgets || {}) },
  };
  saveConfig(config);
  if (edgeChanged) repositionForEdgeChange();
  if (addressChanged) startPolling();
  return config;
});

app.whenReady().then(() => {
  createPanelWindow();
  createTray();
  startPolling();
  setupAutoUpdates();
});

app.on('window-all-closed', () => {
  // Intentionally do nothing: keep the app alive in the tray even if the
  // panel window were ever closed (quitting only happens via the tray menu
  // or the panel's Quit button).
});
