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
  for (const id of ['scramble', 'solve', 'flip', 'stupid', 'open', 'saveppm', 'savepng'])
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

$('drawevery').onchange = applyDisplaySettings;
$('delay').oninput = () => { $('delayval').textContent = `${$('delay').value}ms`; applyDisplaySettings(); };
$('stretch').onchange = () => canvas.classList.toggle('stretch', $('stretch').checked);

function showCanvas(w, h) {
  canvas.width = w;
  canvas.height = h;
  canvas.hidden = false;
  $('placeholder').hidden = true;
  paint();
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
  } else {
    const bmp = await createImageBitmap(file);
    const off = new OffscreenCanvas(bmp.width, bmp.height);
    const octx = off.getContext('2d');
    octx.drawImage(bmp, 0, 0);
    const rgba = octx.getImageData(0, 0, bmp.width, bmp.height).data;
    const buf = Module._malloc(rgba.length);
    Module.HEAPU8.set(rgba, buf);
    Module._eng_load_rgba(buf, bmp.width, bmp.height);
    Module._free(buf);
  }
  log(`Opened File: ${file.name}`);
  showCanvas(Module._eng_width(), Module._eng_height());
}

$('open').onclick = () => $('file').click();
$('file').onchange = () => { if ($('file').files[0]) openFile($('file').files[0]); };

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
if (new URLSearchParams(location.search).has('demo')) {
  const blob = await (await fetch('kevin.ppm')).blob();
  await openFile(new File([blob], 'kevin.ppm'));
  $('scramble').click();
}
