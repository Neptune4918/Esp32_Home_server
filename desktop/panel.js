let config = null;
let hideTimer = null;
let lastData = {}; // last known sensor payload, so pills can render immediately on save

const body = document.body;
const bubble = document.getElementById('bubble');
const panel = document.getElementById('panel');
const dashboardView = document.getElementById('dashboardView');
const settingsView = document.getElementById('settingsView');

const settingsBtn = document.getElementById('settingsBtn');
const collapseBtn = document.getElementById('collapseBtn');
const quitBtn = document.getElementById('quitBtn');
const measureNowBtn = document.getElementById('measureNowBtn');
const saveSettingsBtn = document.getElementById('saveSettingsBtn');
const backBtn = document.getElementById('backBtn');

// Extra-info "pills" (Comfort/Trend/Wi-Fi/Last upload) are persistent DOM
// nodes toggled via a CSS class rather than rebuilt with innerHTML each
// render. That's what lets them animate in/out smoothly instead of just
// popping into existence, and lets Save show/hide them instantly.
const PILL_WIDGETS = ['comfort', 'trend', 'wifi', 'lastUpload'];
const PILL_TRANSITION_MS = 320; // must match the CSS transition duration
const pillEls = {};
const pillHideTimers = {};
PILL_WIDGETS.forEach((key) => {
  pillEls[key] = document.getElementById(`pill-${key}`);
});

function setPillText(key, text) {
  const el = pillEls[key];
  if (el) el.textContent = text;
}

function showPill(key) {
  const el = pillEls[key];
  if (!el) return;
  if (pillHideTimers[key]) {
    clearTimeout(pillHideTimers[key]);
    delete pillHideTimers[key];
  }
  if (el.classList.contains('visible')) return;
  el.style.display = 'block';
  // Force a style flush so the browser registers the "off-screen, invisible"
  // starting state before we flip to "visible" on the next frame - otherwise
  // both changes get batched into one and the transition never plays.
  void el.offsetWidth;
  requestAnimationFrame(() => el.classList.add('visible'));
}

function hidePill(key) {
  const el = pillEls[key];
  if (!el) return;
  if (!el.classList.contains('visible')) {
    el.style.display = 'none';
    return;
  }
  el.classList.remove('visible');
  if (pillHideTimers[key]) clearTimeout(pillHideTimers[key]);
  pillHideTimers[key] = setTimeout(() => {
    el.style.display = 'none';
    delete pillHideTimers[key];
  }, PILL_TRANSITION_MS);
}

function applyEdgeAttribute(edge) {
  body.setAttribute('data-edge', edge);
}

function fillSettingsForm(cfg) {
  document.getElementById('address').value = cfg.address || '';
  document.querySelectorAll('input[name="edge"]').forEach((radio) => {
    radio.checked = radio.value === cfg.edge;
  });
  document.getElementById('autoHideDelay').value = Math.round((cfg.autoHideDelayMs || 3000) / 1000);
  document.getElementById('widgetComfort').checked = !!cfg.widgets.comfort;
  document.getElementById('widgetTrend').checked = !!cfg.widgets.trend;
  document.getElementById('widgetWifi').checked = !!cfg.widgets.wifi;
  document.getElementById('widgetLastUpload').checked = !!cfg.widgets.lastUpload;
}

function showDashboardView() {
  dashboardView.classList.remove('hidden');
  settingsView.classList.add('hidden');
}

function showSettingsView() {
  fillSettingsForm(config);
  settingsView.classList.remove('hidden');
  dashboardView.classList.add('hidden');
}

function renderExtraInfo(data) {
  const widgets = (config && config.widgets) || {};
  const texts = {
    comfort: `Comfort: ${data.comfort ?? 'N/A'}`,
    trend: `Trend: ${data.trendText ?? 'N/A'}`,
    wifi: `Wi-Fi: ${data.wifi ?? 'N/A'}`,
    lastUpload: `Last upload: ${data.lastUpload ?? 'N/A'}`,
  };
  PILL_WIDGETS.forEach((key) => {
    setPillText(key, texts[key]);
    if (widgets[key]) showPill(key);
    else hidePill(key);
  });
}

