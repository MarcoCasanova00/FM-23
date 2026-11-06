/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* bank.c - the recipe-driven drum engine (Phase 2 of docs/SOUND_PLAN.md). Used when cfg.kit > 0;
 * cfg.kit == 0 keeps the original engine untouched. Seeds: D01/D02 kick (by kit), D04 whistle,
 * D05 tom, D06 FM metallic, D07 snare, D08 hats (closed/open), D09-style noise texture.
 * Recipes are PROPOSED starting points (docs/SOUND_EVIDENCE.md). Fixed point on dsp.c. */

#define BK_N 8u
#define BK_KICK 0u
#define BK_SNARE 1u
#define BK_HAT 2u
#define BK_OPEN 3u
#define BK_MET 4u
#define BK_STR 5u
#define BK_NOISE 6u
#define BK_ZAP 7u

typedef struct {
    uint32_t ph_c, inc0;        /* carrier phase, base increment (Q30) */
    int32_t body;               /* Q15 amplitude envelope */
    int32_t body_step;          /* Q16 multiplier */
    int32_t st;                 /* pitch deviation, Q8 */
    int32_t st_step;            /* Q16 multiplier */
    uint32_t ph_m, inc_m;       /* modulator (FM / noise texture) */
    int32_t idx;                /* PM index, Q14 */
    int32_t idx_step;
    dsp_svf svf;
    dsp_dc dc;
    uint8_t kind, alive;
} bk_drum;

static bk_drum bk[BK_N];
static uint32_t bk_noise = 0x12345678u;

static int32_t bk_noise_next(void)
{
    bk_noise ^= bk_noise << 13u;
    bk_noise ^= bk_noise >> 17u;
    bk_noise ^= bk_noise << 5u;
    return (int32_t)(bk_noise >> 16) - 32768;
}

#define BK_PH_PER_HZ 97391u        /* ~ 2^32 / FS, 32-bit (FS = 44100) */

static uint32_t bk_hz_inc(int32_t hz)
{
    if (hz < 1)
        hz = 1;
    return (uint32_t)hz * BK_PH_PER_HZ;
}

/* C:M ratio as a small integer fraction (num <= 19 so the 32-bit product stays safe) */
static uint32_t bk_ratio_inc(int32_t hz, uint32_t num, uint32_t den)
{
    uint32_t base = (uint32_t)hz * BK_PH_PER_HZ;
    if (den == 0)
        den = 1;
    return (base * num) / den;
}

static void bk_trigger(uint8_t i, uint8_t kind, uint8_t vel)
{
    bk_drum *d = &bk[i];
    d->kind = kind;
    d->alive = 1;
    d->ph_c = d->ph_m = 0;
    d->dc.l = d->dc.y = 0;
    d->st = 0;
    d->idx = 0;
    dsp_svf_set(&d->svf, 0, 6);
    switch (kind) {
    case BK_KICK:                    /* D01 (kit 1) / D02 elastic (kit 2) */
        d->inc0 = bk_hz_inc(54);
        d->st = (cfg.kit == 2u ? 40 : 36) << 8;
        d->st_step = dsp_env_pitch_step_q16(cfg.kit == 2u ? 16 : 8);
        d->body_step = dsp_env_dec_step_q16(cfg.kit == 2u ? 260 : 170);
        d->body = 32767;
        if (cfg.kit == 2u)
            dsp_svf_set(&d->svf, 8192, 10);      /* elastic texture branch */
        break;
    case BK_SNARE:                   /* D07 synthetic snare */
        d->inc0 = bk_hz_inc(200);
        d->body_step = dsp_env_dec_step_q16(60);
        d->body = 32767;
        dsp_svf_set(&d->svf, 16384, 16);
        d->idx = 32767;
        d->idx_step = dsp_env_dec_step_q16(110);
        break;
    case BK_HAT:                     /* D08 closed hat */
        d->body_step = dsp_env_dec_step_q16(35);
        d->body = 32767;
        dsp_svf_set(&d->svf, 29491, 20);
        d->idx = 32767;
        d->idx_step = dsp_env_dec_step_q16(35);
        break;
    case BK_OPEN:                    /* D08 open hat */
        d->body_step = dsp_env_dec_step_q16(200);
        d->body = 32767;
        dsp_svf_set(&d->svf, 29491, 20);
        d->idx = 32767;
        d->idx_step = dsp_env_dec_step_q16(200);
        break;
    case BK_MET:                     /* D06 FM metallic (C:M 1:2.73 ~ 19/7) */
        d->inc0 = bk_hz_inc(330);
        d->inc_m = bk_ratio_inc(330, 19, 7);
        d->body_step = dsp_env_dec_step_q16(90);
        d->body = 32767;
        d->idx = 3 * 16384;          /* ~3 rad */
        d->idx_step = dsp_env_dec_step_q16(50);
        break;
    case BK_STR:                     /* D05 tom/bongo (damped modes) */
        d->inc0 = bk_hz_inc(140);
        d->st = 12 << 8;
        d->st_step = dsp_env_pitch_step_q16(12);
        d->body_step = dsp_env_dec_step_q16(180);
        d->body = 32767;
        break;
    case BK_NOISE:                   /* noise texture */
        d->body_step = dsp_env_dec_step_q16(400);
        d->body = 32767;
        dsp_svf_set(&d->svf, 8192, 10);
        d->idx = 32767;
        d->idx_step = dsp_env_dec_step_q16(400);
        break;
    case BK_ZAP:                     /* D04 whistle (hardtekk) */
        d->inc0 = bk_hz_inc(1400);
        d->body_step = dsp_env_dec_step_q16(80);
        d->body = 32767;
        dsp_svf_set(&d->svf, 24576, 12);
        d->idx = 32767;
        d->idx_step = dsp_env_dec_step_q16(80);
        break;
    }
    d->body = d->body * (96u + vel) / 224u;
}

