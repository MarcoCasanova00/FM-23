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

/* shared performance helpers for the ENV / LFO pages */
static int32_t bank_ms(uint8_t v) { return 1 + (int32_t)v * 5u; }

static int32_t bank_macro_ms(uint8_t v)
{
    int32_t m = bank_ms(v);
    if (cfg.macro == 1u)
        m = m * 5 / 4;
    else if (cfg.macro == 3u)
        m = m * 3 / 4;
    return m;
}

static int32_t bank_lfo_val(uint8_t shape, uint32_t ph)
{
    if (shape == 1u) {
        int32_t t = (int32_t)(ph >> 16) - 32768;
        return (t < 0 ? -t : t) * 2 - 32768;
    }
    if (shape == 2u)
        return (ph & 0x80000000u) ? 32767 : -32767;
    if (shape == 3u)
        return (int32_t)((ph >> 16) & 0xFFFFu) - 32768;
    return dsp_sin(ph);
}

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
    if (cfg.macro == 1u) {              /* RUBBER: slower decays, softer pitch drop */
        d->body_step = (int32_t)((int64_t)d->body_step * 5 / 4);
        d->st_step = (int32_t)((int64_t)d->st_step * 5 / 4);
    } else if (cfg.macro == 2u && (kind == BK_MET || kind == BK_STR)) {
        d->idx += 16384;                /* METAL: hotter metallic ping */
        d->idx_step = (int32_t)((int64_t)d->idx_step * 3 / 4);
    } else if (cfg.macro == 3u) {       /* SCREAM: short, hard */
        d->body_step = (int32_t)((int64_t)d->body_step * 3 / 4);
        d->st_step = (int32_t)((int64_t)d->st_step * 3 / 4);
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

/* ------------------------------------------------------ melodic bank (Phase 2b) ---- */
/* B01 kick-derived, B02 rubber FM, B03 tribe saw; L01 hollow, L02 snake, L05 growl, L07 ping.
 * Note frequency -> phase increment uses 2^32/FS per Hz (32-bit, no 64-bit divides). */

typedef struct {
    int32_t g, lvl, atk, dec, rel;
    uint8_t sus;
} b_env;

static void b_env_on(b_env *e, int32_t lvl, int32_t a_ms, int32_t d60, int32_t r_ms)
{
    int32_t a = a_ms > 0 ? FS * a_ms / 1000 : 1;
    int32_t r = r_ms > 0 ? FS * r_ms / 1000 : 1;
    e->lvl = lvl;
    e->atk = a ? lvl / a : lvl;
    e->dec = dsp_env_dec_step_q16(d60);
    e->rel = r ? lvl / r : lvl;
    e->g = 0;
    e->sus = 1;
}

static void b_env_run(b_env *e)
{
    if (e->sus) {
        if (e->g < e->lvl) {
            e->g += e->atk;
            if (e->g > e->lvl)
                e->g = e->lvl;
        } else {
            e->g = (int32_t)(((int64_t)e->g * e->dec) >> 16);
        }
    } else {
        e->g -= e->rel;
        if (e->g < 0)
            e->g = 0;
    }
}

static uint32_t bk_note_inc(uint8_t n)
{
    int32_t rel = ((int32_t)n - 69) * 256;   /* Q8 semitone offset (no signed shift UB) */
    int32_t m = dsp_semis_mult(rel);          /* Q14 */
    int32_t f = (int32_t)(((int64_t)440 * m) >> 14);
    if (f < 1)
        f = 1;
    return (uint32_t)f * 97391u;              /* ~ 2^32 / FS */
}

/* ------------------------------------------------------------ bass ---- */
typedef struct {
    uint32_t ph, mphi, lfo;
    b_env env;
    dsp_svf svf;
    dsp_dc dc;
    int32_t idx, idx_step;
    int32_t st, st_step;
    int32_t cut, cut_step;
    int32_t fcur, fstep, velb;
    uint32_t baseinc;
    uint8_t algo, on;
} b_bass;

static b_bass bb;

static void bank_bass_release(void)
{
    bb.env.sus = 0;
}

static void bank_bass_note(uint8_t note, uint8_t vel)
{
    uint32_t inc = bk_note_inc(note);
    int32_t lvl = 24000 + vel * 60;
    bb.algo = cfg.bass_algo;
    bb.ph = bb.mphi = bb.lfo = 0;
    bb.dc.l = bb.dc.y = 0;
    bb.baseinc = inc;
    bb.on = 1;
    if (bb.algo == 1u) {                /* B01 kick-derived offbeat bass */
        bb.env = (b_env){0};
        b_env_on(&bb.env, lvl, bank_macro_ms(cfg.env_a[0]), bank_macro_ms(cfg.env_d[0]),
                 bank_macro_ms(cfg.env_r[0]));
        bb.st = 5 << 8;
        bb.st_step = dsp_env_pitch_step_q16(bank_macro_ms(cfg.env_p[0]));
        dsp_svf_set(&bb.svf, 4096, 8);
    } else if (bb.algo == 2u) {         /* B02 rubber FM */
        bb.env = (b_env){0};
        b_env_on(&bb.env, lvl, bank_macro_ms(cfg.env_a[0]), bank_macro_ms(cfg.env_d[0]),
                 bank_macro_ms(cfg.env_r[0]));
        bb.idx = 3 * 16384;
        bb.idx_step = dsp_env_dec_step_q16(120);
        dsp_svf_set(&bb.svf, 6554, 6);
    } else if (bb.algo == 4u) {         /* B04 acid-adjacent: saw -> resonant LP, filter env */
        bb.env = (b_env){0};
        b_env_on(&bb.env, lvl, bank_macro_ms(cfg.env_a[0]), bank_macro_ms(cfg.env_d[0]),
                 bank_macro_ms(cfg.env_r[0]));
        bb.fcur = 24576 + (vel > 110 ? 4096 : 0);   /* accent opens the filter more */
        bb.fstep = dsp_env_dec_step_q16(150);
        bb.velb = vel;
        dsp_svf_set(&bb.svf, 24576, 9);
    } else {                            /* B03 tribe saw/pulse */
        bb.env = (b_env){0};
        b_env_on(&bb.env, lvl, bank_macro_ms(cfg.env_a[0]), bank_macro_ms(cfg.env_d[0]),
                 bank_macro_ms(cfg.env_r[0]));
        bb.cut = 32768;
        bb.cut_step = dsp_env_dec_step_q16(140);
        dsp_svf_set(&bb.svf, 16384, 5);
    }
    bb.env.g = 0;
    (void)inc;
}

static void bank_bass_render(int32_t *out, uint32_t n)
{
    uint32_t i;
    if (!bb.on)
        return;
    for (i = 0; i < n; i++) {
        int32_t s = 0, lp = 0, bp = 0, hp = 0;
        uint32_t inc;
        b_env_run(&bb.env);
        if (bb.env.g < 2) {
            bb.on = 0;
            break;
        }
        if (bb.algo == 1u) {
            bb.st = (int32_t)(((int64_t)bb.st * bb.st_step) >> 16);
            inc = bb.baseinc;
            inc = (uint32_t)((int64_t)inc * dsp_semis_mult(bb.st) >> 14);
            bb.ph += inc;
            s = dsp_sin(bb.ph);
            s = (int32_t)((int64_t)dsp_soft(s) * bb.env.g >> 15);
        } else if (bb.algo == 2u) {
            int32_t idx;
            uint32_t modinc;
            bb.idx = (int32_t)(((int64_t)bb.idx * bb.idx_step) >> 16);
            idx = bb.idx;
            inc = bb.baseinc;
            modinc = inc * 2;
            bb.ph += inc;
            bb.mphi += modinc;
            s = dsp_sin(bb.ph + (uint32_t)((int64_t)dsp_sin(bb.mphi) * idx >> 14));
            s = (int32_t)((int64_t)s * bb.env.g >> 15);
            dsp_svf_run(&bb.svf, s, &lp, &bp, &hp);
            s = lp + (s >> 2);
        } else if (bb.algo == 4u) {         /* B04 acid-adjacent */
            int32_t saw;
            inc = bb.baseinc;
            bb.ph += inc;
            saw = (int32_t)(bb.ph >> 16) - 32768;
            bb.fcur = (int32_t)(((int64_t)bb.fcur * bb.fstep) >> 16);
            if (bb.fcur < 4096)
                bb.fcur = 4096;
            dsp_svf_set(&bb.svf, bb.fcur > 29491 ? 29491 : bb.fcur, 9);
            dsp_svf_run(&bb.svf, saw, &lp, &bp, &hp);
            s = lp;
            s = (int32_t)((int64_t)dsp_drive(s, cfg.macro == 3u ? 95 : 70, 0) * bb.env.g >> 15);
        } else {
            uint32_t saw = bb.ph + 0;
            int32_t pulse;
            inc = bb.baseinc;
            bb.ph += inc;
            pulse = (bb.ph & 0x80000000u) ? 32767 : -32767;
            saw = (int32_t)(bb.ph >> 16) - 32768;
            bb.cut = (int32_t)(((int64_t)bb.cut * bb.cut_step) >> 16);
            dsp_svf_set(&bb.svf, bb.cut > 24576 ? 24576 : bb.cut, 4);
            dsp_svf_run(&bb.svf, (int32_t)saw + (pulse >> 1), &lp, &bp, &hp);
            s = lp >> 1;
            s = (int32_t)((int64_t)dsp_drive(s, 45, 0) * bb.env.g >> 15);
        }
        dsp_dc_run(&bb.dc, s, &s);
        if (s > 20000)
            s = 20000;
        else if (s < -20000)
            s = -20000;
        out[i] += (bb.algo == 3u) ? (s >> 2) : (s >> 1);
    }
}

/* ------------------------------------------------------------ lead ---- */
typedef struct {
    uint32_t ph, ph2, mphi, lfo;
    b_env env;
    dsp_svf svf;
    dsp_dc dc;
    int32_t idx, idx_step;
    int32_t vib_depth;
    int32_t rat, rat_step, ff, ff_step;
    uint32_t baseinc, last_mph;
    uint8_t algo, on;
} b_lead;

static b_lead bl;

static void bank_lead_release(void)
{
    if (cfg.arp_on && cfg.arp_hold)
        return;                       /* ARP HOLD: the drone latches until stopped */
    bl.env.sus = 0;
}

static void bank_lead_cut(void)
{
    bl.env.sus = 0;
    bl.on = 0;
}

static void bank_lead_note(uint8_t note, uint8_t vel)
{
    uint32_t inc = bk_note_inc(note);
    int32_t lvl = 22000 + vel * 60;
    bl.algo = cfg.lead_algo;
    bl.ph = bl.ph2 = bl.mphi = bl.lfo = 0;
    bl.dc.l = bl.dc.y = 0;
    bl.idx = 0;
    bl.baseinc = inc;
    bl.last_mph = 0;
    bl.on = 1;
    if (bl.algo == 1u) {                /* L01 hollow (two pulse oscs, octave apart) */
        bl.env = (b_env){0};
        b_env_on(&bl.env, lvl, bank_macro_ms(cfg.env_a[1]), bank_macro_ms(cfg.env_d[1]),
                 bank_macro_ms(cfg.env_r[1]));
        dsp_svf_set(&bl.svf, 12288, 5);
    } else if (bl.algo == 2u) {         /* L02 snake / flute-adjacent */
        bl.env = (b_env){0};
        b_env_on(&bl.env, lvl, bank_macro_ms(cfg.env_a[1]), bank_macro_ms(cfg.env_d[1]),
                 bank_macro_ms(cfg.env_r[1]));
        bl.idx = 0x2000;                /* small PM index */
        dsp_svf_set(&bl.svf, 8192, 6);
        bl.vib_depth = cfg.lfo_on[1] ? (int32_t)cfg.lfo_depth[1] * 300u / 127u : 256;
    } else if (bl.algo == 3u) {         /* L05 resonant growl (bounded) */
        bl.env = (b_env){0};
        b_env_on(&bl.env, lvl, bank_macro_ms(cfg.env_a[1]), bank_macro_ms(cfg.env_d[1]),
                 bank_macro_ms(cfg.env_r[1]));
        dsp_svf_set(&bl.svf, 24576, 14);
    } else if (bl.algo == 5u) {         /* L03 nasal FM reed (C:M 1:3) */
        bl.env = (b_env){0};
        b_env_on(&bl.env, lvl, bank_macro_ms(cfg.env_a[1]), bank_macro_ms(cfg.env_d[1]),
                 bank_macro_ms(cfg.env_r[1]));
        bl.idx = 2 * 16384;
        bl.idx_step = dsp_env_dec_step_q16(100);
        dsp_svf_set(&bl.svf, 13107, 8);   /* band-pass area */
    } else if (bl.algo == 6u) {         /* L04 sync razor */
        bl.env = (b_env){0};
        b_env_on(&bl.env, lvl, bank_macro_ms(cfg.env_a[1]), bank_macro_ms(cfg.env_d[1]),
                 bank_macro_ms(cfg.env_r[1]));
        bl.rat = 6 * 16384;
        bl.rat_step = dsp_env_dec_step_q16(250);
        dsp_svf_set(&bl.svf, 16384, 5);
    } else if (bl.algo == 7u) {         /* L06 acid / filter scream */
        bl.env = (b_env){0};
        b_env_on(&bl.env, lvl, bank_macro_ms(cfg.env_a[1]), bank_macro_ms(cfg.env_d[1]),
                 bank_macro_ms(cfg.env_r[1]));
        bl.ff = 8192;
        bl.ff_step = (32768 - 8192) / (FS * 180 / 1000);
        dsp_svf_set(&bl.svf, 8192, 10);
    } else {                            /* L07 alien ping (FM, percussive) */
        bl.env = (b_env){0};
        b_env_on(&bl.env, lvl, bank_macro_ms(cfg.env_a[1]), bank_macro_ms(cfg.env_d[1]), 0);
        bl.idx = 3 * 16384;
        bl.idx_step = dsp_env_dec_step_q16(160);
        dsp_svf_set(&bl.svf, 16384, 6);
    }
    (void)inc;
}

static void bank_lead_render(int32_t *out, uint32_t n)
{
    uint32_t i;
    if (!bl.on)
        return;
    for (i = 0; i < n; i++) {
        int32_t s = 0, lp = 0, bp = 0, hp = 0;
        uint32_t inc = bk_note_inc(60);   /* placeholder (set below) */
        b_env_run(&bl.env);
        if (bl.env.g < 2) {
            bl.on = 0;
            break;
        }
        if (bl.algo == 1u) {
            int32_t a, b;
            inc = bl.baseinc;
            a = (int32_t)((int64_t)inc * 32898 >> 16);   /* +3 cents */
            b = (int32_t)((int64_t)inc * 32638 >> 16);   /* -3 cents */
            bl.ph += (uint32_t)a;
            bl.ph2 += (uint32_t)b;
            s = (bl.ph & 0x80000000u) ? 32767 : -32767;
            s += ((bl.ph2 & 0x80000000u) ? 32767 : -32767) >> 1;
            dsp_svf_run(&bl.svf, s, &lp, &bp, &hp);
            s = (int32_t)((int64_t)dsp_drive(lp >> 1, 40, 0) * bl.env.g >> 15);
        } else if (bl.algo == 2u) {
            uint32_t modinc;
            inc = bl.baseinc;
            modinc = inc * 2;
            bl.ph += inc;
            bl.mphi += modinc;
            bl.lfo += (uint32_t)(5u * 97391u);   /* ~5 Hz vibrato */
            s = dsp_sin(bl.ph + (uint32_t)((int64_t)dsp_sin(bl.mphi) * bl.idx >> 14));
            s += (int32_t)((int64_t)dsp_sin(bl.lfo) * bl.vib_depth >> 15);
            dsp_svf_run(&bl.svf, s, &lp, &bp, &hp);
            s = (int32_t)((int64_t)lp * bl.env.g >> 15);
        } else if (bl.algo == 3u) {
            int32_t sweep;
            inc = bl.baseinc;
            bl.ph += inc;
            bl.lfo += (uint32_t)(30u * 97391u);
            sweep = (int32_t)((int64_t)dsp_sin(bl.lfo) * 8192 >> 15);
            dsp_svf_set(&bl.svf, 24576 + sweep, 14);
            s = (int32_t)(bl.ph >> 16) - 32768;
            dsp_svf_run(&bl.svf, s, &lp, &bp, &hp);
            s = (int32_t)((int64_t)dsp_drive(bp, cfg.macro == 3u ? 100 : 80, 1) * bl.env.g >> 15);
        } else if (bl.algo == 4u) {
            int32_t idx;
            uint32_t modinc;
            modinc = (uint32_t)((int64_t)inc * 19 / 7);
            bl.idx = (int32_t)(((int64_t)bl.idx * bl.idx_step) >> 16);
            idx = bl.idx;
            bl.ph += inc;
            bl.mphi += modinc;
            s = dsp_sin(bl.ph + (uint32_t)((int64_t)dsp_sin(bl.mphi) * idx >> 14));
            s = (int32_t)((int64_t)s * bl.env.g >> 15);
        } else if (bl.algo == 5u) {         /* L03 nasal FM reed */
            uint32_t modinc;
            modinc = inc * 3;
            bl.idx = (int32_t)(((int64_t)bl.idx * bl.idx_step) >> 16);
            bl.ph += inc;
            bl.mphi += modinc;
            s = dsp_sin(bl.ph + (uint32_t)((int64_t)dsp_sin(bl.mphi) * bl.idx >> 14));
            dsp_svf_run(&bl.svf, s, &lp, &bp, &hp);
            s = (int32_t)((int64_t)bp * bl.env.g >> 15);
        } else if (bl.algo == 6u) {         /* L04 sync razor */
            int32_t rat;
            uint32_t slave = bl.ph;         /* slave phase (hard-synced to master) */
            bl.rat = (int32_t)(((int64_t)bl.rat * bl.rat_step) >> 16);
            rat = bl.rat;
            bl.ph += inc;
            if (bl.ph < bl.last_mph) {      /* master wrapped: hard sync */
                slave = 0;
                if (rat < 2 * 16384)
                    rat = 2 * 16384;
            }
            slave += (uint32_t)((int64_t)inc * rat >> 14);
            bl.last_mph = bl.ph;
            s = (int32_t)(slave >> 16) - 32768;
            dsp_svf_run(&bl.svf, s, &lp, &bp, &hp);
            s = (int32_t)((int64_t)dsp_drive(lp, 60, 0) * bl.env.g >> 15);
        } else {                            /* L06 acid / filter scream */
            int32_t wob;
            bl.ff += bl.ff_step;
            if (bl.ff > 29491)
                bl.ff = 29491;
            bl.lfo += (uint32_t)(70u * 97391u);
            wob = (int32_t)((int64_t)dsp_sin(bl.lfo) * 2048 >> 15);
            dsp_svf_set(&bl.svf, bl.ff + wob, 10);
            s = (int32_t)(bl.ph >> 16) - 32768;
            bl.ph += inc;
            dsp_svf_run(&bl.svf, s, &lp, &bp, &hp);
            s = (int32_t)((int64_t)dsp_drive(bp, cfg.macro == 3u ? 120 : 90, 1) * bl.env.g >> 15);
        }
        dsp_dc_run(&bl.dc, s, &s);
        if (s > 20000)
            s = 20000;
        else if (s < -20000)
            s = -20000;
        out[i] += (bl.algo == 1u) ? (s >> 2) : (s >> 1);
    }
}
