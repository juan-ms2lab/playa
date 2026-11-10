# FM-1 Performance Firmware — V1.0 Specification

**Status:** Agreed V1 design baseline; hardware feasibility and repository integration pending verification  
**Target:** M-VAVE FM-1  
**Firmware base:** [Optimist](https://github.com/w0ts/optimist)  
**Purpose:** Four-track, performance-first live synthesizer firmware.

## 1. Decisions

| Area | V1 decision |
|---|---|
| Base firmware | Optimist; preserve existing hardware, DSP and packaging infrastructure |
| Track 1 | Synth Layer A, live MIDI channel 1 |
| Track 2 | Synth Layer B, live MIDI channel 1 |
| Track 3 | GM-mapped drums, external MIDI channel 10 (GM standard; changed from 2, 2026-11-10) |
| Track 4 | MIDI song accompaniment service, not a fourth permanently allocated synth |
| Keyboard modes | Full layer/unison, hard split, blended split, single (layer A only) |
| Split | Selectable note, direction and blend width |
| FX knobs | Chorus, delay, reverb, filter for the **selected track** |
| Voices | Adaptive allocation, priority to live keyboard performance |
| GM playback | Restricted accompaniment in V1; no claim of full 16-channel GM synthesis |
| Presets | Save and recall complete four-track performance |

## 2. Product behavior

A MIDI keyboard on channel 1 plays two independently configured synth layers, either together or distributed across the keyboard. Layer A and B may use different engines and patches. The performer selects a track and the four effect controls operate on that track. A controller on channel 10 triggers a GM-mapped drum kit. A Standard MIDI File supplies limited accompaniment through available engines and the drum kit. Under overload, live notes take priority over song notes.

### 2.1 Synth layers (Tracks 1 and 2)

Each layer stores: engine identifier, preset identifier, level, transpose (±24 semitones), tuning, velocity response, mute state, chorus send, delay send, reverb send and filter setting. Sustain, pitch bend, expression and note ownership must be handled independently per layer. Both layers receive external MIDI channel 1.

**Required initial engine targets:** Optimist's Prophet-style engine, FM6/DX7-compatible engine and a lightweight subtractive engine, subject to actual build support and resource measurement. Reuse Optimist engine adapters and preset infrastructure. DX7 compatibility means supported six-operator voice/patch import, not a requirement to run desktop Dexed code unchanged.

### 2.2 Keyboard modes

- **LAYER:** Both engines play across the entire keyboard.
- **HARD:** Layer A plays notes below the split; Layer B plays notes at or above it. Direction can be reversed.
- **SINGLE:** Layer A alone across the whole keyboard; layer B is kept but silent (added 2026-11-10).
- **BLEND:** Both layers overlap around the split with a selectable equal-power crossfade. Preserve incoming MIDI velocity; apply per-voice output gain rather than using velocity as the fade parameter.

Defaults: split note 60 (C4), total blend width 12 semitones, normal direction A-low/B-high. Supported width 0–24 semitones; width 0 behaves as HARD. Split point range MIDI 0–127. Store mode, note, width and direction in the performance.

Reference gain calculation for normal direction, with `t = clamp((note - (split - width/2)) / width, 0, 1)`: `gainA = cos(t*pi/2)` and `gainB = sin(t*pi/2)`. Reverse direction swaps the gains. In LAYER both gains are 1. In HARD, the side of the split selects exactly one layer. Apply smoothing where needed and avoid audio-thread trigonometric calculations by precomputing gains or using suitable DSP primitives.

**Note ownership invariant:** Record which engine voices were started by each note-on. Note-offs must release those exact voices even after changes to split, preset, selected track or transpose. Handle repeated same-pitch notes, sustain, all-notes-off and stale handles.

### 2.3 Drums (Track 3)

External MIDI channel 10 controls the drum track (the GM standard). Standard GM percussion note numbers should map to available kit sounds. MIDI-file channel 10 is routed internally to this drum track. Drum kit, volume, mute and effects sends are independent. Unsupported GM percussion notes are ignored or mapped only through a documented table; no arbitrary substitutions.

### 2.4 MIDI accompaniment (Track 4)

Implement a Standard MIDI File (SMF) Type 0 and Type 1 parser and timestamp-based event scheduler. Required support: variable-length delta times, running status, tempo meta-events, note on/off, program change, channel volume, sustain, pitch bend, End of Track, and safe handling of unknown events. Merge multi-track events by musical timestamp. Provide play, stop, pause, resume, loop, song selection and tempo multiplier. Transposition may affect mapped melodic song channels but never percussion.

V1 maps a configurable subset of melodic channels to the existing engine pool, with a constrained accompaniment voice budget and predictable handling of unsupported GM programs. Full GM sound-bank coverage, 16-part independent synthesis and unrestricted polyphony are explicitly **out of scope**. Song channel 10 routes to Track 3. Keep live input source identity separate from MIDI-file source identity even when MIDI channel numbers match.

**Storage feasibility gate:** Verify internal flash capacity and safe reserved storage before promising on-device song libraries. If insufficient, V1 may ship with one compact demonstration song or require externally streamed MIDI accompaniment. Do not depend on unverified microSD hardware modifications.

## 3. Effects and physical UI

Normal performance knob mapping, always applied to the **selected track**:

| Knob | Parameter | Behavior |
|---|---|---|
| 1 | Reverb | Track-specific send/level to shared reverb |
| 2 | Chorus | Track-specific send/depth to shared chorus |
| 3 | Delay | Track-specific send/level to shared delay |
| 4 | Filter | Bipolar LP/HP: low-pass left, neutral center, high-pass right |

Reuse Optimist's effect buses and inserts where available. If track-specific filtering is not supported uniformly by an engine, document the engine-dependent behavior or implement a lightweight track insert within resource limits. Smooth DSP parameter changes; implement soft takeover for absolute controls when switching tracks. Never run UI drawing, flash operations or file parsing in the audio callback.

**External controller knobs (2026-11-10):** CC 70–76 on any channel drive the panel encoders as relative steps: CC 70–73 → KNOB 1–4, CC 74 → SELECT, CC 75 → PRESETS, CC 76 → ALGORITHM (an Akai MPK mini 3's K1–K7 defaults). Implemented in `io/midi/perf.c`.

**UI pages:** Perform (layers, current track, split, effect values), Sound (engine/preset), Split (mode/note/width/direction), Drums (kit/level), Player (song/transport/tempo/mutes), Save (performance name/slot). The selected track cycles through A, B, Drums and Player. Existing button/encoder assignments must be confirmed from Optimist's hardware scanner before implementing short/long presses. Do not assume unverified physical control counts or behaviors; use a banked UI if necessary.

## 4. Architecture

```text
USB MIDI / TRS MIDI / Bluetooth MIDI / SMF playback
                         |
                MIDI event normalization
             (source, channel, timestamp)
                         |
              Performance MIDI router
               /       |        \
       Layer A       Layer B     GM drums
       (CH 1)        (CH 1)      (CH 10 live
                                  CH 10 song)
               \       |        /
               Adaptive voice allocator
                         |
                Optimist synth engines
                         |
           Track gains / filters / FX sends
                         |
           Shared chorus / delay / reverb
                         |
                  Stereo audio output
```

Track 4 is an SMF event source routed into this graph, not a separate permanent synthesizer. Keep the audio path bounded, nonblocking and free of heap allocation. Maintain source-aware event ownership and generation-safe voice handles.

### 4.1 Proposed logical modules

```text
performance/
  performance_manager
  layer_manager
  split_blend
  performance_store
midi/
  input_normalizer
  performance_router
  note_ownership
voices/
  adaptive_allocator
player/
  smf_parser
  event_scheduler
  transport
  gm_channel_mapper
ui/
  perform_page
  sound_page
  split_page
  drums_page
  player_page
  save_page
```

These are **proposed module boundaries**, not claims about existing Optimist filenames or APIs. Map them onto the audited repository without duplicating state.

### 4.2 Voice policy

Priority order: (1) live Layer A/B, (2) live drums, (3) song melodic accompaniment, (4) releasing voices and embellishments. Use actual engine voice/resource costs; do not assume all engines cost the same. Under pressure, steal lower-priority voices first, using release state, age and cost as deterministic tie-breakers. Coordinate paired layered notes when possible. Never release an unrelated voice due to a stale note-off. Expose counters for active voices, steals, dropped notes and underruns.

### 4.3 Persistence

Use a compact versioned binary performance record containing all track settings, engine and preset IDs, split configuration, FX, drum kit, song mapping and transport defaults. Include schema version, record length and CRC; use recoverable or atomic writes where supported. Preserve safe defaults on corruption and stable engine identifiers across builds. Target at least eight performance slots **if measured flash permits**.

## 5. Real-time and quality requirements

- MIDI and SMF parsing must not block audio rendering; use bounded queues and preallocated buffers.
- Schedule SMF events from a monotonic clock, accounting for tempo changes without cumulative timing drift.
- Pause/stop must clean up song-owned notes without affecting live notes.
- Keep original MIDI source/channel/note and allocated voice identity for correct note-off behavior.
- Smooth effect and gain transitions to prevent clicks.
- Save operations must not disrupt audio; detect incomplete writes.
- Preserve Optimist's existing bootloader, firmware packaging, backup and recovery procedures.
- Maintain upstream license notices and comply with applicable licenses.

### 5.1 Proposed acceptance targets (not yet measured)

| Test | Target |
|---|---|
| MIDI-to-audio onset | <15 ms measured |
| Note ownership | 100% correctness in automated tests |
| Audio underruns | Zero during 30-minute nominal stress test |
| MIDI scheduling | Within ±2 ms nominal event timing |
| Preset recall | <500 ms for compatible loaded engines |
| Parameter changes | No audible clicks under normal use |
| Flash/RAM | Within actual Optimist build limits with documented headroom |
| Corrupt storage | Safe boot and default performance recovery |

Stress-test Prophet + FM6 layers, sustained chords, live drums, accompaniment, FX knob movement and UI navigation simultaneously. Document actual polyphony and CPU/flash/RAM figures rather than asserting unmeasured capacity.

## 6. Milestones and acceptance gates

Phase numbers match the §7 prompt numbers.

| Phase | Deliverable | Gate |
|---|---|---|
| 1 | Pinned Optimist baseline, audit, simulator baseline | Reproducible unchanged build, tests and WASM simulator; documented controls, resources and recovery |
| 2 | Router, dual layers, split/blend, ownership, allocator, live drums | Both engines respond to CH1 with independent presets; hard split, crossfade, note-off/sustain, priority and overload tests pass without stuck notes; live CH10 drums mapped correctly |
| 3 | Performance UI, selected-track FX, save/recall | Four parameters follow track selection without jumps or clicks; full performance persists and corruption is recoverable |
| 4 | Performance-only build | Non-performance features off or hidden; remaining screens in the performance UI; freed flash/RAM/CPU measured; every button mapped |
| 5 | SMF accompaniment | Type 0/1 test songs play with correct tempo and cleanup; song CH10 reaches the drums |
| 6 | Validation and hardware release | Stress tests, backup/restore, firmware package validated |

## 7. Agent implementation prompts

Six prompts replace the original six (the stripping step, Prompt 4, was added 2026-11-10). Changes from the original set:
- The simulator check is now a gate in every prompt, not something done only at the end.
- Hardware facts (control count, flash/RAM) move into the audit, because later prompts depend on them.
- Live channel-2 drum routing joins the MIDI router.
- Note ownership and voice allocation are merged, because they share voice handles.
- Persistence joins the UI, because the Save page needs it.

The original text is in `spec.orig.md`.

### Common rules (apply to every prompt)

> Work in `optimist/` on the `performance-v1` branch (Optimist pinned at `e6d9f5d`). Use only APIs confirmed in the audit report (`docs/PERF-AUDIT.md`); never invent filenames or APIs. Reuse existing Optimist engines, effect buses, MIDI handlers, storage and UI page machinery; put new code under a single build flag `PERF` that is exposed as a builder item, with the `performance-v1` profile switching it on. The audio callback must not allocate heap memory, block, draw UI, write flash or parse files.
>
> **Gate for every prompt:** (1) `make test` passes, including the new host tests; (2) the WASM simulator builds (`python3 tools/sim.py build --wasm`) and the behaviour is shown in `make web SIM=1`, driven through `sim_midi_in` and the panel, with a scripted check added next to `tests/sim_test.mjs`; (3) the report lists the files changed, the measured flash, RAM and CPU deltas from the builder budget, and anything left open. Never flash hardware or run `make flash` without explicit approval. Keep license notices.

### Prompt 1: Audit and simulator baseline

> Build the unchanged pinned Optimist: `make setup`, `make test`, `make classic PROFILE=mots-config`, the WASM simulator and `make web SIM=1`. Record what each step needs (toolchain, SDK files, the `fm1base` submodule over SSH). Write `docs/PERF-AUDIT.md` with exact paths and functions for: the audio callback and block size; the track struct and the per-track engine/preset; the engine registry and the note on/off API; the voice allocator, voice ownership and the CPU guard; MIDI input (USB/TRS/BLE) and channel-to-track routing, including whether two tracks can already share a channel; the drum engine and its lane/note mapping; send/return effects, inserts and the per-track filter; the panel's buttons and knobs (exact count and roles) and the page system; the project/preset storage format (versioning, CRC, write safety); free flash for performance slots and songs; licences. For each spec module in §4.1, say whether it maps to existing code (reuse), needs an extension, or is missing. Create the `performance-v1` branch and profile. No functional changes.

### Prompt 2: MIDI router, dual layers, split/blend, ownership, allocation, live drums

> Build a source-aware router: every event carries (source: live USB/TRS/BLE or SMF, channel, timestamp). Live channel 1 goes to Layer A and Layer B (two Optimist tracks with independent engine, preset, level, ±24 transpose, tuning, velocity response, mute and FX sends). Live channel 10 goes to the drum track, mapped through a documented table from GM percussion notes to kit lanes; unmapped notes are ignored. Implement LAYER, HARD and BLEND modes (split 0–127, default 60; width 0–24, default 12, where 0 behaves as HARD; reversible direction) with equal-power gain from a precomputed table, applied as per-voice output gain; velocity is never changed. Note ownership: each note-on records generation-safe handles for the voices it started, and note-off, sustain and all-notes-off release exactly those voices, correctly across changes to split, transpose, preset or selected track, and for repeated same-pitch notes and stale handles. Extend the existing allocator with priorities in this order: live layers, live drums, song, then releasing voices. Steal deterministically by release state, then age, then measured per-engine cost, and keep layered A/B pairs together where feasible. Expose counters for active voices, steals, dropped notes and underruns. Voice capacity, following Melodee (keremimo/melodee 1.6.1, GPL-3.0-only; keep its notices): (a) replace the fixed `NVOICE 8` / `CG_VBUDGET` with a weighted voice budget (cost points per engine, adjustable in GLO, about 16–18 with the second core), with the CPU guard enforcing it; this costs RAM per extra voice slot, so measure it against the profile budget. (b) Port Melodee's two-core PROPHET path so PROPHET plays 8 voices instead of 5. (c) Set `BT_AUDIO=0` in `config/profiles/performance-v1.config` to free the Bluetooth audio encoding job. Report polyphony for PROPHET + FM6 before and after, measured in the simulator and the emulator; only Prompt 5 measures it on hardware. Tests: ownership (100% of cases), saturation, sustain, live/song overlap (with a synthetic song source), and a Prophet + FM6 benchmark in the simulator and against the CPU guard. Simulator demo: Prophet pad and FM6 EP with a C4 blend, plus drums on channel 10.

### Prompt 3: Performance UI, selected-track FX, save/recall

> Use the page system and the controls confirmed in the audit. The **Perform** page follows `intf.png`: the Layer A patch name with its engine tab, SPLIT, the Layer B patch name with its engine tab, a DRUMKIT box, a song-name box, and REV/CHO/DEL/FIL across the bottom. The selected track is highlighted. Add Sound, Split, Drums, Player and Save pages, using banked controls if the panel is short. Selecting a track cycles A, B, Drums, Player. Knobs 1–4, in the sketch order and matching §3, control reverb send, chorus send, delay send and a bipolar LP/HP filter on the existing buses and per-track filter (record any engine where this behaves differently), with smoothing and soft takeover. Persistence: a versioned binary performance record (schema version, length, CRC, stable engine IDs) holding both layers, split, FX, drum kit, song mapping and transport defaults. Write it atomically or recoverably, outside the audio path, and fall back to safe defaults if it is corrupt. Provide as many slots as the measured flash allows, with a target of 8. Tests: no knob jumps on track switch, navigation while notes are held, recall under 500 ms, and recovery from a corrupt or partly written record (including the simulator's flash image).

### Prompt 4: Performance-only build (strip Optimist)

> Decided 2026-11-10: JTS22 is a performance distribution, not a general Optimist build. A lot of Optimist's functionality is removed so that the instrument stays simple, visible and light.
>
> **Remove or switch off** in the `performance-v1` profile and in navigation: the step sequencer and its pages (STEPS, motion, parameter locks, fills, conditions, note generators, patterns, scenes, the song arranger, undo history), sampling and the slicer, SD-card mode, Wi-Fi/Ableton Link and JANK, punch-in effects, macros, the scope, and engines and inserts that no performance preset uses. Keep the MIDI clock only if the song player (Prompt 5) needs it.
>
> **Keep:** the engines used by the layers (PROPHET, FM6 and a light subtractive engine), the drums, the send effects and track filter, MIDI in (USB/TRS/BLE), the CPU guard, project save/load (now "performances"), the updater and backup/rescue.
>
> **How:** builder items set to off where they exist (no deleted upstream code where a flag will do, so upstream fixes still merge), and screens and buttons hidden under `PERF`. The remaining screens a performer needs (the PRESETS list, SAVE/LOAD, a short sound-edit page, SETTINGS) move to the new performance renderer: blue background, white pills, Inter Tight bold, as PERFORM and MIX. Every panel button gets a defined job in the performance UI; anything unassigned does nothing rather than opening an old Optimist page.
>
> **Report:** the list of what was removed with the flash, RAM and CPU freed (real builds), and the button map. Spend the freed resources on polyphony (the voice budget) and the sampled piano if wanted. The gate is the same as every prompt: host tests, the simulator, then hardware with approval.

### Prompt 5: SMF accompaniment (Track 4)

> Only after the storage decision from Prompt 1: either an on-device song library, one built-in demo song, or songs streamed in over MIDI. Write a bounded SMF Type 0/1 parser that runs outside the audio callback and uses preallocated buffers. It must handle variable-length delta times, running status, tempo, notes, program change, channel volume, sustain, pitch bend and End of Track, skip unknown events safely, and reject malformed files predictably. Schedule from a monotonic clock with no cumulative drift across tempo changes. Transport: play, stop, pause, resume, loop, song select and a tempo multiplier. Song channel 10 goes to the drum track (never transposed). A configurable subset of melodic channels goes to existing engines with a limited voice budget at song priority; unsupported programs follow a documented fallback. Stop and pause release only song-owned notes. Tests: Type 0 and Type 1 fixtures, a tempo-change timing check (±2 ms), malformed files, and cleanup on stop while live notes are held. Simulator demo: the full vertical slice from §8.

### Prompt 6: Validation and release

> Regression and stress testing in the simulator and the emulator (`make emu-check`): a 30-minute stress run with Prophet + FM6 layers, held chords, live drums, the song playing, knob movement and page navigation. Then, only with explicit approval, on hardware: MIDI-to-audio latency, `make cpu` load, underruns, and flash/RAM headroom. Package with Optimist's toolchain (`make package PROFILE=performance-v1`). Document backup, install, recovery and the return to stock firmware. Do not flash without approval.

## 8. V1 definition of done

A physical FM-1 reliably plays a Prophet-style pad and FM6 electric piano together, supports C4 hard and blended splits, allows four selected-track FX controls, plays GM-mapped drums from external MIDI channel 10, runs a restricted MIDI accompaniment song, prioritizes live notes under load and saves/recalls the complete performance. It passes source-aware note ownership tests, audio stress testing, storage integrity and firmware recovery validation.

**First vertical slice:** Prophet pad + FM6 electric piano, C4 split, 12-semitone blend, GM drum kit, selected-track FX, one accompaniment song and one saved performance.

## 9. References

- [Optimist firmware](https://github.com/w0ts/optimist) — primary implementation base; verify repository and toolchain at pinned commit.
- [Felucca firmware](https://github.com/pgavazzi/felucca) — reference for multitrack, drum and preset behavior.
- [SLOOP FM-1](https://github.com/isod89/sloop-fm1) — performance-control reference.
- [Clipper FM-1](https://github.com/isod89/clipper-fm1) — preset-navigation reference.

**Open verification items:** exact hardware control mapping, engine compatibility, flash/RAM budget, song storage, performance-slot capacity and real-time benchmarks. None should be treated as proven until checked against a pinned Optimist build and hardware.
