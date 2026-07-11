// main.js -- browser UI for the Image Scramble engine (engine.js/engine.wasm,
// built from web/wasm/). All puzzle logic lives in the WASM; this file only
// moves pixels between the engine heap and a canvas, and wires up controls.

import createEngine from './engine.js';

const $ = (id) => document.getElementById(id);
const canvas = $('canvas');
const ctx = canvas.getContext('2d');

const logBox = $('log');
const log = (msg) => {
  logBox.value += msg + '\n';
  logBox.scrollTop = logBox.scrollHeight;
};

const Module = await createEngine({
  onLog: log,
  onDraw: () => { paint(); updateProgress(); drawNumbers(); },
});

// Addresses cached once; read/written via heap views because exports must not
// be called while the engine is suspended mid-operation (Asyncify).
const ptr = {
  stop: Module._eng_stop_ptr(),
  swirl: Module._eng_swirl_ptr(),
  direction: Module._eng_direction_ptr(),
  count: Module._eng_count_ptr(),
  solved: Module._eng_pixels_solved_ptr(),
  drawEvery: Module._eng_draw_every_ptr(),
  slowMs: Module._eng_slow_ms_ptr(),
};

let running = false;
let currentOp = null;

// Full-resolution copy of the loaded image, kept so the Resolution slider can
// decimate down (short side as low as 4px) and restore back up losslessly.
let source = null; // { rgba: Uint8ClampedArray, w, h }

