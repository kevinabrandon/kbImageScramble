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
  onDraw: () => { paint(); updateProgress(); },
});

// Addresses cached once; read/written via heap views because exports must not
// be called while the engine is suspended mid-operation (Asyncify).
const ptr = {
  stop: Module._eng_stop_ptr(),
  count: Module._eng_count_ptr(),
  solved: Module._eng_pixels_solved_ptr(),
  drawEvery: Module._eng_draw_every_ptr(),
  slowMs: Module._eng_slow_ms_ptr(),
};

let running = false;

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
  const count = Module.HEAPU32[ptr.count >> 2];
  $('progress').style.width = total ? `${(100 * solved / total).toFixed(1)}%` : '0%';
  $('status').textContent = running ? `moves: ${count.toLocaleString()}` : '';
}

function applyDisplaySettings() {
  // Live-adjustable mid-run (heap writes, not calls)
  Module.HEAP32[ptr.drawEvery >> 2] = Math.max(1, +$('drawevery').value || 1);
  Module.HEAP32[ptr.slowMs >> 2] = +$('delay').value;
}

function setRunning(on) {
  running = on;
  for (const id of ['scramble', 'solve', 'flip', 'stupid', 'open', 'saveppm', 'savepng',
    'mup', 'mdown', 'mleft', 'mright'])
    $(id).disabled = on;
  $('stop').disabled = !on;
  if (!on) { paint(); updateProgress(); $('status').textContent = ''; }
}

async function run(name, ...args) {
  if (!Module._eng_have_image()) { log('Open an image first.'); return; }
  // ReDraw off means no yields: the browser freezes until done, like the 2008
  // program's worker thread pegging a core. Warn once in the log.
  if (!$('redraw').checked) log(`${name} running with ReDraw off; page may be unresponsive until done...`);
  Module._eng_set_draw($('redraw').checked ? 1 : 0, Math.max(1, +$('drawevery').value || 1),
    $('slow').checked ? 1 : 0, +$('delay').value);
  setRunning(true);
  try {
    await Module.ccall(name, null, ['number', 'number', 'number'], args, { async: true });
  } finally {
    setRunning(false);
  }
}

$('scramble').onclick = () => {
  const n = Math.min(1000000000, Math.max(1, Math.floor(+$('n').value || 1)));
  run('eng_scramble', n, $('swirl').checked ? 1 : 0, $('direction').checked ? 1 : 0);
};
$('solve').onclick = () => run('eng_solve');
$('flip').onclick = () => run('eng_flip_solve');
$('stupid').onclick = () => run('eng_stupid_solve');
$('stop').onclick = () => { Module.HEAPU8[ptr.stop] = 1; };

// Manual hole moves. Engine direction codes: 0 => y-1 (screen up),
// 1 => y+1 (down), 2 => x-1 (left), 3 => x+1 (right).
function moveHole(dir) {
  if (running || !Module._eng_have_image()) return;
  Module._eng_move_hole(dir);
  paint();
  // Hand focus to the image so arrow keys keep working after a d-pad click
  $('canvasbox').focus({ preventScroll: true });
}
$('mup').onclick = () => moveHole(0);
$('mdown').onclick = () => moveHole(1);
$('mleft').onclick = () => moveHole(2);
$('mright').onclick = () => moveHole(3);

// Arrow keys move the hole while the image box is focused (blue outline).
$('canvasbox').addEventListener('keydown', (e) => {
  const dirs = { ArrowUp: 0, ArrowDown: 1, ArrowLeft: 2, ArrowRight: 3 };
  if (!(e.key in dirs)) return;
  e.preventDefault();
  moveHole(dirs[e.key]);
});

$('drawevery').onchange = applyDisplaySettings;
$('delay').oninput = () => { $('delayval').textContent = `${$('delay').value}ms`; applyDisplaySettings(); };

function showCanvas(w, h) {
  canvas.width = w;
  canvas.height = h;
  canvas.hidden = false;
  $('placeholder').hidden = true;
  paint();
  $('canvasbox').focus({ preventScroll: true });
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
  if (file.name.toLowerCase().endsWith('.ppm')) {
    const bytes = new Uint8Array(await file.arrayBuffer());
    Module.FS.writeFile('/in.ppm', bytes);
    const path = Module.stringToNewUTF8('/in.ppm');
    const ok = Module._eng_load_ppm(path);
    Module._free(path);
    if (!ok) { log(`Unable To Open File: ${file.name}`); return; }
    // Snapshot the loaded PPM back out of the engine as the full-res source.
    const w = Module._eng_width(), h = Module._eng_height();
    const rgba = new Uint8ClampedArray(w * h * 4);
    let src = Module._eng_pixels();
    const rgb = Module.HEAPU8;
    for (let i = 0, o = 0; i < w * h; i++, o += 4) {
      rgba[o] = rgb[src++];
      rgba[o + 1] = rgb[src++];
      rgba[o + 2] = rgb[src++];
      rgba[o + 3] = 255;
    }
    setSource(rgba, w, h);
  } else {
    const bmp = await createImageBitmap(file);
    const off = new OffscreenCanvas(bmp.width, bmp.height);
    const octx = off.getContext('2d');
    octx.drawImage(bmp, 0, 0);
    const rgba = octx.getImageData(0, 0, bmp.width, bmp.height).data;
    setSource(rgba, bmp.width, bmp.height);
    loadIntoEngine(rgba, bmp.width, bmp.height);
  }
  log(`Opened File: ${file.name}`);
  showCanvas(Module._eng_width(), Module._eng_height());
}

$('open').onclick = () => $('file').click();
$('file').onchange = () => { if ($('file').files[0]) openFile($('file').files[0]); };

async function openUrl(src) {
  const blob = await (await fetch(src)).blob();
  await openFile(new File([blob], src.split('/').pop()));
}

for (const b of document.querySelectorAll('.testimg'))
  b.onclick = () => { if (!running) openUrl(b.dataset.src); };

const box = $('canvasbox');
box.ondragover = (e) => { e.preventDefault(); box.classList.add('drag'); };
box.ondragleave = () => box.classList.remove('drag');
box.ondrop = (e) => {
  e.preventDefault();
  box.classList.remove('drag');
  if (e.dataTransfer.files[0]) openFile(e.dataTransfer.files[0]);
};

$('saveppm').onclick = () => {
  if (!Module._eng_have_image()) return;
  const path = Module.stringToNewUTF8('/out.ppm');
  Module._eng_save_ppm(path);
  Module._free(path);
  const bytes = Module.FS.readFile('/out.ppm');
  download(new Blob([bytes]), 'scrambled.ppm');
};
$('savepng').onclick = () => {
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
// &res=N — decimate to N pixels on the short side (e.g. ?demo&res=8).
// ?img=N — load the NxN gradient test image (e.g. ?img=64), no scramble.
const params = new URLSearchParams(location.search);
if (params.has('img')) {
  const n = +params.get('img');
  if ([4, 8, 16, 32, 64, 128, 256, 512, 1024].includes(n)) await openUrl(`gradients/grad-${n}.png`);
}
if (params.has('demo')) {
  const blob = await (await fetch('kevin.ppm')).blob();
  await openFile(new File([blob], 'kevin.ppm'));
  if (params.has('res')) {
    const short = Math.max(4, Math.min(Math.min(source.w, source.h), +params.get('res') || 4));
    const S = Math.min(source.w, source.h);
    $('res').value = Math.round(100 * Math.log(short / 4) / Math.log(S / 4)) || 0;
    updateResReadout();
    await applyResolution();
  }
  $('scramble').click();
}
