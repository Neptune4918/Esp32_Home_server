async function refreshData() {
  try {
    const r = await fetch('/data', { cache: 'no-store' });
    const d = await r.json();

    const set = (sel, val) => {
      const e = document.querySelector(sel);
      if (e) e.textContent = val;
    };

    set('#temp', d.temp.toFixed(1) + ' °C');
    set('#humid', d.humid.toFixed(1) + ' %');
    set('#press', d.press.toFixed(1) + ' kPa');
    set('#lastMeasurement', d.lastMeasurement);
    set('#lastUpload', d.lastUpload);
    set('#nextUpload', d.nextUpload);
    set('#wifi', 'Wi-Fi: ' + d.wifi);
    set('#comfort', d.comfort);
    set('#tempStats', 'Temp min/max: ' + d.tempMin.toFixed(1) + ' / ' + d.tempMax.toFixed(1) + ' °C');
    set('#humidStats', 'Humidity min/max: ' + d.humidMin.toFixed(1) + ' / ' + d.humidMax.toFixed(1) + ' %');
    set('#pressStats', 'Pressure min/max: ' + d.pressMin.toFixed(1) + ' / ' + d.pressMax.toFixed(1) + ' kPa');

    const t = document.querySelector('#trend');
    if (t) {
      t.textContent = d.trendText;
      t.className = 'trend ' + d.trendClass;
    }

    const needleTemp = document.querySelector('#needle-temp');
    if (needleTemp) needleTemp.style.transform = 'rotate(' + d.tempAngle + 'deg) translateX(-50%)';

    const needleHumid = document.querySelector('#needle-humid');
    if (needleHumid) needleHumid.style.transform = 'rotate(' + d.humidAngle + 'deg) translateX(-50%)';

    const needlePress = document.querySelector('#needle-press');
    if (needlePress) needlePress.style.transform = 'rotate(' + d.pressAngle + 'deg) translateX(-50%)';

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
