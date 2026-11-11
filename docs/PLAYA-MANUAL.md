# PLAYA — mini manual

PLAYA is a performance firmware for the M-VAVE FM-1, built on Optimist. Created by JFS with help from Claude.
This page covers everything PLAYA changes; the sounds, engines and effects themselves are Optimist's
([guide/sound.md](guide/sound.md), [guide/effects.md](guide/effects.md), [guide/drums.md](guide/drums.md)).

## The idea

- **Two layers, A and B** (tracks 1 and 2), played from one keyboard. **Drums** on their own track.
- **Every button opens one page.** Press it again for that page's next screen.
- **Knobs 1–4 always set the four boxes (or cards) on the screen.** SELECT scrolls a page's screens.
- **ALGORITHM** steps the selected track's engine (on DRUMS: the kit). **PRESETS** picks its sound.
- The lit button is the page you are on.

## The panel

| Button | Page | What is on it |
|---|---|---|
| **HOME** | PERFORM | layer A, the keyboard mode and split note, layer B, the drum kit, the four FX of the selected row |
| **SEL** (SCL) | MIX | the levels of A, B and the drums; knob 4: the master compressor (grey = off) |
| **FX** | FX (5 screens) | SENDS (REV CHO DLY FILT) → REVERB (size, damping) → CHORUS (rate, depth) → DELAY (time, feedback, colour, mix) → FILTER (low-pass sweep, resonance) |
| **ENV** | ENVELOPE | the ADSR's shape, its four values under it |
| **LFO** | LFO (2 screens) | the wave's shape; rate, wave, trigger, polarity → two destinations and amounts |
| **EDIT** | EDIT (2 screens) | the engine's eight parameters |
| **ARP** | ARP (2 screens) | mode, rate, octaves, gate → swing, probability, hold, order |
| **GLO** | GLOBAL (5 screens) | tune, tempo, THEME → the compressor → the limiter → MIDI CC 1–4 → MIDI CC 5–8 |
| **SAVE** | SAVE | the performance slots |
| **SEQ** | SEQ | the drum step sequencer |
| **PLAY** | — | starts / stops the drum sequencer |
| **OCT− / OCT+** | — | the keyboard octave; on SEQ the bar shown |
| **OCT− + OCT+** together | BLUETOOTH | pair Bluetooth MIDI keyboards (again: back to PERFORM) |
| **GLO held 2 s** | — | the PLAYA screen; let go: the credits for 3 seconds |
| **REC** | — | nothing yet (the looper: v1.5) |

On the ENV, LFO, EDIT and ARP pages the selected track is shown top right in its engine's colour; they do nothing on
the drum track ("LAYERS ONLY").

## PERFORM

SELECT moves down the screen: **LAYER A → MODE → NOTE → LAYER B → DRUMS** (the selected one is highlighted).

- **On a layer or DRUMS:** PRESETS its sound (or kit), ALGORITHM its engine, knobs 1–4 its **REV CHO DLY FILT**
  (FILT: a low-pass, 100 = open).
- **On MODE:** PRESETS (or ALGORITHM) steps the mode; knobs 1–4 are MODE, NOTE, WIDTH, DIR.
- **On NOTE:** PRESETS moves the split a semitone, ALGORITHM an octave.

### Keyboard modes

| Mode | Plays |
|---|---|
| **LAYER** | A and B on every key |
| **SPLIT** | A below the split note, B from it up (DIR B-A: the other way) |
| **BLEND** | A low, B high, and both crossfaded over WIDTH semitones around the split (equal power; WIDTH 0 = SPLIT) |
| **SINGLE** | A alone (B kept, dimmed) |

Default: BLEND at C4, 12 semitones. The note shows light when the mode does not use it.

## MIDI

| In | Goes to |
|---|---|
| Channel 1 | layers A and B, through the mode |
| Channel 10 | the drums (GM note numbers) |
| Sustain, pitch bend, mod wheel on channel 1 | both layers |
| K1–K4: CC 70–73 (any channel) | knobs 1–4 |
| K5 / K6 / K7: CC 74 / 75 / 76 | SELECT / PRESETS / ALGORITHM |
| K8: CC 77 | the selected track's level |

The eight CC numbers are set on **GLOBAL → MIDI CC 1–4 and 5–8** (press GLO until "MIDI CC"): each card is named by
what its knob does; turn it to the CC your controller sends, 0 = OFF. They are saved with the performance. The
defaults are an Akai MPK mini 3's K1–K8. USB, TRS and Bluetooth MIDI all work.

## Voices

Up to 12 notes sound at once across the layers (PROPHET up to 8 of them). If the FM-1 runs short of time, Optimist's
CPU guard sheds the quietest voices first. Tip: set each layer's VOICES (EDIT) to 4 when you use the sustain pedal.

## SEQ: the drum sequencer

- **Black keys** pick the drum (its name in the pill), **white keys** set its 16 steps.
- **ALGORITHM** sets the length: 4, 8, 12 or 16 bars (of four steps). Default 4.
- **OCT− / OCT+** (or SEQ again) show the other screens of sixteen; the title shows the length and the screen (1/2 …).
- **PLAY** starts and stops it. The small grid under the steps shows every drum.
- Steps are only edited here: on the other pages the keys just play.

## SAVE

SELECT a slot, **tap SAVE** to store the whole performance there (both layers, mode, split, FX, drums, theme), **hold
SAVE** to load it. A message confirms ("SAVED").

## Themes

GLOBAL, knob 3: **PLAYA** (the boot screen's sunset — default), **BLUE** (blue, white, yellow), **MONOCHROME** (black,
white, pink), **CREAM** (brown, cream, white), **ROLO** (charcoal, cream, orange). Saved with the performance.

## Bluetooth MIDI

The FM-1 is always visible as a Bluetooth MIDI device (a computer, tablet or phone connects to it). To play it from a
Bluetooth MIDI **keyboard**, press **OCT− and OCT+ together**: the BLUETOOTH page searches while it is open and nothing
is connected. Knob 1 sets MODE (OFF, MANUAL, AUTO: reconnect to the last keyboard at power-on). SELECT picks a device,
**SAVE** connects to it (or lets go of the one connected), **hold SAVE** forgets a remembered one. Remembered devices are
white, new ones light. The mode is saved when you leave the page.

## Getting back

Back up before installing. If an install stops, install again; if the FM-1 does not start, hold **OCT−** while
switching on and install again, or return to the official firmware ([guide/getting-back.md](guide/getting-back.md)).
