/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* FM-23: a self-contained mental tekno / tribe groovebox for the M-VAVE FM-1. The app's five
 * hooks and the includes of its parts: data.c (settings + the active pattern, the stored records),
 * sound.c (the 303-style acid bass, the lead/drone synth, the synthesized percussion), seq.c (the
 * 16-step pattern sequencer, in the audio ISR) and ui.c (the pages and the input). Keep the
 * contract in app.h; the platform is in firmware/src/. */

#ifndef APP_NAME
#define APP_NAME "FM23"
#endif
#ifndef APP_VERSION
#define APP_VERSION "v0.1"       /* build.py --release X.Y */
#endif
#define APP_ID 0x1A2B3C4Du       /* this app's records in the store (store.c) */

#include "data.c"
#include "sound.c"
#include "seq.c"
#include "gen.c"
#include "ui.c"

static void app_init(void)
{
    data_init();
    sound_init();
    palette_set(8u);                 /* NIGHT: the vintage phosphor monochrome look */
    ui_rows_fill();
}

static void app_frame(void)
{
    ui_frame();
}

static __attribute__((noinline)) void app_audio(int32_t *out, uint32_t n)
{
    seq_take();                  /* pattern swap, play/rec requests (main loop -> ISR) */
    sound_audio(out, n);
    seq_clock(n);                /* the step clock: fires the pattern's steps */
}

static void app_audio_stop(void) { sound_stop(); }

static const char *app_fault_name(void) { return APP_NAME " CRASH"; }
