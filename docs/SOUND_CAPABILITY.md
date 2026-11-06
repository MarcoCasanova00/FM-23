# Capability audit (FM-23, branch `main`)

Audited from the checked-out repository (commit `69f6f37`, branch `main`). This is the actual
platform the sound bank must live on; every statement below is verified in the code/build output.

## Identity and hardware
- Firmware: **FM-23** (`APP_NAME "FM23"`, v0.1, app id `0x1A2B3C4D`), built from the FM-1 Firmware Kit.
- Target: **M-VAVE FM-1**, JieLi AC791N / WL82, pi32v2 core. No audio input.
- Toolchain: JieLi clang for pi32v2; `./build.sh` (build/firmware.fwsc, identity `FM-1_900`).

## Audio path
- Sample rate **FS = 44100**; audio block **CTL = 32 frames**, 4 calls per 128-frame half buffer (2.9 ms).
- `app_audio(int32_t *out, uint32_t n)` renders stereo Q15 (out[2i] L, out[2i+1] R); the platform shifts to 24-bit at -6 dBFS.
- Main loop separate; audio ISR constraints: O(1)/block, no drawing/flash/wait, rare paths `noinline`.

## What exists today (in `firmware/app/`)
- Oscillators: **triangle/saw from a phase accumulator** only (no sine/PM/pulse/sync, no wavetable).
- Filters: **one-pole low-pass** (coefficient from cutoff 0..127) per voice. No resonance/BP/HP provision.
- Drive: piecewise-soft clipper (pre-gain into bounded clip, blended by drive amount).
- Effects: a mono **delay** and a **reverb** (feedback buffers in `.pool`, sends by cfg) on the mix bus.
- Modulation: only a simple delayed LFO vibrato on the lead; no routing matrix, no parameter locks.
- Envelopes: linear ramps to a set target (attack/release), exponential *decay by repeated division* on drums. No pitch envelopes, no time-constant semantics.
- Voices: 1 mono bass, 1 lead (2 detuned oscs), 8 mono drum slots (mixed, no per-voice filtering).
- Sequencer: 16-step x 6-lane patterns, per-step velocity 0..127 (edit cycles 90/110/127), song chain, MIDI clock in.
- Persistence: store slots (4 KB, CRC'd): slot 0 settings, slot 1 active pattern, slots 2..9 song bank, slot 10 chain.

## Budgets (measured, from `tests/target_budget.txt` + latest build)
- Flash: image ~69 KB (limit ~580 KB). RAM `.data+.bss`: ~6.3 KB of 51,200. Pool: ~68 KB used of 391,168 (FX buffers).
- Audio ISR cost: `fm1_alnk0_irq` = 24 (budget 24); engines: bass_render 51, lead_render 80, drum_render 554, sound_audio 233, app_audio 3, seq_clock 37.

## Tests / rendering
- Host tests (`tests/run_tests.sh`): app, store, input, MIDI, loader, OTA, installer, budgets, ASan/UBSan.
- Emulator: `web/emu` (wasm + AudioWorklet) with `emu_test.mjs`; ~330-420x realtime.
- Existing rendering: PNG screenshots only; **no audio render tool yet** (needed for audition WAVs).

## Licensing
- GPL-3.0-only (Felucca-derived); keep headers; new code copyright MarcoCasanova00 + M-Vave-OP1 contributors.

## Gaps this mission must fill
1. Sine + two-operator PM/FM oscillators (with independent amp/index envelopes).
2. Resonant state-variable filter (LP/BP/HP + stable Q).
3. Pitch envelope with `tauP` semantics and exponential amplitude decay with `D60` semantics.
4. A small modulation matrix / parameter smoothing.
5. A voice architecture separating LOW-FOUNDATION / CHARACTER / SPACE.
6. A host-side WAV render tool using the same DSP code.
7. Per-voice gain staging and stress tests (finite output, DC, resets, retriggers).

No bootloader, flash layout, update protocol, or recovery code was touched by this mission; those remain as the shipped kit (see `docs/SAFETY.md`).
