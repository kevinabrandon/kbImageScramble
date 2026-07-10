# kbImageScramble

A 15-puzzle where every tile is a pixel of a photograph — scramble it with hundreds of
millions of random moves, then watch it solve itself back one pixel at a time.

**▶ Try it: https://kevinabrandon.github.io/kbImageScramble/**
(or [jump straight into a demo](https://kevinabrandon.github.io/kbImageScramble/?demo))

Drop in any image and scramble it into noise with up to a billion random tile moves, then
hit **Solve!!** and watch the hole run through the image, escorting every pixel back home
row by row until the picture reassembles itself. **Flip!!** solves to the upside-down
image instead; **Swirl** scrambles with a drifting bias so the photo smears rather than
dissolves; **Stuipd Solve** *[sic]* solves greedily and visibly fails forever, on purpose.
**ReDraw**/**Slow** control how much of the action you watch live.

## Running / building

The app is static files in `web/public/` — serve them any way you like:

```
cd web/public && python3 -m http.server 8000
```

The engine is the original January 2008 C++, compiled unmodified to WebAssembly. To
rebuild it after touching `web/wasm/`, install [emsdk](https://emscripten.org) and:

```
./web/wasm/build.sh
node web/test/smoke.mjs   # scrambles + solves a test image, asserts pixel-perfect restore
```

## History

Born from a brother-to-brother coding challenge in January 2008: who could first
auto-solve a 15-puzzle — and then, what if instead of 15 tiles it was every pixel of a
photograph? The algorithm was worked out by solving a physical 15-puzzle by hand and
writing down the steps. The original 2008 sources are preserved offline and may be
published here someday.