static void bk_render(int32_t *out, uint32_t n)
{
    uint32_t i, k;
    for (k = 0; k < BK_N; k++) {
        bk_drum *d = &bk[k];
        if (!d->alive)
            continue;
        for (i = 0; i < n; i++) {
            int32_t s = 0, lp = 0, bp = 0, hp = 0;
            uint8_t kind = d->kind;
            if (d->body < 4) {
                d->alive = 0;
                break;
            }
            d->body = (int32_t)(((int64_t)d->body * d->body_step) >> 16);
            if (kind == BK_KICK || kind == BK_STR) {
                int32_t mult;
                uint32_t inc;
                d->st = (int32_t)(((int64_t)d->st * d->st_step) >> 16);
                mult = (int32_t)dsp_semis_mult(d->st);
                inc = (uint32_t)((int64_t)d->inc0 * mult >> 14);
                d->ph_c += inc;
                s = dsp_sin(d->ph_c);
                if (kind == BK_KICK && cfg.kit == 2u) {
                    dsp_svf_run(&d->svf, s, &lp, &bp, &hp);
                    s = s + (bp >> 2);          /* enveloped resonant texture branch */
                    s = (int32_t)((int64_t)dsp_soft(s) * d->body >> 15);
                } else if (kind == BK_KICK) {
                    s = (int32_t)((int64_t)dsp_soft(s) * d->body >> 13);  /* dry body boosted */
                } else {
                    s = (int32_t)((int64_t)dsp_soft(s) * d->body >> 15);
                }
            } else if (kind == BK_MET) {
                int32_t m, idx;
                d->idx = (int32_t)(((int64_t)d->idx * d->idx_step) >> 16);
                idx = d->idx;
                d->ph_c += d->inc0;
                d->ph_m += d->inc_m;
                m = (int32_t)((int64_t)dsp_sin(d->ph_m) * idx >> 14);
                s = dsp_sin(d->ph_c + (uint32_t)m);
                s = (int32_t)((int64_t)s * d->body >> 15);
            } else if (kind == BK_HAT || kind == BK_OPEN || kind == BK_NOISE) {
                dsp_svf_run(&d->svf, bk_noise_next(), &lp, &bp, &hp);
                s = (int32_t)((int64_t)hp * d->body >> 15);
            } else if (kind == BK_SNARE) {
                dsp_svf_run(&d->svf, bk_noise_next(), &lp, &bp, &hp);
                d->ph_c += d->inc0;
                s = (int32_t)((int64_t)dsp_sin(d->ph_c) * d->body >> 15) + (hp >> 1);
            } else {                            /* BK_ZAP whistle */
                dsp_svf_run(&d->svf, dsp_sin(d->ph_c), &lp, &bp, &hp);
                d->ph_c += d->inc0;
                s = (int32_t)((int64_t)bp * d->body >> 15);
            }
            dsp_dc_run(&d->dc, s, &s);
            if (kind == BK_HAT || kind == BK_OPEN || kind == BK_NOISE || kind == BK_SNARE)
                out[i] += s >> 2;                /* the noise voices sit hotter: keep headroom */
            else
                out[i] += s >> 1;
        }
    }
}
