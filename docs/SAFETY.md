# FM-23 Safety & Install

The FM-1 has **one firmware bank and no recovery button**. A wrong flash can leave the unit
bricked until it is recovered with extra hardware. This document is the contract that keeps an
install safe; read it before the first install.

## The platform's safety net (kept, never weakened)

- **Boot guard** — two boots in a row that die in their first 30 s send the unit to the chip's
  boot mode (`WL80UBOOT`) with USB up, so it can be reflashed.
- **OCT− + OCT+ held 5 s** — while the firmware runs, enters the same boot mode (recovery).
  FM-23 never reuses this gesture.
- **The update entry** — the running app always accepts a new package over USB-MIDI, so a bad
  app can be replaced even after it starts (as long as the main loop keeps returning every frame).
- **The watchdog** — resets a hung main loop (~8 s).
- **Return to official V15** — the web installer offers it, as does
  `python3 tools/fm1_install.py --stock` and M-VAVE's own updater.
- **Package identity** — `FM-1_900` and the USB product name are untouched; the installers find
  the unit by them and refuse to write a foreign package.

FM-23 must never disable or bypass any of these, and never touch `firmware/loader/`, the boot
guard, the UBOOT path, or the update protocol.

## The working order (never the reverse)

1. **Host tests** — `tests/run_tests.sh` must end in `ALL HOST TESTS PASSED` (this includes the
   update loader, the install protocol and the installer page tests).
2. **Browser emulator** — the app is auditioned in `web/emu` before it is offered to hardware.
3. **Careful install** — only after 1 and 2 are green, and with the user's explicit go-ahead.

## First install (the risky one)

- Have the **recovery hardware at hand**: a [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter)
  (Seeed XIAO RP2040 on the FM-1's USB lines) reads/writes the flash from boot mode.
- **Back up the stock flash first** (the transporter, or save `firmware/FM-1_015.fwsc` from the
  installer site).
- **Coming from another custom firmware (Felucca, Baud Girl, Carnac, AMARU, ...)? Return to
  official V15 first**, then install FM-23. Writing straight over another custom firmware is the
  most common cause of an FM-1 that no longer answers.
- While FM-23 has never booted on your unit, treat the install as possibly final: keep the stock
  image and the recovery path verified.

## Installing

- **Web installer** — open `webapp/installer/` on the published site (or locally:
  `python3 web/make_site.py build/firmware.fwsc dev build/site`, serve `build/site`, open
  `http://localhost:8000/webapp/installer/`) in Chrome/Edge with the FM-1 connected by a data USB
  cable. Use "Return to official V15" for the way back.
- **Offline** — `python3 tools/fm1_install.py build/firmware.fwsc` (needs `mido` +
  `python-rtmidi`; `--stock` restores official V15).
- Do not unplug during a write; if interrupted, replug and install again (the update resumes from
  the start).
