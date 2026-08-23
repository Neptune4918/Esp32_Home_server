let config = null;
let hideTimer = null;

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
  const container = document.getElementById('extraInfo');
  container.innerHTML = '';
  const widgets = config.widgets || {};

  if (widgets.comfort) {
    container.innerHTML += `<div class="pill">Comfort: ${data.comfort ?? 'N/A'}</div>`;
  }
  if (widgets.trend) {
    container.innerHTML += `<div class="pill">Trend: ${data.trendText ?? 'N/A'}</div>`;
  }
  if (widgets.wifi) {
    container.innerHTML += `<div class="pill">Wi-Fi: ${data.wifi ?? 'N/A'}</div>`;
  }
  if (widgets.lastUpload) {
    container.innerHTML += `<div class="pill">Last upload: ${data.lastUpload ?? 'N/A'}</div>`;
  }
}

function renderData(data) {
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
