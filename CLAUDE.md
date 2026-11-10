# CLAUDE.md — PLAYA (JTS22)

PLAYA is a performance firmware for the M-VAVE FM-1, built on Optimist (upstream `w0ts/optimist` at `e6d9f5d`).
Repo: `github.com/juan-ms2lab/playa` (a fork of Optimist), branch `playa`. Created by JFS with help from Claude.
Spec: `docs/JTS22-SPEC.md`. Manual: `docs/PLAYA-MANUAL.md`. Optimist's own docs (BUILDING.md, CONTRIBUTING.md) apply.

## Ground rules
- **Simulator first, hardware second.** Every change: host tests → WASM simulator (screenshots for UI) → only then
  package and flash. Flash only when the user asks or has approved flashing for the current work.
- Before flashing, check the device: `tools/builder/venv/bin/python tools/fm1_install.py --info`. It must report
  `FM-1_780` (PLAYA) or `FM-1_700` (an older PLAYA / Optimist dev). Another identity = another firmware: ask first.
- Everything PLAYA adds is behind the builder item `PERF` (`FELUCCA_PERF`); stock Optimist builds must stay unchanged.
  Prefer builder flags and hiding over deleting upstream code, so upstream fixes still merge.
- Match Optimist's code and comment style (terse, lowercase, the user's requests quoted with their date).
  New files: SPDX `GPL-3.0-only` header plus the PLAYA credit line (see `firmware/src/io/midi/perf.c`).
- Ported code keeps its source and licence in the file header and in `LICENSING.md`.

## Build, test, flash (classic build; `fm1base` is private and unavailable)
```
make setup                         # if downloads fail SSL: SSL_CERT_FILE=$(tools/builder/venv/bin/python -m certifi)
cc -O2 -w -Ibuild/gen-host -Ifirmware/src -DFELUCCA_SECTIONS=4 -DFELUCCA_PERF=1 -DFELUCCA_MIDI_EXPR=1 \
   -o build/host/perf_test tests/perf_test.c -lm && build/host/perf_test          # (python3 tools/build.py --host-headers first)
python3 tools/sim.py build --wasm --config config/profiles/performance-v1.config --out build/sim/perf.wasm
node tests/sim_perf_test.mjs
sh tests/run_tests.sh              # Optimist's suite; fm1base tests skip; insend_test / auto cpu fail upstream too
python3 tools/div_audit.py         # every divide by a variable needs a reason in tools/div_audit.txt
python3 tools/optimist.py package --classic --profile performance-v1 --out firmwares
python3 tools/optimist.py flash --classic --port <port name> --yes firmwares/<file>.fwsc   # macOS may show an old port name
```
- The `PERF` build's identity is `FM-1_780`, version "PLAYA 0.1" (`tools/build.py` PLAYA_PRODUCT / PLAYA_VERSION).
- `tests/builder_test.py` cost checks fail: `measure_costs.py` needs `fm1base` (known, documented in README).
- Regenerating the UI fonts needs Raqm: `brew install libraqm`, then run `tools/gen_aa_font.py` with
  `DYLD_LIBRARY_PATH=/opt/homebrew/lib` (command in `firmware/src/ui/optimist/perf_aa.c`).

## Where things are
| What | Where |
|---|---|
| Layers, split / blend / single, CC knobs, the split's storage (FX record TLV 0x4F) | `firmware/src/io/midi/perf.c`, `midi_control.c` |
| Per-voice gain, voice budget (12), PROPHET 8 voices | `core/core.h` (VBUDGET, voice_t.xg), `core/voice.c`, `engines/prophet/eng_prophet.c` |
| Pages, buttons, encoders (one page a button) | `ui/optimist/op_perf.c` (pf_tap, pf_enc, PG_*) |
| Drawing (PERFORM, MIX, pages, ENV/LFO graphs, SAVE, SEQ) | `ui/optimist/op_perfdraw.c` |
| Text / pill renderer, fonts, themes | `ui/optimist/perf_aa.c`, `perf_font.h` (generated), PF_THEMES |
| Popups in the new style | `op_preset.c` pf_pop_paint, `op_graph.c` pf_modal_paint / pf_toast_paint |
| Boot screen | `ui/splash.c` playa_paint |
| Profile | `config/profiles/performance-v1.config` |

## Testing with a controller
An Akai MPK mini 3 on the Mac reaches the FM-1 through a small mido bridge (MPK in → FM-1 out, everything passed
through): keys ch 1 → layers, pads ch 10 → drums, K1–K7 = CC 70–76 → knobs 1–4, SELECT, PRESETS, ALGORITHM.

## Next (v1.5 and later)
- Looper (REC, PLAY), replacing the removed step sequencer for the synth tracks.
- MIDI file player (deprecated in V1; track 3 is kept for it).
- Measured polyphony and CPU on hardware; a two-core PROPHET split as Melodee's.
- The Firmware Hub submission (needs the user's go-ahead).
