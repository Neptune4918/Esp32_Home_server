const { app, BrowserWindow, Tray, Menu, ipcMain, screen } = require('electron');
const path = require('path');
const fs = require('fs');
const http = require('http');

const configPath = path.join(app.getPath('userData'), 'config.json');

// Windows enforces a minimum top-level window width/height (SM_CXMIN/SM_CYMIN)
// even for frameless BrowserWindows, which in testing on this machine clamped
// anything smaller than ~64px back up to 64px. HOTZONE_SIZE is picked above
// that floor anyway since the user wants a visibly bigger/wider bubble.
const HOTZONE_SIZE = 90;   // px width of the hover strip when collapsed
const PANEL_WIDTH = 300;   // px width of the fully expanded panel
const PANEL_GAP = 10;      // px gap between the floating panel and the true screen edge
const VERTICAL_SPAN = 0.5; // bubble/panel occupy the middle 50% of the screen height (25%-75%)
const ANIMATION_MS = 320;
const ANIMATION_FRAME_MS = 16; // ~60fps steps

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
let animating = false;
let pollTimer = null;

function getDisplayWorkArea() {
  return screen.getPrimaryDisplay().workArea;
}

// widthPx: current window width. gapPx: distance between the window's outer
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

function easeOutCubic(t) {
  return 1 - Math.pow(1 - t, 3);
}

function animateTo(targetWidth, targetGap, onDone) {
  if (!panelWindow) return;
  animating = true;
  const startBounds = panelWindow.getBounds();
  const startWidth = startBounds.width;
  const startGap = expanded ? PANEL_GAP : 0; // gap state we're animating away from
  const totalSteps = Math.max(1, Math.round(ANIMATION_MS / ANIMATION_FRAME_MS));
  let step = 0;

  const timer = setInterval(() => {
    step += 1;
    const progress = easeOutCubic(Math.min(1, step / totalSteps));
    const width = Math.round(startWidth + (targetWidth - startWidth) * progress);
    const gap = startGap + (targetGap - startGap) * progress;
    panelWindow.setBounds(boundsFor(width, gap));

    if (step >= totalSteps) {
      clearInterval(timer);
      panelWindow.setBounds(boundsFor(targetWidth, targetGap));
      animating = false;
      if (onDone) onDone();
    }
  }, ANIMATION_FRAME_MS);
}

function expandPanel() {
  if (expanded || animating) return;
  expanded = true;
  panelWindow.webContents.send('panel:state', 'expanded');
  animateTo(PANEL_WIDTH, PANEL_GAP);
}

function collapsePanel() {
  if (!expanded || animating) return;
  expanded = false;
  animateTo(HOTZONE_SIZE, 0, () => {
    panelWindow.webContents.send('panel:state', 'collapsed');
  });
}

function repositionForEdgeChange() {
  if (!panelWindow) return;
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

  const menu = Menu.buildFromTemplate([
    { label: 'Show panel', click: () => expandPanel() },
    { label: 'Hide panel', click: () => collapsePanel() },
    { type: 'separator' },
    { label: 'Quit', click: () => app.quit() },
  ]);
  tray.setContextMenu(menu);
  tray.on('click', () => {
    if (expanded) collapsePanel();
    else expandPanel();
  });
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
});

app.on('window-all-closed', () => {
  // Intentionally do nothing: keep the app alive in the tray even if the
  // panel window were ever closed (quitting only happens via the tray menu
  // or the panel's Quit button).
});
