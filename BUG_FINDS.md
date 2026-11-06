# FM-23 bug findings (review + fixes)

Findings from a full review, with status. Fixes landed in these commits:
- `d45ee5b` — UB repairs + test-harness fixes (see below).
- `2d6468b` — 512-entry sine table, 32-bit DC HP, ARP HOLD classic path,
  SEQ PART latch, song-bank default fix, bank lead pitch fix, seeded
  variation now live.

## Fixed (confirmed)
1. **tests/app_test.c — did not compile** (`peak` used before its static
   definition). The suite was silently running a stale binary. Fixed with a
   forward declaration. This was the root cause of the apparent "bank test
   failures": they were artifacts of a stale binary plus test windows that
   read past the freshly rendered buffer.
2. **tests** measured stale memory: `host_render` overwrites the buffer from
   index 0, but the "ends silent"/HOLD checks looked at offsets beyond the
   just-rendered region (e.g., frame 2047 of a 2000-frame render). Real
   engine behavior was fine. Checks now use only freshly written windows.
3. **dsp_sin** — signed-overflow UB in the interpolation product
   (`(b-a)*frac` up to ~1.1e12). Now a 512-entry table with a 16-bit
   fraction; product fits 32-bit and precision improves.
4. **bk_note_inc** — left shift of a negative value (UB). Now a multiply.
5. **SVF** — `fc*411774`, `damp*band`, `f*high`, `f*band` all overflowed
   int32; cutoffs above ~0.16·FS wrapped non-monotonically. Products are
   int64 and integrator states are clamped to ±2^17 (was runaway resonance
   at high Q, previously hidden by the overflow wrap).
6. **DC-removal one-pole** — `y*65477` overflow. Now `y - (y>>10)` in
   32-bit (same pole, well-defined).
7. **bank.c lead seeds L07/L03/L04/L06** — used the placeholder
   `bk_note_inc(60)`; all sounded at C4. Now use the triggered note.
8. **data.c song_load** — a missing song-bank cell called `pat_default()`,
   clobbering the active pattern. Now defaults empty.
9. **theory.c** — seeded variation ran before the melodic fill, so seeds
   only wiggled hat velocities. Now runs on the finished cell.
10. **ARP HOLD with the classic lead** — only the bank path honored HOLD;
    toggling HOLD off never released a latched drone. Classic voice now
    latches too; force-cut (PLAY-stop/panic) is an instant mute for both.
11. **SEQ PART (song-chain editor) unreachable** — `seq_part` was never
    settable from the UI. LANE now runs through 6 and latches PART; the
    PART row toggles back to 0 to exit.
12. **Emulator guide text** — `var(--text)` was undefined (black on black).
    Now `var(--fg)`.

## Open (to verify / UX)
- **GEN page feedback** — user report: "in gen mode i cannot know when the
  engine really generated a sequence because it has no trigger, just turning
  of knobs. its not a good user experience." Currently regeneration is
  implicit on any knob change; only the GEN page shows a status line. A
  dedicated trigger/lock or a visible "generated" pulse could address this.
- **Swing** — the timing refactor (offbeat shift) is validated on host, not
  measured on target silicon with the ISR meter.
- **Concurrency** — no stress test for tearing of `ed_pat`/`pat_bank`
  while the ISR swaps patterns (single 32-bit copy is atomic by
  word-size, but a tear across the copy is not exercised).
- **Listening validation** of `build/audition/*.wav` remains human-only.

## Checks performed
- `./build.sh` green; RAM 12340/51200.
- `tests/run_tests.sh` green, including the ASan/UBSan app run (no runtime
  errors) and the ISR budget (sound_audio 2404 vs 2772, −13%).
- Emulator build + node smoke tests, 406× realtime.
