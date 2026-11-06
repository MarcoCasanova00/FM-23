# FM-23 UI guide (buttons, encoders, gestures)

Quick reference for the panel. The screen is a 240×240 monochrome phosphor UI; the selected
parameter row is inverted. **Every parameter page works the same way**: SELECT moves the
cursor, ALGORITHM edits it, KNOB 1–4 edit the visible rows.

## Buttons (14)

| Button | Page / action |
|---|---|
| **HOME** | Performance page: transport, INSTRUMENT, tempo/swing/master, drum pads + bass/lead strip |
| **SEQ** | Sequencer page: pattern matrix, melodic summary, chain editor |
| **ARP** | ARP page (arpeggiator on/off, style, hold, octave) |
| **EDIT** | SOUND tabs — cycles BASS → LEAD → DRUMS → LAYER |
| **ENV** | MOD page, envelope view — per-voice attack / release / decay / pitch-tauP |
| **LFO** | MOD page, LFO view — per-voice ON / rate / depth / shape |
| **FX** | FX page — delay/reverb sends, drive, type |
| **GLO** | Global — root, scale, MIDI clock |
| **SCL** | GEN page — seed, scale, style, mode (song) |
| **SAVE** | Save the current page's data (settings / pattern+song) |
| **PLAY** | Start/stop the pattern or song |
| **REC** | Loop pedal on any page except SEQ (see gestures); SEQ keeps step-edit REC |
| **OCT− / OCT+** | Shift bass/lead octaves; **hold both 5 s = recovery (UBOOT)** — never reused for anything else |

## Encoders (7)

| Encoder | Role |
|---|---|
| **SELECT** | move the cursor row (the inverted bar) |
| **ALGORITHM** | change the selected row's value |
| **PRESETS** | cycle the current pattern number |
| **KNOB 1..4** | change the four visible rows (six on the SOUND tabs) |

## Pages

| Page | Rows (K1..K4) | Notes |
|---|---|---|
| HOME | TEMPO, SWING, MASTER, INSTRUMENT | INSTRUMENT = the whole keyboard: 0 DRUMS / 1 BASS / 2 LEAD. Drum pads; bass/lead strip lights as you play |
| SEQ | LANE, TEMPO, AUTO, PATTERN | keys = step gates; REC+key = velocity; AUTO = per-step automation target |
| SEQ (PART=1) | PART, SONG, STEP, LENGTH | keys 0–7 choose the bank cell for the chain step (enter via LANE 6) |
| ARP | ON, STYLE, HOLD, OCTAVE | HOLD latches notes as a drone (and a constant root drone under the beats) until PLAY-stop / REC |
| EDIT BASS | ENGINE, CUTOFF, DRIVE, ACCENT, PORTA, ARCH | ENGINE 0 classic / 1 B01 / 2 B02 / 3 B03 / 4 B04 acid (default) · ARCH 0 SINGLE / 1 DUAL / 2 STACK (sub octave) |
| EDIT LEAD | ENGINE, CUTOFF, LEVEL, VOICE, PORTA, ARCH | ENGINE 0 classic / 1 L01 / 2 L02 / 3 L05 / 4 L07 / 5 L03 / 6 L04 / 7 L06 scream (default) · VOICE 0 RETRIG / 1 LEGATO / 2 UNISON · ARCH 0 SINGLE / 1 DUAL / 2 STACK |
| EDIT DRUMS | KIT, MACRO, LVL, DECAY | KIT 0 classic / 1 tribe / 2 mental / 3 hardtekk · MACRO 0 off / 1 RUBBER / 2 METAL / 3 SCREAM |
| EDIT LAYER | ANALOG B, ANALOG L | per-voice analog drift, 0 exact → 127 heavy (default 30) — the instrument itself lives on HOME |
| FX | DELAY, REVERB, DRIVE, TYPE | sends default 0 |
| GLO | ROOT, SCALE, MIDI CLK | |
| GEN | SEED, SCALE, STYLE, MODE | twist to auto-generate; MODE=1 builds the song chain; tap any pad to GENERATE now |
| MOD (ENV) | ATTACK, RELEASE, DECAY D60, PITCH tauP | applies to the voice selected in SOUND (EDIT) |
| MOD (LFO) | ON, RATE, DEPTH, SHAPE | per-voice LFO (bass filter / lead vibrato) |

## Sound crafting (how to make the bass / lead / drones)
- **Engines**: `EDIT` cycles BASS → LEAD → DRUMS → LAYER. The first row is
  **ENGINE** — it shows the voice name (e.g. `B04 ACID`, `L06 SCREAM`). Turn it
  to choose another engine; CUTOFF/DRIVE/ACCENT/LEVEL/PORTA/ARCH shape it.
  `ENV`/`LFO` (the MOD page) shape the envelope and modulation of that voice.
- **Keyboard instrument** (HOME → INSTRUMENT row): the WHOLE keyboard plays one
  instrument — 0 **DRUMS** (keys 0–7 pads, keys 8–26 cycle the 8 drum voices),
  1 **BASS** (every key is a bass note), 2 **LEAD** (every key is a lead note).
  No splits: the selected instrument owns the full keyboard.
- **Voice modes**: LEAD tab → VOICE 0 RETRIG / 1 LEGATO / 2 UNISON (4 detuned
  playheads on classic, a detuned second playhead on the bank voices).
- **Analog feel**: LAYER tab → ANALOG B/L adds per-note pitch scatter + slow
  drift (default gentle 30; 0 = exact).
- **Drones**: `ARP → ON + HOLD` latches notes as drones (Carnac-style) and the
  sequencer keeps a constant root drone under the beats; PLAY-stop / REC
  force-cut them.
- **Sequenced notes**: on SEQ, lanes 0–3 are drums, lane 4 = bass, lane 5 = lead.
  Step LEDs light where a step is set (also for the melodic lanes).

## Gestures
- **REC + step key** (SEQ): cycle that step's velocity (90 → 110 → 127).
- **REC = loop pedal** (on any page except SEQ): press once to record what you
  play (keys/MIDI/drums), press again to stop and loop it endlessly; press a
  third time to clear. The header shows L (recording) / l (playing).
- **LANE = 6** (SEQ): latches the PART (song-chain) editor — keys pick the bank cell for the highlighted chain step; set PART to 0 to exit.
- **GEN**: editing SEED/SCALE/STYLE regenerates instantly; MODE → SONG builds the 8-cell A/A'/B/B' chain with cross-pattern legato (SLIDE lane). Every generation flashes **GENERATED** and blinks the step LEDs. **Any pad press on GEN is the explicit trigger**.
- **TD-3 lanes**: patterns carry SLIDE (glide into a note, works across song cells) and GATE (note length in steps; default 1). GEN writes style-appropriate slides.
- **OCT− + OCT+ held 5 s**: recovery (UBOOT) — do not repurpose.
- **SAVE**: page-dependent — HOME/GLO saves settings, SEQ saves pattern (+song in chain mode), GEN saves settings + song.

## Envelope/LFO semantics
- Values are 0–127 controls mapped to approximate time (1–~640 ms). ENV uses D60 decay semantics and a tauP pitch-envelope time constant for melodic voices.
- LFO shapes: 0 sine, 1 triangle, 2 square, 3 sample&hold. RATE maps ~0–25 Hz.
- LFO ON+RATE+DEPTH for the bass modulates the filter; for the lead it modulates pitch (vibrato).

## Safety
Recovery and update paths are unchanged — see `docs/SAFETY.md`. First flash: return to official V15 first if coming from another custom firmware.
