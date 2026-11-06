# FM-23 Voices & Layers — design doc

Mapping the Carnac "voices & layers" concept onto the FM-1 / FM-23 hardware.
This is a design first: no code was written for this doc. Status: **proposal** for review.

## 0. What Carnac has, and what FM-23 is

Carnac (a larger FM-1 app) has 8 physical voices, 2 layers, a rich list of
architectures, wavetable banks, PCM waves, 3 mod slots, and per-voice analog
tolerances. It runs on two cores with self-imposed voice caps to stay in
budget.

FM-23 is a self-contained mental/tekno **groovebox** on the same FM-1 silicon
but with a single app core, a fixed-page UI (9 pages already), and a hard ISR
budget of 2772 units (measured `sound_audio` = 2436 → **~12% headroom**).
RAM: 13012/51200 B; pool: 264440/391168 B.

**Conclusion:** we cannot and should not copy Carnac wholesale. We port the
*musical concept* — layered voices, performance modes, voice modes, analog
drift, glide, filter character — into FM-23's existing engines, staying inside
the budget.

## 1. What FM-23 already has (the layers exist)

FM-23 already ships three "layers" in all but name:

| Layer | Engine | Options |
|-------|--------|---------|
| Drums | `cfg.kit` | 0 classic, 1 tribe, 2 mental, 3 hardtekk (+macro) |
| Bass | `cfg.bass_algo` | 0 classic, 1 B01, 2 B02, 3 B03, 4 B04 acid |
| Lead | `cfg.lead_algo` | 0 classic, 1 L01, 2 L02, 3 L05, 4 L07, 5 L03, 6 L04, 7 L06 |

Each melodic layer already has its own `ENV` and `LFO` page (per-voice attack/
release/decay/pitch and rate/depth/shape), a filter cutoff/reso/drive, and its
own sends to FX. The sequencer already has per-lane patterns (lanes 0–5) and
TD-3 slide/gate lanes.

So the foundation is 80% there. The gap is the **experience**: today the user
edits one layer at a time with no notion of *performance mode*, *voice mode*,
*drift*, or *architecture* — and the UI does not make the layer structure
visible.

## 2. What we port (the "similar" part)

### 2.1 Performance modes (HOME) — SINGLE / DUAL / SPLIT
A new `cfg.perf` (0 single, 1 dual, 2 split) row on HOME.

- **SINGLE**: only the selected layer plays (bass *or* lead). Lowest CPU, best
  for tight acid lines.
- **DUAL**: bass and lead both play the same key (stacked). The "strong beats +
  303 + haunted lead" full sound. Voices are shared: if a layer is mono, the
  other layer gets the freed voices.
- **SPLIT**: bass below a split note, lead from the split up (left-hand bass /
  right-hand lead). `SPLIT NOTE` knob (0–127, C-2..G8) sets the boundary.
  Velocity split is optional (later).

This directly maps the "DUAL / SPLIT share the keyboard" Carnac behavior and is
cheap: it is key-routing logic in `sound_audio`, not new DSP.

### 2.2 Voice modes (EDIT per layer) — RETRIG / LEGATO / UNISON
A `cfg.voice_mode` (0 retrig, 1 legato, 2 unison) per melodic layer.

- **RETRIG**: a new note restarts the envelope (current behavior).
- **LEGATO**: overlapping notes glide — no retrigger. The TD-3 slide lane
  already gives us cross-pattern glide; this generalizes it to performance.
- **UNISON**: 2 detuned copies of the voice. The classic lead already has two
  detuned oscillators; we extend the same idea to the bank engines with a small
  detune spread. Cost: ~1 extra osc per voice in the lead path (measured before
  as affordable; re-benchmark at the end).

### 2.3 Analog drift (EDIT per layer) — ANALOG knob
A `cfg.analog` (0–127) knob per layer. Each voice gets small fixed tolerances
(pitch, cutoff, resonance, envelope rate/level) seeded from its note number and
a per-note hash; from ~40 up, a slowly drifting LFO eases in. At 0 the layer is
exact.

This is the highest "analog vibe per cost" item — a few multiply/shift
operations per note and per block, no new voices.

### 2.4 Portamento / glide (EDIT per layer) — PORTA TIME
A `cfg.porta` knob (0 = none, else constant-time glide ms) on the bass and lead
layers, reusing the existing glide logic in `bass_render`/`bank_*`.
Works with RETRIG and LEGATO.

### 2.5 Architectures (EDIT per layer) — SINGLE / DUAL / STACK
A `cfg.arch` row per melodic layer (0 single, 1 dual osc, 2 stack), mapping the
Carnac architecture list to the engines we already have:

| FM-23 arch | Meaning | Reuses |
|-----------|---------|--------|
| SINGLE | one oscillator path (current engine) | everything |
| DUAL OSC | two detuned oscillators, mixed (lead) or cross-mod (bass B02) | classic lead detune, B02 FM |
| STACK | main oscillator + sub octave | sub osc = the existing B01/kick-derived body or a +12 semitone layer |

FULL/BINAURAL/STRIKE/DM SPRD are out of scope (see §3).

### 2.6 Filter flavour (FX page or EDIT) — SH-2 / MS-20
A `cfg.fil_flav` row on the filter section: 0 SH-2-style (the current CF
clean resonance), 1 MS-20-style growl (adds the existing drive/diode-style
nonlinearity with higher resonance). Cheap: we already have the SVF and the
drive/abrasive shaper; this mostly selects routing/gain.

## 3. Explicitly out of scope (and why)

