# FM-23 bug findings (review)

Findings from a full manual review + build/test run. Status: to be verified, no code changed in this pass.

## High
1. data.c / song_load (around line 335) - missing song-bank pattern calls pat_default(), which clobbers freshly loaded ed_pat and leaves pat_bank[i] zero-initialized. Fix: default into the bank, not ed_pat.
2. bank.c / bank_lead_render (lines 509, 547-594) - inc keeps placeholder bk_note_inc(60) for algos 4,5,6,7, so L07/L03/L04/L06 ignore the triggered note and sound at C4. Fix: set inc = bl.baseinc in each branch.
3. dsp.c / dsp_svf_set and dsp_svf_run - 32-bit overflows; cutoffs above ~0.16*FS non-monotonically wrap. Fix: promote products to int64, clamp.
4. tests/app_test.c - no longer builds: peak used at line ~348 before static definition at ~423. Fix: forward declare.

## Medium
5. sound.c / lead_release - ARP HOLD only honoured in bank path; classic lead always releases; toggling HOLD off while latched never releases. Fix: add classic branch and release edge.
6. theory.c / th_vary - seeded melodic variation is dead because it runs before p->bass / p->lead filled. Fix: move call after fill.
7. ui.c - seq_part is never set on any UI path, so the song-chain editor is unreachable by the user.
8. seq.c / swing - step length rewritten mid-block, potential skip. Validate.

## Low / UX
9. GEN page trigger (user request): "in gen mode i cannot know when the engine really generated a sequence because it has no trigger, just turning of knobs. its not a good user experience". UX feedback relies only on the gen_done flag on the GEN page.

## Web
10. web/emu/index.html - guide text was var(--text), which is undefined (black on black). Fixed this pass to var(--fg).

## Checks performed
- ./build.sh passes; RAM 12340/51200
- tests/run_tests.sh all listed tests passed; 375 ok; emulator 406x real time
- ASan/UBSan attempted, blocked by compile issue #4
- Read-through of all app/*.c modules

## Not verified
- Listening validation of build/audition/*.wav (human only)
- Concurrency stress of ed_pat/pat_bank copies in ISR
- Target timing of the swing refactor
