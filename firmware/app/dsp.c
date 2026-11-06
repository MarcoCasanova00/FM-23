/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* dsp.c - the sound bank's primitives (Phase 1 of docs/SOUND_PLAN.md). All fixed point, no FPU
 * in the audio path, no dynamic allocation. Tables are filled once at boot (app_init -> dsp_init)
 * with integer recurrences, so the ISR only reads them.
 *
 * Semantics (per docs/SOUND_MISSION.md section 4):
 *  - pitch envelope: st(t) = st0 * exp(-t / tauP),  frequency(t) = final * 2^(st(t)/12).
 *  - amplitude decay: amplitude(t) = exp(-ln(1000) * t / D60)  ("D60").
 *  - PM:  out = ampEnv(t) * sin(carrierPhase + indexEnv(t) * sin(modulatorPhase)).
 *
 * Anti-aliasing: pulse uses an edge-correction "polyBLEP"-style step; the saw is kept naive and
 * documented (bass range); sine is exact to the table. Oversampling is deferred until benchmarked. */

#define DSP_Q15 32767

/* ------------------------------------------------------------ tables ---- */
static int16_t sin256[257];          /* sin(2*pi*i/256) in Q15 (wrap-around) */
static uint16_t exp2_64[65];         /* 2^(i/64) in Q14, for the 1/12-octave fraction */
static uint16_t drive_comp[128];     /* Q16 output compensation per drive value */

static void dsp_init(void)
{
    int32_t x = 16384, y = 0;       /* CORDIC-style rotation, start angle 0 */
    uint32_t i, n;
    for (i = 0; i <= 256u; i++) {
        sin256[i & 255u] = (int16_t)((y * 32000u) >> 15);   /* gain-corrected readout */
        {
            int32_t nx = x - ((y * 402) >> 14);   /* tan(2pi/256) ~ 0.024544 */
            int32_t ny = y + ((x * 402) >> 14);
            x = nx;
            y = ny;
        }
    }
    sin256[256] = sin256[0];
    exp2_64[0] = 16384;
    for (n = 1; n <= 64u; n++)
        exp2_64[n] = (uint16_t)(((uint32_t)exp2_64[n - 1u] * 16557u) >> 14);
      /* 2^(1/64) ~ 1.010889; Q14 */
    for (n = 0; n < 128u; n++)
        drive_comp[n] = (uint16_t)((256u << 16) / (256u + 3u * n));
}

/* ------------------------------------------------------------ sine ---- */
static int32_t dsp_sin(uint32_t ph)         /* Q15 output, ph = full phase (2^32 = 2pi) */
{
    uint32_t i = ph >> 24;
    int32_t a = sin256[i & 255u], b = sin256[(i + 1u) & 255u];
    int32_t frac = (int32_t)(ph & 0x00FFFFFFu);
    return a + (((b - a) * frac) >> 24);
}

/* semitone -> frequency multiplier: 2^(st/12), st in Q8 (1 = 1/256 semitone) */
static int32_t dsp_semis_mult(int32_t st_q8)
{
    int32_t oct = st_q8 / (12 * 256);
    int32_t rem = st_q8 % (12 * 256);
    if (rem < 0)
        rem += 12 * 256, oct--;
    {
        uint32_t idx = (uint32_t)((rem * 64u) / (12 * 256));
        int32_t f = exp2_64[idx];
        if (oct >= 0)
            return f << (oct < 15 ? oct : 15);
        return f >> (-oct > 15 ? 15 : -oct);
    }
}

/* ------------------------------------------------------------ SVF ---- */
typedef struct {
    int32_t v1, v2;
    int32_t f;              /* fc normalized 0..~0.45, Q16 */
    int32_t damp;           /* 1/Q, Q16 */
} dsp_svf;

static void dsp_svf_set(dsp_svf *s, int32_t fc_q16, int32_t q)
{
    int32_t f;
    if (fc_q16 < 0) fc_q16 = 0;
    if (fc_q16 > 29491) fc_q16 = 29491;          /* ~0.45 Hz/FS */
    f = (fc_q16 * 411774u) >> 16;                /* 2*sin(pi*fc) ~ 2*pi*fc */
    if (f > 58982) f = 58982;
    s->f = f;
    s->damp = q > 0 ? ((1u << 26) / q) : (1 << 26);
}

static void dsp_svf_run(dsp_svf *s, int32_t x, int32_t *lp, int32_t *bp, int32_t *hp)
{
    int32_t f = s->f, damp = s->damp;
    int32_t low = s->v2, band = s->v1;
    int32_t high = x - low - ((damp * band) >> 16);
    band += ((f * high) >> 16);
    low += ((f * band) >> 16);
    s->v1 = band;
    s->v2 = low;
    *lp = low;
    *bp = band;
    *hp = high;
}

