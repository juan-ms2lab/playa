# Performance V1: Optimist audit (pinned e6d9f5d)

This maps JTS22 `spec.md` onto the code. Paths are under `firmware/src/` unless they start with another top-level directory. The firmware is one compilation unit (`felucca.c`) built with `FELUCCA_*` flags.

| Spec area | Existing code | Fit |
|---|---|---|
| Audio callback | `fm1_alnk0_irq` (`core/audio.c:208`) → `mix_block` (`fx/fx.c:3360`): `events_block` → drums → `mix_part` ×3 → `mix_finish`. Half buffer 256 frames, blocks of 32. Heap-free. | reuse |
| Tracks | `track_t trk[NTRK]` (`core/core.h:848`): NTRK 4, NPART 3 synth, TRK_DRUM 3. `song.sel` is the selected track. NTRK=4 is baked into the formats. | **Layer A = part 0, Layer B = part 1, song melodic = part 2, drums = track 3**. No 5th track. |
| Engines | `engine_t` (`core.h:730`), `ENGINE_LIST` (`core/registry.h:146`). FM6 UID 9, PROPHET UID 24 (off by default), ANALOG UID 0. Note API: `trk_note_on/off`, `trk_all_off` (`core/voice.c:926/1031/1077`). | reuse; the profile turns PROPHET on |
| Voices | `voice_alloc` (`voice.c:672`), `voice_room/victim` against `CG_VBUDGET`, `shed_voice` (:579). Voices are owned per track (VOICE_OWN). **8 voices in total across all parts.** CPU guard: `system/cpuguard.c`. | extend with live/song priority |
| MIDI in | USB/TRS/BLE/Wi-Fi → `midi_in_enqueue` (`io/midi/midi_rings.c:46`) → `events_block` → `midi_event` (`io/midi/midi_control.c:287`). `midi_track_idx(ch)` returns the first match only. `midi_held_t` stores one track per press. | **change:** ch1 fans out to parts 0+1 through split/blend; held entries become per-track |
| Split/blend | none | new |
| Drums | `drums/drums.c`: 10 lanes, GM `INST_OF_GM[35..81]`, `lane_of_note` (:199), `drum_on`. Drum channel set by `G_DRCH`. | reuse; the profile sets drums to ch2 |
| FX | Sends `P_CHOR/P_DLY/P_REV` per track; bipolar per-track filter `P_TFLT` (`FELUCCA_TRK_FILT`). Buses in `fx_buses` (`fx.c:1751`). | reuse; one new knob set REV/CHO/DEL/FIL |
| UI | Buttons FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+; encoders SELECT ALGORITHM PRESETS K1–K4 (`ui/panel.c`). Screens in `ui/optimist/op_state.c`, pages in `params.c PAGES`. The HOME mixer (`op_mixer.c`) is the closest existing screen. | new Perform screen |
| Persistence | `project_t` FUNB fmt 11 with FNV-1a sum (`storage/project.c`). Section log `storage/sections/sec_log.c` (u8 ids; 142–225 reserved for MIDI tracks). Snapshots. User presets in UPB4. | new sec_log record |
| SMF | none. SD/FatFs present (`storage/sd`). | new parser; no SD dependency in V1 |
| Simulator | `firmware/sim/sim.c`; `python3 tools/sim.py build --wasm` (emcc); `make web SIM=1`; MIDI through `sim_midi_buf/sim_midi_in` (as USB); `node tests/sim_test.mjs`. | gate for every prompt |
| Host tests | `make test` needs the JieLi toolchain (`make setup`). One test without it: `python3 tools/build.py --host-headers`, then `cc -O2 -w -Ibuild/gen-host -Ifirmware/src -o build/host/X tests/X_test.c -lm`. | |
| Builder item | CONTRIBUTING.md:146: `tools/builder/registry.py` `_add`, `core.py` rules, BUILDER.md, `export_builder_web.py`, `size_fns.py`, `run_tests.sh`, `costs.json`. | add `PERF` |

## Risks
- **Voices:** 8 are shared by layers A and B and the song. FM6 + PROPHET polyphony must be measured; the allocator's priority order matters.
- **Flash:** the default profile is full. `performance-v1` must drop engines (LAYOUT 1 leaves 143–188 KB).
- **Song storage:** internal flash is tight. V1 uses one built-in demo song or MIDI streamed in (spec §2.4 gate).
