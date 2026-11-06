# Sound-bank change log

Records what was implemented, tuned, and the evidence class of each decision. Renders for human
listening: `build/audition/*.wav` (regenerate with `build/host/render_bank`).

## Phase 1 — DSP core (`firmware/app/dsp.c`)
- Sine from a 256-entry runtime-filled table + interp (no FPU). PROPOSED implementation; table
  filled with an integer CORDIC-style recurrence at boot.
- Resonant SVF (LP/BP/HP) with bounded Q. PROPOSED.
- Envelopes in the mission's semantics: `tauP` pitch (exp), `D60` amplitude (exp), linear A/R.
- Saturation: soft and abrasive; gain-compensated by a drive LUT.
- DC removal: proper ~7 Hz one-pole HP (the initial crude differentiator gutted the kick bodies —
  fixed, see below).

## Phase 2 — drum kit bank (`firmware/app/bank.c`)
Seeds implemented (kit selectable via `cfg.kit`; 0 = original engine untouched):
| Seed | Kind | Status |
|---|---|---|
| D01 dry tribe kick | kick (kit 1) | implemented, body boosted |
| D02 elastic mental kick | kick (kit 2, + resonant texture branch) | implemented |
| D04 hardtekk whistle | zap | implemented |
| D05 tom/bongo | str | implemented |
| D06 FM metallic | met (C:M 19/7 ~ 2.71) | implemented |
| D07 synthetic snare | snare | implemented |
| D08 hats closed/open | hat/open | implemented |
| D09-style noise texture | noise | implemented |

## Deviations and why
- **C:M ratios** use small integer fractions (19/7, etc.) instead of exact irrationals to keep the
  32-bit math safe on the target (no 64-bit divide helpers → no ROM references). PROPOSED.
- The initial DC blocker was an aggressive differentiator that removed the low-frequency body of
  kicks/toms; replaced with the ~7 Hz one-pole HP. Measured improvement: dry kick peak 322 →
  9,581 (Q15), with no audible DC.
- Gain staging: noise voices (hats/snare/texture) are output at half gain to avoid clipping; the
  dry kick body is boosted 4× (still predistortion, well within ±32767).

## Measured results (host renders, Q15 peaks)
- kick dry tribe 9,581 · kick elastic mental 24,073 · whistle 17,044 · tom 3,851
- FM metallic 4,583 · snare 17,557 · hat closed 21,112 · hat open 20,361 · noise 19,001
- All finite; no clipping; DC ~0; mono-identical (single bus).

## Evidence classes
- D01/D02 pitch-envelope + body concept: INFERRED from genre discussions (listed) + PROPOSED numbers.
- D04 whistle: PROPOSED.
- D06 PM metal: PROPOSED ratios (not claimed as any historic ratio).
- D08: PROPOSED (listed frequency seed used only as exploration).
- Korg volca kick (self-oscillating-filter kick): DOCUMENTED reference; we use a resonator-style
  approach only as inspiration, not a circuit clone.

## Phase 2b — melodic seeds (implemented, initial tuning)
- Bass: B01 kick-derived, B02 rubber FM (C:M 1:2), B03 tribe saw/pulse (LP + drive).
- Leads: L01 hollow (two pulse oscs), L02 snake (sine + PM + delayed vibrato), L05 growl
  (saw → resonant BP sweep, bounded), L07 alien ping (FM ratio 19/7).
- All melodic outputs run through a ±20k limiter before the bus (no clipping); Q lowered where
  the SVF resonated into full scale.
- Selectable via EDIT → BASS/LEAD → ALGO (0 = classic engine).

## TD-3-complementary sequencer lanes
- `pattern_t` now carries SLIDE (0/1) and GATE (1..4 steps, ties) lanes per melodic step.
- GEN writes bass slide flags per style (mental b2 slides on 3/11, etc.) and arms cross-pattern
  legato on every song-cell boundary (first bass step slides from the previous cell's note).
- The bass engine implements the slide as legato glide (phase-continuous) — across patterns too,
  which the real TD-3 cannot do in Track mode (see SANTO track-chain doc).

## Unverified / remaining
- B04 acid-adjacent bass, L03 nasal reed, L04 sync razor, L06 scream: not implemented yet.
- Automation/parameter locks, swing, oversampling: not implemented; documented as Phase 4.
- No listening validation by machine; auditions are for human review.

## Hardware safety
- No changes to loader, boot guard, update path, flash layout, or recovery.
- Audio output remains bounded (stress tests at max resonance/drive/feedback pending for the later
  lead voices; drum bank verified finite).
