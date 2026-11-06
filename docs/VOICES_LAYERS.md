# FM-23 Voices & Layers — design doc

Mapping the Carnac "voices & layers" concept onto the FM-1 / FM-23 hardware.
Status: **implemented** (M1–M3 complete) with the remaining Carnac features
explicitly out of scope.

## 0. What Carnac has, and what FM-23 is

Carnac (a larger FM-1 app) has 8 physical voices, 2 layers, a rich list of
architectures, wavetable banks, PCM waves, 3 mod slots, and per-voice analog
tolerances. It runs on two cores with self-imposed voice caps to stay in
budget.

FM-23 is a self-contained mental/tekno **groovebox** on the same FM-1 silicon
but with a single app core, a fixed-page UI, and a hard ISR budget. Measured
runtime: **1.2 µs/block vs 2900 µs budget (2417× headroom, 605× realtime)**;
static worst-case budget is at target for every audio function. RAM:
~13 KB/51200 B; pool: 264440/391168 B.

**Conclusion:** we cannot and should not copy Carnac wholesale. We ported the
*musical concept* — layered voices, performance modes, voice modes, analog
drift, porta, architecture, filter character — into FM-23's existing engines,
staying inside the budget.

## 1. What FM-23 has (the layers)

FM-23 ships three "layers":

| Layer | Engine | Options |
|-------|--------|---------|
| Drums | `cfg.kit` | 0 classic, 1 tribe, 2 mental, 3 hardtekk (+macro) |
| Bass | `cfg.bass_algo` | 0 classic, 1 B01, 2 B02, 3 B03, 4 B04 acid |
| Lead | `cfg.lead_algo` | 0 classic, 1 L01, 2 L02, 3 L05, 4 L07, 5 L03, 6 L04, 7 L06 |

Each melodic layer has its own envelope/LFO (merged into one MOD page), a
filter cutoff/reso/drive, sends to FX, and per-lane sequencer patterns with
TD-3 slide/gate lanes.

## 2. Implemented (M1–M3)

### 2.1 Performance modes — SINGLE / DUAL / SPLIT ✅
`cfg.perf` on the LAYER view.
- **SINGLE**: classic bass/lead split (bass keys → bass, lead keys → lead).
- **DUAL**: any melodic key plays both layers stacked (keyboard only; the
  sequencer keeps its lanes independent).
- **SPLIT**: keys below `cfg.split_note` play bass, from the split up play lead
  (left-hand bass / right-hand lead).

Implemented as key-routing in `sound_audio`; no new DSP. Host-tested.

### 2.2 Voice modes — RETRIG / LEGATO / UNISON ✅
`cfg.voice_mode[1]` (lead) on the LAYER view.
- **RETRIG**: a new note restarts the voice (default).
- **LEGATO**: keep the running envelope, chase the pitch (classic and bank
  paths; verified by phase continuity).
- **UNISON**: classic lead becomes 4 detuned playheads; the bank lead mixes a
  detuned second playhead before the DC block. Verified audibly.

### 2.3 Analog drift — ANALOG knob ✅
`cfg.analog[0..1]` (per layer) on the LAYER view (lead) and the bass/lead
EDIT pages' ANALOG row. Per-note scatter at note-on, plus a slow drift LFO that
eases in from 40 up; 0 is exact (verified: identical increments). Default 30
(gentle).

### 2.4 Portamento — PORTA knob ✅
`cfg.porta[0..1]` (per layer) on the bass/lead EDIT pages. 0 = none; higher =
slower constant-rate glide. Reuses the existing glide logic; host-tested.

### 2.5 Architectures — SINGLE / DUAL / STACK ✅
`cfg.arch[0..1]` (per layer) on the bass/lead EDIT pages.
- **SINGLE** (default): the current engine path.
- **DUAL**: two detuned oscillators (the lead's playheads / bass detune).
- **STACK**: adds a sub octave (tri sub-oscillator, clipped before the filter).
  Host-tested.

### 2.6 Filter flavour — (not implemented, per review)
The proposed SH-2 vs MS-20 flavour row was reviewed and **dropped** (budget
trade-off); the existing drive/abrasive shaper already covers the grit.

## 3. UI notes

The UI runs in the main loop, not the audio ISR (confirmed: the ISR only runs
`app_audio`). Therefore a downsample/8-bit-LCD restyle would save no audio
budget; the current NIGHT phosphor UI is kept as-is.

## 4. Explicitly out of scope (and why)

| Carnac feature | Why not |
|----------------|---------|
| 15 wavetable banks + PCM waves | Flash budget; banks live outside the firmware and we have no wave data. |
| 3 mod slots with visual portal | Page budget; one new page max. |
| BINAURAL / dual-core load balancing | FM-23 is single-core; stereo-per-ear is a different build. |
| 8 architectures | The 3 implemented cover the musical roles (plain/detuned/stacked). |
| Velocity split | Optional future extra on SPLIT. |

## 5. Data model (all clamped on load, saved with the config)

```c
uint8_t perf;        /* 0 single, 1 dual, 2 split */
uint8_t split_note;  /* 0..127 */
uint8_t voice_mode[2];/* per melodic layer: 0 retrig, 1 legato, 2 unison */
uint8_t analog[2];   /* 0..127 per layer (default 30) */
uint8_t porta[2];    /* 0..127 per layer (0 none) */
uint8_t arch[2];     /* 0 single, 1 dual, 2 stack */
```

## 6. Budget

Static worst-case per function is at target (bass 315, lead 541, sound_audio
2434; ±10% tolerance on the budget file). Runtime headroom is 2417×. M2/M3
additives only cost when enabled (voices/arch/porta active), and defaults keep
the base path identical.

## 7. Test plan (status)

| Check | Status |
|-------|--------|
| PERF routing (SINGLE/DUAL/SPLIT) | ✅ host tests |
| VOICE RETRIG vs LEGATO (bank phase) | ✅ host tests |
| VOICE UNISON (classic timbre change) | ✅ host tests |
| ANALOG exact at 0, scatter at 127 | ✅ host tests |
| PORTA glide changes the sound | ✅ host tests |
| ARCH STACK sub octave changes the sound | ✅ host tests |
| Melodic step LEDs on SEQ | ✅ host tests |
| ISR budget at target | ✅ |
| Emulator build + speed | ✅ 614× realtime |
| Flash/installer safety | unchanged, still green |

## 8. Open questions / future

1. **Velocity split** on SPLIT (optional).
2. **More engines** (a few more seeds) without touching the budget.
3. **Per-pattern voice-mode memory** (voice mode stored in the pattern rather
   than globally) if wanted.