| Carnac feature | Why not |
|----------------|---------|
| 15 wavetable banks + PCM waves | Flash budget; banks live outside the firmware and we have no wave data. |
| 3 mod slots with visual portal | Page budget; one new page max. A *single* mod slot could be a future step, but the visual ring is out. |
| BINAURAL / dual-core load balancing | FM-23 is single-core; stereo-per-ear is a different build. |
| 8 architectures | The 3 we can do cover the musical roles (plain/detuned/stacked). |
| Per-voice poly voice budgets with caps | We have at most a handful of voices (1 bass + 1 lead + 8 drums). No stealing needed yet; if we add poly lead, a simple STEAL-OLDEST policy suffices. |

## 4. UI budget (this is the hardest constraint)

Pages are full (9/9). Options:

- **A (preferred): repurpose HOME's 4th row.** HOME currently has TEMPO / SWING
  / MASTER / PATTERN. Add a sub-edit: when in a *perf mode*, the 4 rows become
  the mode + its knob (SPLIT NOTE / ANALOG / PORTA / VOICE MODE). The `EDIT`
  page remains per-layer.
- **B: a new LAYER page** would require either dropping a page or merging ENV
  and LFO into one page (they are small). Then EDIT becomes the per-layer craft
  page and LAYER holds PERFORM / VOICE / ANALOG / PORTA / ARCH / FILTER FLAV.
  This is the cleanest but touches the page map.

Recommendation: **B** if we accept merging ENV+LFO (one page switching on the
voice selector), else **A** as a no-new-page fallback.

## 5. Data model

Append to `cfg_t` (version-independent fields first, clamped on load):

```c
uint8_t perf;        /* 0 single, 1 dual, 2 split */
uint8_t split_note;  /* 0..127 */
uint8_t voice_mode[2];/* per melodic layer: 0 retrig, 1 legato, 2 unison */
uint8_t analog[2];   /* 0..127 per layer */
uint8_t porta[2];    /* 0..127 per layer (0 none) */
uint8_t arch[2];     /* 0 single, 1 dual osc, 2 stack */
uint8_t fil_flav;    /* 0 SH-2, 1 MS-20 */
```

All clamped in `cfg_clamp()`; saved with the config like the rest.

## 6. Engine touchpoints

- `sound_audio`: key routing for perf modes (SINGLE/DUAL/SPLIT), note split,
  and voice-mode gating.
- `bass_render` / `lead_render` + bank counterparts: legato glide (already),
  unison detune (2nd osc), analog drift injection (per-note seed), sub osc for
  STACK, filter flavour selection.
- `seq_fire`: when in DUAL, a bass step may also fire the lead on the same
  step (or leave to the pattern as-is — decide: DUAL is a *performance* mode,
  patterns stay independent).
- `ui.c`: HOME/EDIT rows, page merge if option B, drawing the split line on the
  key strip (SPLIT), and an ANALOG/voice-mode readout.

## 7. Budget estimate

Measured baseline: `sound_audio` 2436/2772 (12% free).

| Addition | Est. cost | Notes |
|----------|-----------|-------|
| perf-mode key routing | ~30–50 | integer logic, per block, trivial |
| legato (existing glide) | ~0 | already implemented |
| unison (2nd osc) | ~60–120 | only when voice_mode==2; osc math exists |
| analog drift | ~40–80 | per-block LFO + per-note hash |
| porta | ~10–30 | reuses glide; per-note setup |
| sub osc (STACK) | ~40 | one extra osc when arch==2 |
| filter flavour | ~0–40 | routing + an extra drive stage |

Worst case ≈ 350–400 units of the 336 free → **tight**. Mitigations: unison
and STACK are mutually exclusive feature choices; run the benchmark tool
(`tests/bench_render.c`) and the ISR budget (`tests/target_budget.py`) after
each step, and if needed restrict DUAL+UNISON combos or drop the MS-20 flavour
to a macro post-gain.

## 8. Test plan

- Host tests: perf-mode routing (SINGLE/DUAL/SPLIT produce the right voices),
  split-note boundary, legato no-retrig, unison detune produces pitch spread at
  zero drift, analog=0 exact vs analog>0 scatters, porta time, arch stack
  presence of sub octave, clamp on load for every new field.
- Budget: after each milestone run `tests/target_budget.py`; keep every
  function under its target.
- Emulator: extend `emu_test.mjs` to set the new rows and assert the modes
  audibly/visibly.

## 9. Milestones

1. **M1 – performance modes** (HOME row + key routing). Smallest, highest
   musical value: play bass+lead together, split keyboard.
2. **M2 – voice modes + analog drift** (EDIT rows). Analog character without
   new voices.
3. **M3 – porta + architectures** (SINGLE/DUAL/STACK + sub osc).
4. **M4 – filter flavour + page merge** (if option B chosen).
5. **M5 – docs + publish** (this doc updates, UI guide, emulator).

Each milestone keeps all tests green and the budget under target before moving on.

## 10. Open questions for review

1. **UI option A or B?** (no-new-page vs merge ENV+LFO into one page for a
   dedicated LAYER page). I lean B for clarity, but A is less risky.
2. **DUAL semantics**: should both layers follow the same pattern lanes, or is
   DUAL strictly a keyboard-performance mode (sequencer keeps its lanes)?
3. **UNISON scope**: lead-only, or also bass?
4. **Analog drift default**: off (0) or a gentle default (~30) so the box
   "feels" analog on first boot?
5. **Filter flavour**: worth the budget if it costs some high-voice combos?
