// SPDX-License-Identifier: GPL-3.0-only
// JTS22 PLAYA, a performance build for the M-VAVE FM-1: created by JFS with help from Claude (Anthropic), 2026.
// Built on Optimist by w0ts and contributors (github.com/w0ts/optimist), GPL-3.0-only; this file is part of that work
// and under the same licence (LICENSE).
// PERF (JTS22) in the browser simulator's WebAssembly, under node: a performance-v1 build (tools/sim.py build --wasm
// --config config/profiles/performance-v1.config) sounds from channel 1 on the layers (inside the blend, below it, above
// it), from channel 10 on the drums, holds under the pedal, and is silent again after the note offs: no stuck note.
//   node tests/sim_perf_test.mjs [build/sim/perf.wasm]
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

const WASM = process.argv[2] || fileURLToPath(new URL('../build/sim/perf.wasm', import.meta.url));
const module = new WebAssembly.Module(readFileSync(WASM));
let fails = 0;
const check = (ok, what) => { console.log(`${ok ? 'ok  ' : 'FAIL'} ${what}`); if (!ok) fails++; };

const x = new WebAssembly.Instance(module, {}).exports;
if (x._initialize) x._initialize();
const mem = () => x.memory.buffer;
let pcm = [];
const take = () => { const n = x.sim_audio_take(); pcm.push(new Int16Array(mem(), x.sim_audio(), 2 * n).slice()); };
const run = (sec) => { const frames = Math.round(sec * 44100); for (let f = 0; f < frames; f += 1024) { x.sim_run(Math.min(1024, frames - f)); take(); } };
const midi = (...b) => { new Uint8Array(mem(), x.sim_midi_buf(), b.length).set(b); x.sim_midi_in(b.length); };
const rms = (sec) => { pcm = []; run(sec); let s = 0, n = 0; for (const c of pcm) for (const v of c) { s += v * v; n++; } return Math.sqrt(s / Math.max(n, 1)); };
const db = (r) => (r > 0 ? 20 * Math.log10(r / 32768) : -Infinity).toFixed(1);

x.sim_boot();
run(2);                                              // (boot, the default project)
const floor = rms(0.5);
check(floor < 20, `silent at rest (${db(floor)} dBFS)`);
for (const [note, what] of [[60, 'inside the blend: both layers'], [40, 'below the window: A'], [80, 'above the window: B']]) {
  midi(0x90, note, 100);
  const on = rms(0.4);
  midi(0x80, note, 0);
  run(3);
  const after = rms(0.5);
  check(on > 10 * Math.max(floor, 1) && after < 20, `ch 1 note ${note}, ${what}: sounds (${db(on)} dBFS), then silent (${db(after)})`);
}
midi(0x99, 36, 110);                                 // a kick on channel 10
const kick = rms(0.3);
run(2);
check(kick > 10 * Math.max(floor, 1) && rms(0.5) < 20, `ch 10: the drums sound (${db(kick)} dBFS), then silent`);
midi(0xB0, 64, 127, 0x90, 64, 100, 0x80, 64, 0);     // pedal down, a note played and released
run(0.2);
const held = rms(0.4);
midi(0xB0, 64, 0);
run(3);
const freed = rms(0.5);
check(held > 10 * Math.max(floor, 1) && freed < 20, `the pedal holds the note (${db(held)} dBFS); pedal up: silent (${db(freed)})`);
const screen = () => { const p = new Uint16Array(mem(), x.sim_pixels(), 240 * 240); let h = 0; for (let i = 0; i < p.length; i++) h = (h * 31 + p[i]) >>> 0; return h; };
run(1);
const idle1 = screen(); run(0.5); const idle2 = screen();
midi(0xB0, 74, 0); for (let v = 4; v <= 40; v += 4) { midi(0xB0, 74, v); run(0.05); }
run(0.5);
check(idle1 === idle2 && screen() !== idle2, 'CC 74 (an MPK knob) turns SELECT: the screen changes (still before)');
console.log(fails ? `sim_perf: ${fails} failed` : 'sim_perf: all passed');
process.exit(fails ? 1 : 0);
