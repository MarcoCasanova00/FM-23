# FM-23 Firmware Specification

FM-23 is a self-contained **mental tekno / tribe groovebox** for the M-VAVE FM-1.
It ports the sound theory and concept of the SANTO CENTER music composer (the
TD-3 acid bass, Volca tribal percussion and FM-1 spiral lead engine) into one
instrument: no exports, no DAW — the device generates, plays and performs a
full mental-tekno track on its own.

## What it is

A pocket live-performance instrument. It carries three sound engines — a
303-style acid bass, a lead/drone synth in the spirit of Carnac OS / Moog-mini
(oscillators, filters, detune, LFO, effects), and a synthesized percussion
engine — driven by a pattern/sequence machine that implements the SANTO CENTER
musical grammar: Phrygian-family scales, 2–4-note melodic cells, accent +
slide phrasing, rest masks that let the 4/4 kick breathe, polyrhythmic
percussion (3-cycle and 5-cycle), and a five-part low→high story arc that
loop-resolves to the root.

## Musical grammar (ported from SANTO CENTER)

- **Melodic cells:** per pattern, all notes come from a small pool (root, b2,
  b3, 5th, octave) of the selected scale. No wandering chromatic runs — the
  hypnotic 2–4-note feel is the point.
- **Rest masks:** a pattern knows where the kick sits; other voices leave air
  there so the pounding 4/4 kick and the 3/5 polyrhythms cut through.
- **Accents and slides:** per-step accent and slide flags. Slides are the
  "viscous squelch" glides; accents land on syncopated hits. Every pattern's
  last slide resolves inside the pattern and ends on a rest (no cross-pattern
  glide bleeding).
- **Story arc:** LOW (drone intro) → HALF-TIME (mellow 80-feel bed) → MEDIUM
  (augmentation) → HIGH (peak octave-topping screams) → VARIATION (outro).
  Each section derives from the previous one by augmentation; every section
  resolves its last sounding note to the root.
- **Scales (bank):** Phrygian, Phrygian Dominant, Dorian b2, Natural Minor,
  Harmonic Minor, Hungarian Minor, Locrian, Insen, Iwato, Double Harmonic,
  Octatonic HW, Tritone Industrial, Pentatonic Minor, Blues, Chromatic.
- **Polyrhythm:** the percussion section can run fixed-length cycles (3, 5,
  7 steps) against the 16-step grid, producing phase-shifting tribal layers.

## Sound engines

### 1. 303-style Acid Bass (monophonic)
- Saw / square oscillator, resonant low-pass filter with envelope, accent
  (per-step level + filter punch), slide/portamento between notes.
- Distortion / drive stage after the filter (squelch, hard-clip at high drive).
- Knobs shape cutoff, resonance, accent amount, slide amount.
- One voice, note bank = the 16 lowest keys + MIDI in. Octave via OCT−/OCT+.

### 2. Lead / Drone Synth (2 voices) — Carnac OS / Moog-mini spirit
- Two oscillators per voice: wavetable/FM with detune (Moog-mini-style
  chorusing and fat unison) and cross-modulation.
- Filter: low-pass with resonance, key-track, filter envelope, LFO.
- Drone mode: sustained root/b2/b5 drones for the LOW section; screaming
  octave-topping leads for the HIGH section.
- Effects: delay (tape/BBD), spring reverb, chorus on the lead, with per-part
  sends. Moog-mini-style warmth: slight drive, pitch drift, envelope-based
  filter movement.
- Note bank = 16 keys + MIDI in; OCT−/OCT+ transpose.

### 3. Percussion engine (synthesized, up to 8 voices)
| Voice | Synthesis | Notes |
| --- | --- | --- |
| KICK | sine pitch-envelope thud + click | 4/4 anchor; pitch/decay knobs |
| SNARE/CLAP | noise burst + tonal body | backbeat 2 & 4 |
| CL.HAT | short high-pass noise | 16th motor |
| OP.HAT | longer noise decay | offbeat lift |
| PERC MET | metallic ring/ping (FM) | 3-cycle polyrhythm |
| PERC STR | tuned gong/woodblock (FM) | 5-cycle polyrhythm |
| NOISE SWEEP | filtered noise swell | texture / riser |
| MOD.ZAP | FM zap / glitch | one-shot FX |

### Effects (global)
- Delay (BBD/tape), spring reverb, chorus, drive/distortion on bass and lead.
- Per-part effect sends, so drums can stay dry while the lead gets space.

