/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* Host audition renderer for the recipe bank (docs/SOUND_PLAN.md Phase 3). Uses the exact DSP
 * code the firmware runs (host_platform includes firmware/app/app.c) and writes dry isolated hits
 * as WAV to build/audition/. Not a validator; provides material for human listening.
 *   cc -O1 -Ibuild/gen -Ifirmware/src -Ifirmware/app -Itests -o build/host/render_bank \
 *       tests/render_bank.c -lm
 *   build/host/render_bank */
#include "host_platform.h"
#include <stdio.h>

#define BUF_FRAMES (2u * FS * 3u)
static int32_t abuf[BUF_FRAMES];

static void wav16(const char *path, const int32_t *b, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    uint32_t data = frames * 4u, i;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f);
    fwrite(&(uint32_t){ 36 + data }, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&(uint32_t){ 16 }, 4, 1, f);
    fwrite(&(uint16_t){ 1 }, 2, 1, f);              /* PCM */
    fwrite(&(uint16_t){ 2 }, 2, 1, f);              /* stereo */
    fwrite(&(uint32_t){ FS }, 4, 1, f);
    fwrite(&(uint32_t){ FS * 4u }, 4, 1, f);
    fwrite(&(uint16_t){ 4 }, 2, 1, f);
    fwrite(&(uint16_t){ 16 }, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data, 4, 1, f);
    for (i = 0; i < frames; i++) {
        int16_t s = (int16_t)b[2u * i];            /* Q15 == PCM16 (host render is Q15) */
        fwrite(&s, 2, 1, f);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

static void voice(const char *name, uint8_t kind, uint8_t vel, uint32_t ms)
{
    uint32_t frames = FS * ms / 1000u;
    uint32_t i;
    if (frames > BUF_FRAMES / 2u)
        frames = BUF_FRAMES / 2u;
    for (i = 0; i < frames; i++)
        abuf[2u * i] = abuf[2u * i + 1u] = 0;
    drum_trigger(kind, vel);
    host_render(abuf, frames);
    {
        char path[128];
        int32_t pk = 0;
        uint32_t q;
        for (q = 0; q < 2u * frames; q++) {
            int32_t v = abuf[q];
            if (v < 0) v = -v;
            if (v > pk) pk = v;
        }
        snprintf(path, sizeof path, "build/audition/%s.wav", name);
        wav16(path, abuf, frames);
        printf("wrote %-28s peak_q15=%d\n", path, pk);
    }
}

static void note_seed(const char *name, int is_bass, uint8_t algo, uint8_t note, uint32_t ms)
{
    uint32_t frames = FS * ms / 1000u;
    uint32_t i;
    if (frames > BUF_FRAMES / 2u)
        frames = BUF_FRAMES / 2u;
    for (i = 0; i < frames; i++)
        abuf[2u * i] = abuf[2u * i + 1u] = 0;
    if (is_bass) {
        cfg.bass_algo = algo;
        bass_note(note, 105, 0);
    } else {
        cfg.lead_algo = algo;
        lead_note(note, 105);
    }
    host_render(abuf, frames);
    {
        char path[128];
        int32_t pk = 0;
        uint32_t q;
        for (q = 0; q < 2u * frames; q++) {
            int32_t v = abuf[q];
            if (v < 0) v = -v;
            if (v > pk) pk = v;
        }
        snprintf(path, sizeof path, "build/audition/%s.wav", name);
        wav16(path, abuf, frames);
        printf("wrote %-28s peak_q15=%d\n", path, pk);
    }
    if (is_bass)
        bass_release();
    else
        lead_release();
    host_render(abuf, 1024);
}

int main(void)
{
    if (system("mkdir -p build/audition"))
        return 1;
    host_boot();
    cfg.kit = 1;                     /* the recipe bank */
    cfg.fx_delay = 0;
    cfg.fx_reverb = 0;
    voice("kick_dry_tribe", 0, 100, 400);
    cfg.kit = 2;
    voice("kick_elastic_mental", 0, 100, 500);
    cfg.kit = 3;
    voice("zap_whistle_hardtekk", 7, 110, 300);
    cfg.kit = 1;
    voice("tom_bongo", 5, 100, 400);
    voice("fm_metallic", 4, 110, 400);
    voice("snare_synth", 1, 110, 300);
    voice("hat_closed", 2, 110, 200);
    voice("hat_open", 3, 110, 500);
    voice("noise_texture", 6, 100, 600);

    /* melodic seeds (Phase 2b) */
    note_seed("bass_b01_kickderived", 1, 1, 38, 500);
    note_seed("bass_b02_rubber_fm", 1, 2, 38, 500);
    note_seed("bass_b03_tribe_saw", 1, 3, 38, 500);
    note_seed("bass_b04_acid", 1, 4, 38, 500);
    note_seed("lead_l01_hollow", 0, 1, 60, 600);
    note_seed("lead_l02_snake", 0, 2, 60, 600);
    note_seed("lead_l05_growl", 0, 3, 60, 600);
    note_seed("lead_l07_alien_ping", 0, 4, 60, 700);
    note_seed("lead_l03_nasal", 0, 5, 60, 600);
    note_seed("lead_l04_sync", 0, 6, 60, 600);
    note_seed("lead_l06_scream", 0, 7, 60, 600);
    cfg.bass_algo = 0;
    cfg.lead_algo = 0;
    return 0;
}
