// Headless smoke test of the WASM engine: scramble a synthetic image, run
// each solver, and verify the pixels really come home. Run: node test/smoke.mjs
import createEngine from '../public/engine.js';

const Module = await createEngine({ onLog: (m) => m.trim() && console.log('  [engine]', m) });

const W = 40, H = 30;

// Synthetic image: every pixel unique-ish gradient
const rgba = new Uint8Array(W * H * 4);
for (let j = 0; j < H; j++)
  for (let i = 0; i < W; i++) {
    const k = 4 * (i + j * W);
    rgba[k] = (i * 6) & 0xff;
    rgba[k + 1] = (j * 8) & 0xff;
    rgba[k + 2] = (i * 3 + j * 5) & 0xff;
    rgba[k + 3] = 255;
  }

const buf = Module._malloc(rgba.length);
Module.HEAPU8.set(rgba, buf);
Module._eng_set_draw(0, 1000, 0, 0); // ReDraw off => fully synchronous
Module._eng_load_rgba(buf, W, H);
Module._free(buf);

const pixelsPtr = Module._eng_pixels();
const snapshot = () => Module.HEAPU8.slice(pixelsPtr, pixelsPtr + W * H * 3);
const original = snapshot(); // includes the black hole at bottom-right

const diffCount = (a, b) => {
  let n = 0;
  for (let i = 0; i < a.length; i += 3)
    if (a[i] !== b[i] || a[i + 1] !== b[i + 1] || a[i + 2] !== b[i + 2]) n++;
  return n;
};

let failures = 0;
const check = (name, cond, detail) => {
  console.log(`${cond ? 'PASS' : 'FAIL'}: ${name}${detail ? ` (${detail})` : ''}`);
  if (!cond) failures++;
};

// 1. Scramble (classic random walk)
Module._eng_scramble(50000, 0, 0);
const scrambled = snapshot();
check('scramble changes the image', diffCount(original, scrambled) > W * H * 0.5,
  `${diffCount(original, scrambled)}/${W * H} pixels moved`);
check('scramble avg distance > 0', Module._eng_avg_distance() > 1,
  `avg=${Module._eng_avg_distance().toFixed(2)}`);

// 2. Solve — every pixel must walk home
Module._eng_solve();
check('Solve restores the image exactly', diffCount(original, snapshot()) === 0,
  `${diffCount(original, snapshot())} mismatched pixels`);

// 3. Swirl scramble + StupidSolve. Deliberately greedy (hence the 2008
// "Stuipd Solve" button): it re-disturbs placed pixels as it routes later
// ones, purposely demonstrating why the real Solve needs its layered
// strategy. Assert improvement, not restoration.
Module._eng_scramble(30000, 1, 0);
const distBefore = Module._eng_avg_distance();
Module._eng_stupid_solve();
const distAfter = Module._eng_avg_distance();
check('StupidSolve reduces average displacement', distAfter < distBefore,
  `avg ${distBefore.toFixed(2)} -> ${distAfter.toFixed(2)}`);

// 4. FlipSolve — solves the image rotated 180°
Module._eng_scramble(30000, 0, 0);
Module._eng_flip_solve();
const flipped = snapshot();
const rotated = new Uint8Array(W * H * 3);
for (let j = 0; j < H; j++)
  for (let i = 0; i < W; i++) {
    const src = 3 * (i + j * W);
    const dst = 3 * ((W - 1 - i) + (H - 1 - j) * W);
    rotated[dst] = original[src];
    rotated[dst + 1] = original[src + 1];
    rotated[dst + 2] = original[src + 2];
  }
const flipDiff = diffCount(rotated, flipped);
check('FlipSolve yields the 180°-rotated image', flipDiff <= 2, `${flipDiff} mismatched pixels`);

// 5. Manual hole moves: one move changes the image; the opposite move undoes
// it. (After FlipSolve the hole sits top-left, so move right first — moving
// left there is a boundary no-op, which is itself correct behavior.)
const before = snapshot();
Module._eng_move_hole(2); // left (x-1): blocked at boundary
check('manual move into the boundary is a no-op', diffCount(before, snapshot()) === 0);
Module._eng_move_hole(3); // right (x+1)
check('manual move changes the image', diffCount(before, snapshot()) > 0);
Module._eng_move_hole(2); // left (x-1)
check('manual move round-trip restores the image', diffCount(before, snapshot()) === 0);

process.exit(failures ? 1 : 0);