## Controls

**27 keys**
- Performance mode (HOME): bottom keys trigger drum voices (Volca-style
  hotkeys), upper keys play bass/lead notes.
- SEQ mode: keys are the 16 steps of the focused lane (toggle/velocity).
- ARP mode: chord keys + step keys as on the Carnac-style arpeggiator.

**14 buttons**
- FX — effects page (delay, reverb, drive sends).
- SCL — scale + root selection page.
- ENV — envelope page (amp/filter envelopes per engine).
- LFO — LFO page (rate, shape, depth, destinations).
- EDIT — sound edit page (per-voice parameters, oscillators, filter).
- GLO — global settings (tempo, swing, MIDI, brightness, tune).
- HOME — performance page.
- SAVE — save current pattern/sound/kit (two-step confirm).
- ARP — arpeggiator page.
- SEQ — sequencer page.
- PLAY — start/stop the pattern chain (holds both seq + arp).
- REC — record a pattern from the keys (step record).
- OCT− / OCT+ — octave for bass/lead; both held 5 s = recovery (platform).

**7 encoders**
- SELECT — move through page rows / select parameter.
- ALGORITHM — change highlighted value / step through engine architectures.
- PRESETS — browse stored patterns/sounds/kits.
- KNOB 1..4 — four page parameters (per-page assignments).

**Gestures**
- Long-press buttons for alternate functions; double-tap for quick actions.

## Screens (240×240, amber CRT palette: dark, amber/green glow)
- **HOME** — performance pad grid, transport, mode, tempo, pattern name.
- **SEQ** — 4–6 lane × 16-step matrix, playhead, per-lane event type
  (note/dynamics/gate/ratchet/probability), loop points.
- **ARP** — arp pattern editor, style, rate, octave, rhythm.
- **EDIT** — per-engine sound parameters (3 sub-pages: BASS / LEAD / DRUMS).
- **FX** — delay/reverb/drive sends, types, times, feedback.
- **GLOBAL** — scale, root, tempo, swing, MIDI channels, audio/brightness.

## Sequencing and time
- Patterns of 16 steps; 4–6 lanes (KICK, SNARE/HAT, PERC, BASS, LEAD, FX).
- Up to 16 patterns stored; a song chain orders them (SD: 8-chain).
- Tempo 130–200 BPM; swing/shuffle; per-lane prescale changes
  (16ths, 8ths, triplets, 32nds).
- MIDI clock in syncs the transport; internal clock also runs free.

## Storage (flash store: 36 records × 4 KB)
- Patterns: 12 slots (16-step, 6 lanes).
- Sound presets: 12 slots (bass, lead, drums kits).
- Kit: percussion engine voice set (1 slot).
- Global settings + song chain: 2 slots.
- Layout chosen so each record fits 4 KB.

## MIDI
- **In:** USB (notes, CC, clock). TRS MIDI IN left off (feature option).
- **Out:** USB (notes, CC, clock) — for monitoring/recording, not required.
- Note input drives bass/lead + drum triggers; CC maps to knob parameters.

## Milestones
- **M1 skeleton:** name screen, all pages render, controls table wired to
  visible values, one voice of each engine sounding (bass note, a kick, a lead
  note), one stored pattern, host tests green in the emulator.
- **M2 sequencing:** 4-lane 16-step sequencer, play/stop, pattern store/recall,
  song chain, step record, tempo/swing, MIDI clock in.
- **M3 full engines:** complete bass engine (accent/slide/resonance), lead
  engine (detune/LFO/FX), percussion set (8 voices), per-engine edit pages.
- **M4 theory engine:** scale bank, melodic cell generator, rest masks,
  accent/slide grammar, 3/5 polyrhythms, five-part story arc with root
  resolution, per-part FX sends.
- **M5 polish:** ARP page, performance hotkeys, MIDI out, backup/safety docs,
  battery behavior, final budgets.

## Out of scope for v1
- Audio input (hardware limitation), samples/PCM loading, wireless, export of
  files to other hardware (this is the instrument), multi-track DAW editing.

## Open questions
- Exact voice budget split (bass 1 + lead 2 + drums 8 on the FM-1's cores)
  — resolve during M3 with the budget tests.
- Whether the lead engine should grow to 4 voices in DUAL mode when drums are
  idle (platform allows; confirm after M3 measurements).