function paint() {
  const w = canvas.width, h = canvas.height;
  if (!w || !h) return;
  const rgb = Module.HEAPU8;
  let src = Module._eng_pixels();
  const img = ctx.createImageData(w, h);
  const out = img.data;
  for (let i = 0, o = 0; i < w * h; i++, o += 4) {
    out[o] = rgb[src++];
    out[o + 1] = rgb[src++];
    out[o + 2] = rgb[src++];
    out[o + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

function updateProgress() {
  const total = canvas.width * canvas.height;
  const solved = Module.HEAP32[ptr.solved >> 2];
  // The engine's reporting counter is a double (exact to 2^53) — the uint32
  // counter wraps at ~4.3e9, which big FlipSolves genuinely exceed.
  const count = Module.HEAPF64[ptr.count >> 3];
  // Progress tracks the solvers; an endless scramble has no destination.
  const pct = currentOp === 'eng_scramble' ? 0 : total ? 100 * solved / total : 0;
  $('progress').style.width = `${pct.toFixed(1)}%`;
  $('status').textContent = running ? `moves: ${count.toLocaleString()}` : '';
}

// Single speed ladder, fastest (right) to slowest (left):
// slider 14..8 -> redraw every 10^6 .. 10^0 moves with no delay;
// slider  7..0 -> redraw every move with a 1, 2, 4, ... 128 ms delay.
function speedParams() {
  const s = +$('speed').value;
  if (s >= 8) return { every: Math.pow(10, s - 8), delay: 0 };
  return { every: 1, delay: 1 << (7 - s) };
}

function applySpeed() {
  const p = speedParams();
  $('speedval').textContent = p.delay ? `draw every move + ${p.delay}ms`
    : p.every === 1 ? 'draw every move'
    : `draw every ${p.every.toLocaleString()} moves`;
  // Live-adjustable mid-run (heap writes, not calls)
  Module.HEAP32[ptr.drawEvery >> 2] = p.every;
  Module.HEAP32[ptr.slowMs >> 2] = p.delay;
}

// --- Tile numbers (idle-only overlay) ---
// The number on a tile is its home index + 1 (row-major), read from the
// engine's scrambler map. Drawn only while nothing is running, so calling
// exports is safe (Asyncify forbids reentry mid-run), and only when tiles
// render large enough to be readable.
const overlay = $('overlay');
const octx = overlay.getContext('2d');

function drawNumbers() {
  const w = canvas.width, h = canvas.height;
  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  overlay.width = Math.round(rect.width * dpr);
  overlay.height = Math.round(rect.height * dpr);
  octx.clearRect(0, 0, overlay.width, overlay.height);
  if (!w || w * h > 4096) return;
  // Mid-run, show numbers only at watchable speeds (every move or slower).
  if (running && speedParams().every !== 1) return;
  if (!Module._eng_have_image()) return;
  // object-fit: contain letterboxes the image inside the element — map the
  // tile grid onto the actual content rectangle, not the element box.
  const scale = Math.min(rect.width / w, rect.height / h);
  if (scale < 18) return; // tiles too small to read
  const tile = scale * dpr;
  const offX = (overlay.width - w * tile) / 2;
  const offY = (overlay.height - h * tile) / 2;
  const hole = w * h - 1;
  octx.font = `${Math.round(tile * 0.38)}px system-ui, sans-serif`;
  octx.textAlign = 'center';
  octx.textBaseline = 'middle';
  octx.lineJoin = 'round'; // miter joins spike on tight glyph corners (the "2" starburst)
  octx.lineWidth = Math.max(1.5, tile * 0.05);
  octx.strokeStyle = 'rgba(0,0,0,.8)';
  octx.fillStyle = '#fff';
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) {
      const idx = Module._eng_home_index(x, y);
      if (idx === hole) continue;
      const label = String(idx + 1);
      octx.strokeText(label, offX + (x + 0.5) * tile, offY + (y + 0.5) * tile);
      octx.fillText(label, offX + (x + 0.5) * tile, offY + (y + 0.5) * tile);
    }
}

window.addEventListener('resize', drawNumbers);

function setRunning(on) {
  running = on;
  const scrambling = on && currentOp === 'eng_scramble';
  for (const id of ['solve', 'flip', 'stupid', 'open', 'save', 'preset'])
    $(id).disabled = on;
  // The scramble button stays live during its own run: it's the off switch.
  $('scramble').disabled = on && !scrambling;
  $('scramble').textContent = scrambling ? 'Stop Scrambling' : 'Scramble!';
  $('stop').disabled = !on;
  // Mode stays live during a scramble so the walk can be steered mid-run.
  for (const b of document.querySelectorAll('#mode button')) b.disabled = on && !scrambling;
  $('res').disabled = on || !source;
  if (on) octx.clearRect(0, 0, overlay.width, overlay.height);
  else { paint(); updateProgress(); $('status').textContent = ''; drawNumbers(); }
}

async function run(name, ...args) {
  if (!Module._eng_have_image()) { log('Open an image first.'); return; }
  const p = speedParams();
  Module._eng_set_draw(1, p.every, 0, p.delay);
  currentOp = name;
  setRunning(true);
  try {
    await Module.ccall(name, null, ['number', 'number', 'number'], args, { async: true });
  } finally {
    currentOp = null;
    setRunning(false);
  }
}

// Scramble mode segmented control. The 2008 flags map exactly: Swirl is the
// rotating direction bias, and "Toggle Direction" reversed the rotation —
// tracing the suggestion cycle gives clockwise vs counter-clockwise.
let scrambleMode = 'cw';
for (const b of document.querySelectorAll('#mode button'))
  b.onclick = () => {
    scrambleMode = b.dataset.mode;
    for (const o of document.querySelectorAll('#mode button'))
      o.classList.toggle('active', o === b);
    // Heap writes so the mode can change mid-scramble (the 2008 loop reads
    // these flags every iteration; exports can't be called while suspended).
    // The hole walks the rotation; the visible smear flows the opposite
    // way. The buttons label what you see, so cw here sets the 2008
    // "direction" flag (the hole's ccw).
    Module.HEAPU8[ptr.swirl] = scrambleMode === 'random' ? 0 : 1;
    Module.HEAPU8[ptr.direction] = scrambleMode === 'cw' ? 1 : 0;
  };

// Scramble runs "forever" (1e15 moves ≈ years) until stopped — the button
// toggles, and the 2008 engine's own m_bStop checks are the off switch.
$('scramble').onclick = () => {
  if (running) {
    if (currentOp === 'eng_scramble') Module.HEAPU8[ptr.stop] = 1;
    return;
  }
  run('eng_scramble', 1e15, scrambleMode === 'random' ? 0 : 1, scrambleMode === 'cw' ? 1 : 0);
};
$('solve').onclick = () => run('eng_solve');
$('flip').onclick = () => run('eng_flip_solve');
$('stupid').onclick = () => run('eng_stupid_solve');
$('stop').onclick = () => { Module.HEAPU8[ptr.stop] = 1; };

// Confetti burst for solving the puzzle by hand. Self-contained: a fixed
// full-viewport canvas that removes itself when the last particle settles.
function confetti() {
  const c = document.createElement('canvas');
  c.style.cssText = 'position:fixed;inset:0;pointer-events:none;z-index:99';
  c.width = innerWidth;
  c.height = innerHeight;
  document.body.appendChild(c);
  const cx = c.getContext('2d');
  const colors = ['#e628ff', '#59f', '#ffd24d', '#5be05b', '#ff5b5b', '#fff'];
  const parts = Array.from({ length: 160 }, () => ({
    x: c.width / 2 + (Math.random() - 0.5) * c.width * 0.4,
    y: c.height * 0.35,
    vx: (Math.random() - 0.5) * 14,
    vy: -Math.random() * 14 - 4,
    r: Math.random() * 5 + 3,
    a: Math.random() * Math.PI,
    va: (Math.random() - 0.5) * 0.4,
    color: colors[(Math.random() * colors.length) | 0],
  }));
  const t0 = performance.now();
  (function tick(t) {
    const dt = Math.min((t - t0) / 1000, 3);
    cx.clearRect(0, 0, c.width, c.height);
    for (const p of parts) {
      p.x += p.vx;
      p.y += p.vy;
      p.vy += 0.45;
      p.a += p.va;
      cx.save();
      cx.translate(p.x, p.y);
      cx.rotate(p.a);
      cx.globalAlpha = Math.max(0, 1 - dt / 2.8);
      cx.fillStyle = p.color;
      cx.fillRect(-p.r, -p.r / 2, p.r * 2, p.r);
      cx.restore();
    }
    if (dt < 2.8) requestAnimationFrame(tick);
    else c.remove();
  })(t0);
}

// Manual hole moves. Engine direction codes: 0 => y-1 (screen up),
// 1 => y+1 (down), 2 => x-1 (left), 3 => x+1 (right).
function moveHole(dir) {
  if (running || !Module._eng_have_image()) return;
  const wasSolved = Module._eng_is_solved();
  Module._eng_move_hole(dir);
  paint();
  drawNumbers();
  if (!wasSolved && Module._eng_is_solved()) {
    confetti();
    log('Solved by hand!');
    log(' ');
  }
}

// Arrow keys move the hole while the image box is focused (blue outline).
$('canvasbox').addEventListener('keydown', (e) => {
  const dirs = { ArrowUp: 0, ArrowDown: 1, ArrowLeft: 2, ArrowRight: 3 };
  if (!(e.key in dirs)) return;
  e.preventDefault();
  moveHole(dirs[e.key]);
});

$('speed').oninput = applySpeed;
applySpeed();

// Auto-speed on open/preset/resize: one speed step per half-octave of board
// size — 4px short side lands on the slowest stop (128 ms/move), 512px and
// up on the fastest (redraw every 1M moves).
function autoSpeed(w, h) {
  const s = Math.round(2 * (Math.log2(Math.min(w, h)) - 2));
  $('speed').value = Math.max(0, Math.min(14, s));
  applySpeed();
}

function showCanvas(w, h) {
  canvas.width = w;
  canvas.height = h;
  $('canvaswrap').hidden = false;
  $('placeholder').hidden = true;
  autoSpeed(w, h);
  paint();
  $('canvasbox').focus({ preventScroll: true });
  drawNumbers();
}

function loadIntoEngine(rgba, w, h) {
  const buf = Module._malloc(rgba.length);
  Module.HEAPU8.set(rgba, buf);
  Module._eng_load_rgba(buf, w, h);
  Module._free(buf);
}

// --- Resolution / decimation ---

// Log-scale slider: 0 => short side 4px, 100 => native resolution.
function dimsForSlider(t) {
  const S = Math.min(source.w, source.h);
  if (S <= 4 || t >= 100) return { w: source.w, h: source.h, native: true };
  const short = Math.min(S, Math.round(4 * Math.pow(S / 4, t / 100)));
  if (short >= S) return { w: source.w, h: source.h, native: true };
  const scale = short / S;
  return source.w <= source.h
    ? { w: short, h: Math.round(source.h * scale), native: false }
    : { w: Math.round(source.w * scale), h: short, native: false };
}

// Downsample by repeated halving (better area averaging than one big jump).
async function downsample(rgba, sw, sh, tw, th) {
  let bmp = await createImageBitmap(new ImageData(rgba, sw, sh));
  let cw = sw, ch = sh;
  while (cw / 2 >= tw && ch / 2 >= th) {
    cw = Math.max(tw, Math.round(cw / 2));
    ch = Math.max(th, Math.round(ch / 2));
    const off = new OffscreenCanvas(cw, ch);
    const octx = off.getContext('2d');
    octx.imageSmoothingEnabled = true;
    octx.imageSmoothingQuality = 'high';
    octx.drawImage(bmp, 0, 0, cw, ch);
    bmp = off.transferToImageBitmap();
  }
  const off = new OffscreenCanvas(tw, th);
  const octx = off.getContext('2d');
  octx.imageSmoothingEnabled = true;
  octx.imageSmoothingQuality = 'high';
  octx.drawImage(bmp, 0, 0, tw, th);
  return octx.getImageData(0, 0, tw, th).data;
}

async function applyResolution() {
  if (!source || running) return;
  const d = dimsForSlider(+$('res').value);
  const rgba = d.native ? source.rgba : await downsample(source.rgba, source.w, source.h, d.w, d.h);
  loadIntoEngine(rgba, d.w, d.h);
  showCanvas(d.w, d.h);
  log(d.native ? `Restored full resolution (${d.w}x${d.h})` : `Decimated to ${d.w}x${d.h} (${d.w * d.h - 1} tiles + 1 hole)`);
  log(' ');
}

function updateResReadout() {
  if (!source) return;
  const d = dimsForSlider(+$('res').value);
  $('resval').textContent = `${d.w}×${d.h}`;
}

$('res').oninput = updateResReadout;
$('res').onchange = applyResolution;

function setSource(rgba, w, h) {
  source = { rgba, w, h };
  $('res').value = 100;
  $('res').disabled = false;
  updateResReadout();
}

async function openFile(file) {
  if (running) return;
  let rgba, w, h;
  if (file.name.toLowerCase().endsWith('.ppm')) {
    const bytes = new Uint8Array(await file.arrayBuffer());
    Module.FS.writeFile('/in.ppm', bytes);
    const path = Module.stringToNewUTF8('/in.ppm');
    const ok = Module._eng_load_ppm(path);
    Module._free(path);
    if (!ok) { log(`Unable To Open File: ${file.name}`); return; }
    // Snapshot the loaded PPM back out of the engine as the full-res source.
    w = Module._eng_width();
    h = Module._eng_height();
    rgba = new Uint8ClampedArray(w * h * 4);
    let src = Module._eng_pixels();
    const rgb = Module.HEAPU8;
    for (let i = 0, o = 0; i < w * h; i++, o += 4) {
      rgba[o] = rgb[src++];
      rgba[o + 1] = rgb[src++];
      rgba[o + 2] = rgb[src++];
      rgba[o + 3] = 255;
    }
  } else {
    const bmp = await createImageBitmap(file);
    const off = new OffscreenCanvas(bmp.width, bmp.height);
    const octx = off.getContext('2d');
    octx.drawImage(bmp, 0, 0);
    rgba = octx.getImageData(0, 0, bmp.width, bmp.height).data;
    w = bmp.width;
    h = bmp.height;
  }

  // Cap huge inputs: the working copy never exceeds 2048 on the long side.
  if (Math.max(w, h) > 2048) {
    const scale = 2048 / Math.max(w, h);
    const nw = Math.round(w * scale), nh = Math.round(h * scale);
    rgba = await downsample(rgba, w, h, nw, nh);
    log(`Reduced ${w}x${h} image to ${nw}x${nh} (2048 max)`);
    w = nw;
    h = nh;
  }

  setSource(rgba, w, h);
  log(`Opened File: ${file.name}`);

  // Big boards open at ~512 (nearest Resize stop) — slide right for full size.
  if (Math.min(w, h) > 512) {
    $('res').value = Math.round(100 * Math.log(512 / 4) / Math.log(Math.min(w, h) / 4));
    updateResReadout();
    await applyResolution();
  } else {
    loadIntoEngine(rgba, w, h);
    showCanvas(w, h);
  }
}

$('open').onclick = () => $('file').click();
$('file').onchange = () => { if ($('file').files[0]) openFile($('file').files[0]); };

async function openUrl(src) {
  const blob = await (await fetch(src)).blob();
  await openFile(new File([blob], src.split('/').pop()));
}

$('preset').onchange = () => {
  const src = $('preset').value;
  $('preset').selectedIndex = 0; // reset so the same preset can be re-picked
  if (src && !running) openUrl(src);
};

const box = $('canvasbox');
box.ondragover = (e) => { e.preventDefault(); box.classList.add('drag'); };
box.ondragleave = () => box.classList.remove('drag');
box.ondrop = (e) => {
  e.preventDefault();
  box.classList.remove('drag');
  if (e.dataTransfer.files[0]) openFile(e.dataTransfer.files[0]);
};

$('save').onclick = () => {
  if (!Module._eng_have_image()) return;
  canvas.toBlob((blob) => download(blob, 'scrambled.png'), 'image/png');
};

function download(blob, name) {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = name;
  a.click();
  URL.revokeObjectURL(a.href);
}

log('kbImageScramble ready.');
log('Engine: original 2008 C++ via WebAssembly.');
log(' ');

// ?demo — load the bundled 2008 test image (kevin.ppm) and scramble it.
// ?img — load the gradient board instead.
// &res=N — resize whatever loaded to N pixels on the short side.
const params = new URLSearchParams(location.search);
if (params.has('img')) await openUrl('gradients/grad-1024.png');
else if (params.has('demo')) await openUrl('kevin.ppm');
if (source && params.has('res')) {
  const short = Math.max(4, Math.min(Math.min(source.w, source.h), +params.get('res') || 4));
  const S = Math.min(source.w, source.h);
  $('res').value = Math.round(100 * Math.log(short / 4) / Math.log(S / 4)) || 0;
  updateResReadout();
  await applyResolution();
}
if (params.has('demo')) $('scramble').click();