function renderData(data) {
  lastData = data;
  document.getElementById('temp').textContent = `${Number(data.temp).toFixed(1)} °C`;
  document.getElementById('humid').textContent = `${Number(data.humid).toFixed(1)} %`;
  document.getElementById('press').textContent = `${Number(data.press).toFixed(1)} kPa`;

  document.getElementById('needle-temp').style.transform = `rotate(${data.tempAngle}deg) translateX(-50%)`;
  document.getElementById('needle-humid').style.transform = `rotate(${data.humidAngle}deg) translateX(-50%)`;
  document.getElementById('needle-press').style.transform = `rotate(${data.pressAngle}deg) translateX(-50%)`;

  document.getElementById('statusLine').textContent = data.lastMeasurement
    ? `Last measurement: ${data.lastMeasurement}`
    : 'Waiting for data...';

  renderExtraInfo(data);
}

function scheduleAutoHide() {
  if (hideTimer) clearTimeout(hideTimer);
  const delay = (config && config.autoHideDelayMs) || 3000;
  hideTimer = setTimeout(() => {
    window.panelAPI.collapse();
  }, delay);
}

function cancelAutoHide() {
  if (hideTimer) {
    clearTimeout(hideTimer);
    hideTimer = null;
  }
}

bubble.addEventListener('click', () => window.panelAPI.expand());
collapseBtn.addEventListener('click', () => window.panelAPI.collapse());
quitBtn.addEventListener('click', () => window.panelAPI.quit());
settingsBtn.addEventListener('click', showSettingsView);
backBtn.addEventListener('click', showDashboardView);

measureNowBtn.addEventListener('click', () => {
  measureNowBtn.disabled = true;
  measureNowBtn.textContent = 'Measuring...';
  window.panelAPI.measureNow();
});

panel.addEventListener('mouseenter', cancelAutoHide);
panel.addEventListener('mouseleave', scheduleAutoHide);

saveSettingsBtn.addEventListener('click', async () => {
  const edgeInput = document.querySelector('input[name="edge"]:checked');
  const newConfig = {
    address: document.getElementById('address').value.trim() || config.address,
    edge: edgeInput ? edgeInput.value : config.edge,
    autoHideDelayMs: Math.max(1, Number(document.getElementById('autoHideDelay').value) || 3) * 1000,
    widgets: {
      comfort: document.getElementById('widgetComfort').checked,
      trend: document.getElementById('widgetTrend').checked,
      wifi: document.getElementById('widgetWifi').checked,
      lastUpload: document.getElementById('widgetLastUpload').checked,
    },
  };

  config = await window.panelAPI.saveConfig(newConfig);
  applyEdgeAttribute(config.edge);
  renderExtraInfo(lastData); // show/hide pills immediately, don't wait for next poll
  showDashboardView();
});

window.panelAPI.onState((state) => {
  if (state === 'expanded') {
    body.classList.remove('collapsed');
    body.classList.add('expanded');
    showDashboardView();
  } else {
    body.classList.remove('expanded');
    body.classList.add('collapsed');
    cancelAutoHide();
  }
});

window.panelAPI.onDataUpdate((data) => {
  renderData(data);
  measureNowBtn.disabled = false;
  measureNowBtn.textContent = 'Measure now';
});

window.panelAPI.onDataError((message) => {
  document.getElementById('statusLine').textContent = `Connection error: ${message}`;
  measureNowBtn.disabled = false;
  measureNowBtn.textContent = 'Measure now';
});

window.panelAPI.getConfig().then((cfg) => {
  config = cfg;
  applyEdgeAttribute(config.edge);
});
