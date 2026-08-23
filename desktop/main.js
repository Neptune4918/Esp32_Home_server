const { app, BrowserWindow, Tray, Menu, ipcMain, screen } = require('electron');
const path = require('path');
const fs = require('fs');
const http = require('http');

const configPath = path.join(app.getPath('userData'), 'config.json');

// Windows enforces a minimum top-level window width/height (SM_CXMIN/SM_CYMIN)
// even for frameless BrowserWindows, which in testing on this machine clamped
// anything smaller than ~64px back up to 64px. Using a value at/above that
// floor keeps the window fully on-screen (otherwise the extra forced width
// spills past the screen edge and the visible bubble ends up mis-anchored).
const HOTZONE_SIZE = 64;   // px width of the thin hover strip when collapsed
const PANEL_WIDTH = 300;   // px width of the fully expanded panel
const ANIMATION_MS = 220;
const ANIMATION_STEPS = 12;

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

function boundsFor(widthPx) {
  const wa = getDisplayWorkArea();
  const x = config.edge === 'left' ? wa.x : wa.x + wa.width - widthPx;
  return { x, y: wa.y, width: widthPx, height: wa.height };
}

function animateTo(targetWidth, onDone) {
  if (!panelWindow) return;
  animating = true;
  const start = panelWindow.getBounds().width;
  const delta = targetWidth - start;
  let step = 0;

  const timer = setInterval(() => {
    step += 1;
    const progress = step / ANIMATION_STEPS;
    const width = Math.round(start + delta * progress);
    panelWindow.setBounds(boundsFor(width));

    if (step >= ANIMATION_STEPS) {
      clearInterval(timer);
      panelWindow.setBounds(boundsFor(targetWidth));
      animating = false;
      if (onDone) onDone();
    }
  }, ANIMATION_MS / ANIMATION_STEPS);
}

function expandPanel() {
  if (expanded || animating) return;
  expanded = true;
  panelWindow.webContents.send('panel:state', 'expanded');
  animateTo(PANEL_WIDTH);
}

function collapsePanel() {
  if (!expanded || animating) return;
  expanded = false;
  animateTo(HOTZONE_SIZE, () => {
    panelWindow.webContents.send('panel:state', 'collapsed');
  });
}

function repositionForEdgeChange() {
  if (!panelWindow) return;
  const width = expanded ? PANEL_WIDTH : HOTZONE_SIZE;
  panelWindow.setBounds(boundsFor(width));
}

function fetchDashboardData(address) {
  return new Promise((resolve, reject) => {
    const url = /^https?:\/\//i.test(address) ? address + '/data' : 'http://' + address + '/data';
    const req = http.get(url, { timeout: 5000 }, (res) => {
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

function pollData() {
  fetchDashboardData(config.address)
    .then((data) => {
      if (panelWindow) panelWindow.webContents.send('data:update', data);
    })
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
  panelWindow = new BrowserWindow({
    width: HOTZONE_SIZE,
    height: getDisplayWorkArea().height,
    x: boundsFor(HOTZONE_SIZE).x,
    y: boundsFor(HOTZONE_SIZE).y,
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
