# FM-23

A self-contained mental tekno / tribe groovebox for the FM-1: 303 acid bass, Carnac/Moog-style lead and drones, synthesized tribal percussion, and the SANTO CENTER pattern theory in one instrument.

Custom firmware for the M-VAVE FM-1, made from the FM-1 Firmware Kit: Felucca's platform (GPL-3.0-only, via
M-Vave-OP1) in `firmware/src/`, `hal/` and `loader/`, this firmware in `firmware/app/`. What it does and how it plays:
`docs/SPEC.md` (keep it current); how the repository was set up: `docs/PROJECT.json`.

- **Any firmware work:** load the `fm1-platform` skill (`.claude/skills/fm1-platform/SKILL.md`) first: it holds the
  app contract, the budgets, the test loop and the safety rules for installing on a real FM-1.
- While `firmware/app/` still holds the kit's start app (a name screen, a triangle wave, SAVE keeps the knobs), the
  repository's setup is not finished: continue the `fm1-new-project` skill at its step 5 (the skeleton from
  docs/SPEC.md).

Code, comments and docs in the repository are in English; talk to the user in their language.
