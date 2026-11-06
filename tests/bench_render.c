/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* Host render benchmark for the audio path (same DSP code as the firmware). Measures the worst
 * typical configuration: the recipe bank with full kit + the heaviest bass/lead seeds, transport
 * running. Reports us per 32-frame block versus the 2900 us hardware budget.
 *   cc -O1 -Ibuild/gen -Ifirmware/src -Ifirmware/app -Itests -o build/host/bench_render \
 *       tests/bench_render.c -lm
 *   build/host/bench_render */
#include "host_platform.h"
#include <time.h>
#include <stdio.h>

static int32_t abuf[2u * 1024u];

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(void)
{
    uint32_t i, blocks, total = FS * 10u;   /* 10 s of audio */
    double t0, t1, secs;
    host_boot();
    /* the heaviest realistic scene */
    cfg.kit = 3;
    cfg.bass_algo = 4;
    cfg.lead_algo = 6;
    cfg.fx_delay = 127;
    cfg.fx_reverb = 127;
    cfg.fx_drive = 127;
    cfg.song_on = 0;
    seq_toggle_play();
    host_render(abuf, CTL);
    t0 = now_s();
    for (i = 0; i < total; i += 1024u)
        host_render(abuf, 1024u);
    t1 = now_s();
    seq_toggle_play();
    host_render(abuf, CTL);
    secs = t1 - t0;
    blocks = total / CTL;
    printf("bench: %u frames in %.3f s -> %.1f us/block (32 frames), realtime x%.1f\n",
           total, secs, secs * 1e6 / blocks, (double)total / FS / secs);
    printf("bench: hardware budget 2900 us/block; headroom %.0fx\n",
           (2900.0 * blocks) / (secs * 1e6));
    return 0;
}