/* ---------------------------------------------- envelopes (semantics) ---- */
/* exponential decay with time constant in samples (D60 semantics: D60 ms -> tau = D60/ln(1000)) */
typedef struct {
    int32_t g;              /* Q15 amplitude envelope */
    int32_t step;           /* per-sample multiplicative step, Q16 */
    uint32_t state;
} dsp_env_dec;

static int32_t dsp_env_dec_step_q16(int32_t d60_ms)
{
    int32_t n = d60_ms > 0 ? (FS * d60_ms / 1000) * 10000 / 6908 : 1;
    /* samples to -60 dB: N = FS*D60/1000 * ln(1000); step = (1<<16)*(1 - 1/N) */
    if (n < 1) n = 1;
    if (n > (1 << 24)) n = 1 << 24;
    return (1 << 16) - ((1 << 22) / n);
}

static void dsp_env_dec_start(dsp_env_dec *e, int32_t d60_ms, int32_t level_q15)
{
    e->g = level_q15;
    e->step = dsp_env_dec_step_q16(d60_ms);
}

static void dsp_env_dec_run(dsp_env_dec *e)
{
    e->g = (int32_t)(((int64_t)e->g * e->step) >> 16);
    if (e->g < 2)
        e->g = 0;
}

/* pitch envelope: st (semitones) decays exponentially with tauP; a per-sample multiplier */
typedef struct {
    int32_t st_q8;          /* current deviation, Q8 */
    int32_t scale_q16;      /* exp(-1/(tauP*FS)) in Q16 */
} dsp_env_pitch;

static int32_t dsp_env_pitch_step_q16(int32_t tauP_ms)
{
    int32_t n = tauP_ms > 0 ? FS * tauP_ms / 1000 : 1;
    if (n < 1) n = 1;
    if (n > (1 << 24)) n = 1 << 24;
    return (1 << 16) - ((1 << 22) / n);
}

static void dsp_env_pitch_start(dsp_env_pitch *e, int32_t st0_q8, int32_t tauP_ms)
{
    e->st_q8 = st0_q8;
    e->scale_q16 = dsp_env_pitch_step_q16(tauP_ms);
}

static void dsp_env_pitch_run(dsp_env_pitch *e)
{
    e->st_q8 = (int32_t)(((int64_t)e->st_q8 * e->scale_q16) >> 16);
}

/* linear attack (samples) + exponential decay, used for melodic voices */
typedef struct {
    int32_t g;
    int32_t target;
    int32_t atk;            /* per-sample increment during attack */
    int32_t dec;            /* multiplicative per-sample step after attack (Q16) */
    uint8_t attacking;
} dsp_env_ar;

static void dsp_env_ar_start(dsp_env_ar *e, int32_t level_q15, int32_t a_ms, int32_t d60_ms)
{
    int32_t a = a_ms > 0 ? FS * a_ms / 1000 : 1;
    e->target = level_q15;
    e->atk = a ? level_q15 / a : level_q15;
    e->dec = dsp_env_dec_step_q16(d60_ms);
    e->g = 0;
    e->attacking = 1;
}

static void dsp_env_ar_run(dsp_env_ar *e)
{
    if (e->attacking) {
        e->g += e->atk;
        if (e->g >= e->target) {
            e->g = e->target;
            e->attacking = 0;
        }
    } else {
        e->g = (int32_t)(((int64_t)e->g * e->dec) >> 16);
    }
}

/* ------------------------------------------------------------ saturation ---- */
static int32_t dsp_soft(int32_t x)              /* x in Q15; soft saturation */
{
    int64_t x3 = (int64_t)x * x * x >> 30;
    return x - (int32_t)(x3 / 3);
}

static int32_t dsp_abrasive(int32_t x)         /* fold-style abrasive shaping */
{
    if (x > 20000)
        x = 40000 - x;
    else if (x < -20000)
        x = -40000 - x;
    return x;
}

/* drive with input gain and output compensation: drive 0..127 */
static int32_t dsp_drive(int32_t x, int32_t drive, int32_t abrasive)
{
    int32_t g = 256 + (drive * 3);              /* up to ~2.5x pre-gain */
    int32_t y = (int32_t)((int64_t)x * g >> 8);
    y = abrasive ? dsp_abrasive(y) : dsp_soft(y);
    {
        int32_t c = drive_comp[drive < 0 ? 0 : drive > 127 ? 127 : drive];
        return (int32_t)((int64_t)y * c >> 16);
    }
}

/* ------------------------------------------------------------ DC removal ---- */
/* a proper one-pole high-pass (~7 Hz), so the sub/low body is preserved */
typedef struct {
    int32_t l, y;
} dsp_dc;

static void dsp_dc_run(dsp_dc *d, int32_t x, int32_t *y)
{
    int32_t hp = x - d->l + ((d->y * 65477) >> 16);   /* a ~ 0.9990 */
    d->l = x;
    d->y = hp;
    *y = hp;
}
