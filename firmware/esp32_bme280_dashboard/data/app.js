const roomEls = new Map(); // device id -> cloned room element

function createRoomEl(id) {
  const tpl = document.getElementById('room-template');
  const node = tpl.content.firstElementChild.cloneNode(true);
  document.getElementById('rooms').appendChild(node);
  roomEls.set(id, node);
  return node;
}

function updateRoom(root, d) {
  const set = (sel, val) => {
    const e = root.querySelector(sel);
    if (e) e.textContent = val;
  };

  root.querySelector('.room-name').textContent = d.name + (d.online ? '' : ' (offline)');
  set('.room-wifi', 'Status: ' + d.wifi);
  set('.value-temp', d.temp.toFixed(1) + ' °C');
  set('.value-humid', d.humid.toFixed(1) + ' %');
  set('.value-press', d.press.toFixed(1) + ' kPa');
  set('.room-lastMeasurement', d.lastMeasurement);
  set('.room-lastUpload', d.lastUpload);
  set('.room-nextUpload', d.nextUpload);
  set('.room-comfort', d.comfort);
  set('.room-tempStats', 'Temp min/max: ' + d.tempMin.toFixed(1) + ' / ' + d.tempMax.toFixed(1) + ' °C');
  set('.room-humidStats', 'Humidity min/max: ' + d.humidMin.toFixed(1) + ' / ' + d.humidMax.toFixed(1) + ' %');
  set('.room-pressStats', 'Pressure min/max: ' + d.pressMin.toFixed(1) + ' / ' + d.pressMax.toFixed(1) + ' kPa');

  const t = root.querySelector('.room-trend');
  if (t) {
    t.textContent = d.trendText;
    t.className = 'room-trend trend ' + d.trendClass;
  }

  const needleTemp = root.querySelector('.needle-temp');
  if (needleTemp) needleTemp.style.transform = 'rotate(' + d.tempAngle + 'deg) translateX(-50%)';

  const needleHumid = root.querySelector('.needle-humid');
  if (needleHumid) needleHumid.style.transform = 'rotate(' + d.humidAngle + 'deg) translateX(-50%)';

  const needlePress = root.querySelector('.needle-press');
  if (needlePress) needlePress.style.transform = 'rotate(' + d.pressAngle + 'deg) translateX(-50%)';

  root.classList.toggle('offline', !d.online);
}

async function refreshData() {
  try {
    const r = await fetch('/data', { cache: 'no-store' });
    const devices = await r.json();

    for (const d of devices) {
      let root = roomEls.get(d.id);
      if (!root) root = createRoomEl(d.id);
      updateRoom(root, d);
    }

    const m = document.querySelector('.meta');
    if (m) m.classList.add('updated');
  } catch (e) {
    // Ignore transient network errors; next interval will retry.
  }
}

window.addEventListener('load', () => {
  refreshData();
  setInterval(refreshData, 15000);
});

