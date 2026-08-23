// One-off script to (re)generate assets/tray-icon.ico as a proper
// multi-resolution icon (electron-builder requires >=256x256 for the
// Windows app/installer icon; the old file was only 32x32).
//
// Run with: node assets/generate-icon.js
const path = require('path');
const { Jimp } = require('jimp');
const { imagesToIco } = require('png-to-ico');

const SIZE = 256;
const CENTER = SIZE / 2;
const BLUE = 0x2b7de9ff; // matches the panel's accent blue
const WHITE = 0xffffffff;
const TRANSPARENT = 0x00000000;

async function main() {
  const img = new Jimp({ width: SIZE, height: SIZE, color: TRANSPARENT });

  const outerR = SIZE * 0.47;
  const ringOuterR = SIZE * 0.36;
  const ringInnerR = SIZE * 0.27;
  const needleLen = SIZE * 0.3;
  const needleAngle = (-40 * Math.PI) / 180; // pointing up-right, like a gauge reading
  const needleWidth = SIZE * 0.035;

  img.scan(0, 0, SIZE, SIZE, function (x, y, idx) {
    const dx = x - CENTER;
    const dy = y - CENTER;
    const dist = Math.sqrt(dx * dx + dy * dy);

    let color = TRANSPARENT;

    if (dist <= outerR) {
      color = BLUE;

      // White dial ring (like a gauge face).
      if (dist <= ringOuterR && dist >= ringInnerR) {
        color = WHITE;
      }

      // Needle: distance from the point (x,y) to the line segment from
      // center to the tip, thickened by needleWidth.
      const tipX = CENTER + needleLen * Math.cos(needleAngle);
      const tipY = CENTER + needleLen * Math.sin(needleAngle);
      const vx = tipX - CENTER;
      const vy = tipY - CENTER;
      const wx = x - CENTER;
      const wy = y - CENTER;
      const segLenSq = vx * vx + vy * vy;
      let t = (wx * vx + wy * vy) / segLenSq;
      t = Math.max(0, Math.min(1, t));
      const projX = CENTER + t * vx;
      const projY = CENTER + t * vy;
      const distToSeg = Math.sqrt((x - projX) ** 2 + (y - projY) ** 2);
      if (distToSeg <= needleWidth && dist <= ringInnerR + 2) {
        color = WHITE;
      }

      // Center hub dot.
      if (dist <= SIZE * 0.045) {
        color = WHITE;
      }
    }

    img.bitmap.data.writeUInt32BE(color, idx);
  });

  const sizes = [256, 48, 32, 16];
  const bitmaps = [];
  for (const s of sizes) {
    const resized = img.clone().resize({ w: s, h: s });
    bitmaps.push({ width: resized.bitmap.width, height: resized.bitmap.height, data: resized.bitmap.data });
  }

  const icoBuffer = await imagesToIco(bitmaps);
  const outPath = path.join(__dirname, 'tray-icon.ico');
  require('fs').writeFileSync(outPath, icoBuffer);
  console.log('Wrote', outPath, '(' + icoBuffer.length + ' bytes)');
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
