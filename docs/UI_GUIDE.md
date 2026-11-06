# FM-23 UI guide (buttons, encoders, gestures)

Quick reference for the panel. The screen is a 240×240 monochrome phosphor UI; the selected
parameter row is inverted.

## Buttons (14)

| Button | Page / action |
|---|---|
| **FX** | FX page — delay/reverb sends, drive, type |
| **SCL** | **GEN** page — root, scale, style, mode (song) |
| **ENV** | **Envelope page** — per-voice A / R / decay / pitch-tauP |
| **LFO** | **LFO page** — per-voice ON / rate / depth / shape |
| **EDIT** | Timbre edit — cycles BASS → LEAD → DRUMS |
| **GLO** | Global — root, scale, MIDI clock |
| **HOME** | Performance page (drum pads + transport) |
| **SAVE** | Save current page's data (settings / pattern+song) |
| **ARP** | ARP page (arpeggiator on/off, style, octave, rhythm) |
| **SEQ** | Sequencer page (pattern/chain edit, transport) |
| **PLAY** | Start/stop the pattern or song |
| **REC** | Arm REC (see gestures) — also indicator |
| **OCT− / OCT+** | Shift bass/lead octaves; **hold both 5 s = recovery (UBOOT)** — never reused for anything else |

## Encoders (7)

| Encoder | Role |
|---|---|
| **SELECT** | move the cursor row (the inverted bar) |
| **ALGORITHM** | change the selected row's value |
| **PRESETS** | cycle the current pattern number |
| **KNOB 1..4** | change the four rows shown on the page |

## Pages

| Page | Rows (K1..K4) | Notes |
|---|---|---|
| HOME | TEMPO, SWING, MASTER, PATTERN | 8 drum pads; bass keys 8–17, lead keys 18–26 |
| SEQ | LANE, TEMPO, SWING, PATTERN | keys = step gates; REC+key = velocity |
| SEQ (PART=1) | PART, SONG, STEP, LENGTH | keys 0–7 choose the bank cell for the chain step |
| SEQ | LANE, TEMPO, AUTO, PATTERN | LANE 0–6; 6 latches the PART (chain) editor |
| ARP | ON, STYLE, HOLD, OCTAVE | HOLD latches notes as a drone (and a constant root drone under the beats) until PLAY-stop / REC |
| EDIT BASS | ALGO, CUTOFF, DRIVE, ACCENT | ALGO 0 classic / 1 B01 / 2 B02 / 3 B03 / 4 B04 (default 4 = acid) |
| EDIT LEAD | ALGO, LFO, CUTOFF, LEVEL | ALGO 0 classic / 1 L01 / 2 L02 / 3 L05 / 4 L07 / 5 L03 / 6 L04 / 7 L06 (default 7 = scream) |
| EDIT DRUMS | KICK DECAY, LVL, KIT, MACRO | KIT 0 classic / 1 tribe / 2 mental / 3 hardtekk · MACRO 0 off / 1 RUBBER / 2 METAL / 3 SCREAM |
| FX | DELAY, REVERB, DRIVE, TYPE | sends default 0 |
| GLO | ROOT, SCALE, MIDI CLK | |
| GEN | ROOT, SCALE, STYLE, MODE | twist ROOT/SCALE/STYLE to auto-generate; MODE=1 builds the song chain; tap any pad to GENERATE now |
| ENV | ATTACK, RELEASE, DECAY D60, PITCH tauP | applies to the selected voice (see EDIT) |
| LFO | ON, RATE, DEPTH, SHAPE | per-voice LFO (bass filter / lead vibrato) |

## Gestures
- **REC + step key** (SEQ): cycle that step's velocity (90 → 110 → 127).
- **LANE = 6** (SEQ): latches the PART (song-chain) editor — keys pick the bank cell for the highlighted chain step; set PART to 0 to exit.
- **GEN**: editing ROOT/SCALE/STYLE regenerates instantly; MODE → SONG builds the 8-cell A/A'/B/B' chain with cross-pattern legato (SLIDE lane). Every generation flashes **GENERATED** and blinks the step LEDs. **Any pad press on GEN is the explicit trigger** — generate a fresh cell, or a fresh song in MODE=1, with the current settings.
- **TD-3 lanes**: patterns carry SLIDE (glide into a note, works across song cells) and GATE (note length in steps; default 1). GEN writes style-appropriate slides.
- **OCT− + OCT+ held 5 s**: recovery (UBOOT) — do not repurpose.
- **SAVE**: page-dependent — HOME/GLO saves settings, SEQ saves pattern (+song in chain mode), GEN saves settings + song.

## Envelope/LFO semantics
- Values are 0–127 controls mapped to approximate time (1–~640 ms). ENV uses D60 decay semantics and a tauP pitch-envelope time constant for melodic voices.
- LFO shapes: 0 sine, 1 triangle, 2 square, 3 sample&hold. RATE maps ~0–25 Hz.
- LFO ON+RATE+DEPTH for the bass modulates the filter; for the lead it modulates pitch (vibrato).

## Safety
Recovery and update paths are unchanged — see `docs/SAFETY.md`. First flash: return to official V15 first if coming from another custom firmware.
