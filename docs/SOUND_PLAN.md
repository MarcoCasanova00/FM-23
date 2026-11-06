# Implementation plan (sound bank)

Based on `docs/SOUND_CAPABILITY.md`. The bank is built from new, modular DSP primitives; the
existing voice code is kept as the baseline (kit/algo 0) so nothing regresses.

## Phase 1 — DSP core (`firmware/app/dsp.c`)
| Block | Implementation | Cost/notes |
|---|---|---|
| Sine | 256-entry quarter-wave table + linear interpolation, phase accumulator | ~2 cycles/sample; used for body, PM, resonance |
| Triangle/saw/pulse | existing phase-based, + band-limited pulse via polyBLEP-style edge correction (justified: bright sources) | saw kept naive for bass; documented alias risk |
| Two-op PM | `sin(carrier + indexEnv(t)·sin(modulator))`, phases advanced from instantaneous freq | index & amp envelopes independent |
| Resonant SVF | LP/BP/HP outputs, per-sample update, Q mapped 0.5..25, bounded | 1 filter per voice (reuse per branch) |
| Envelopes | linear A/R; exponential pitch `tauP`; exponential amp `D60` (integer staircase: g -= g>>k with k from D60) | no FPU in hot path |
| Saturation | soft `x - x³/3` and abrasive (piecewise + fold) with input-gain/output-compensation | parameterized |
| DC blocker | one-pole HP ~20 Hz on voice output | safety |
| Feedback texture | short (2–15 ms) filtered delay with low feedback, saturator in-loop, bounded | optional voice |

Oversampling is **not** enabled initially; benchmarks will decide (Phase 4).

## Phase 2 — first seeds (representative set)
Drums (new kit engine, `bank.c`, replaces the naive engine when `cfg.kit > 0`):
- D01 dry tribe kick, D02 elastic mental kick, D04 hardtekk punch/whistle, D05 tom/bongo,
  D06 FM metallic perc, D07 snare, D08 hats (closed/open), D09 clap/rim.
Bass (`cfg.bass_algo`): B01 kick-derived offbeat, B02 rubber FM (C:M 1:2 / 1:1 / 1:3).
Leads (`cfg.lead_algo`): L01 hollow, L02 snake/flute, L05 growl (bounded feedback), L07 alien ping.

## Phase 3 — audition + test harness
- Host WAV render tool (`tests/render_bank.c`) using the same DSP code, writing
  `build/audition/<voice>.wav` (dry hits, velocities, quick patterns).
- DSP tests: finiteness, peak headroom, DC ≈ 0, reset/mute silence, fast retriggers,
  macro extremes (resonance/drive/feedback stress), mono identity.
- Budget check: add new render functions to `tests/target_budget.txt`.

## Phase 4 — tuning & documentation
- Tune recipes in firmware; record deviations in `docs/SOUND_LOG.md` (with evidence class).
- Enable oversampling only where it measurably helps and stays in budget.
- Emulator + firmware build, publish site.

## Trade-offs
- Sine table: ~1 KB flash, big quality win for body/PM.
- SVF per voice: more ops than today's one-pole, but bounded and reused per voice; total ISR cost target < ~1200 (budget after each step).
- Feedback growl: kept per-voice with hard bounds + reset path; never let limiter be the primary stability mechanism.
- FM bass/lead: 2 oscillators + envelopes ≈ 2× current lead cost; acceptable at current voice counts (1 bass, 1 lead).
- No stereo widening; all voices validated in mono first.

## Safety
No bootloader/flash/update/recovery changes. Voice gains staged to keep peak mix < ±32767; stress tests at max drive/resonance/feedback.
