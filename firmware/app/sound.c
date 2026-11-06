/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* sound.c - FM-23's three engines as they render in the audio ISR: the 303-style acid bass
 * (mono, a one-pole filter + a bounded soft drive + glide), the lead (two detuned oscillators,
 * LFO vibrato), and the synthesized percussion set (kick, snare, hats, metallic/strange
 * polyrhythmic pings, a noise sweep, an FM zap). Oscillators are triangle/saw from a phase
 * accumulator (no tables), fixed point, O(1) per block; the main loop only sets volatile requests
 * (drum triggers, a stop), the ISR reads the keys and MIDI itself. */

#define DRUM_N 8u
#define KEY_DRUM0 0u          /* keys 0..7: drum pads */
#define KEY_BASS0 8u          /* keys 8..17: bass (note = 36 + k + 12*cfg.bass_oct) */
#define KEY_LEAD0 18u         /* keys 18..26: lead (note = 60 + k + 12*cfg.lead_oct) */
#define BASS_BASE 36
#define LEAD_BASE 60

/* seq.c defines these (it is included after sound.c) */
static void seq_midi_clock(void);
static void seq_ext_start(void);
static void seq_ext_stop(void);

/* phase increments of C4..B4 (2^32 f / FS = 44100) */
static const uint32_t OCT4_INC[12] = {25480119u, 26995246u, 28600467u, 30301139u, 32102938u, 34011878u,
                                      36034330u, 38177043u, 40447168u, 42852281u, 45400411u, 48100060u};

static uint32_t note_inc(uint32_t n)
{
    uint32_t oct = n / 12u, inc = OCT4_INC[n % 12u];
    return oct >= 5u ? inc << (oct - 5u) : inc >> (5u - oct);
}

static int32_t tri_val(uint32_t ph)             /* triangle from a phase, -32768..32767 */
{
    int32_t s = (int32_t)(ph >> 16) - 32768;
    return (s < 0 ? -s : s) * 2 - 32768;
}

static int32_t quad_co(int32_t cut)             /* cutoff 0..127 -> Q15 one-pole coefficient */
{
    return (int32_t)((int64_t)cut * cut * 32767u / (127u * 127u));
}

/* ramp gain toward tgt without clicks: a small difference jumps, the rest eases */
static void env_ramp(int32_t *gain, int32_t tgt)
{
    int32_t d = tgt - *gain;
    if (d > 0) {
        int32_t adv = d >> 5;
        *gain += adv ? adv : 1;
        if (*gain > tgt)
            *gain = tgt;
    } else if (d < 0) {
        int32_t adv = (-d) >> 5;
        *gain -= adv ? adv : 1;
        if (*gain < tgt)
            *gain = tgt;
    }
}

/* ---------------------------------------------------------- the bass ---- */
static struct {
    uint32_t phase, inc, tinc;     /* phase, current increment, glide target */
    int32_t gain, tgt;             /* 0..2048 envelope, target */
    int32_t fcut, fz;              /* filter coefficient (Q15), filter state */
    int32_t drive;                 /* 0..127 */
    uint8_t note;                  /* the sounding note, 0xff none */
    uint8_t midi;                  /* held MIDI note, 0xff none */
    volatile uint8_t stop_req;
} bass;

static void bass_note(uint8_t note, uint8_t vel, uint8_t slide)
{
    if (cfg.bass_algo) {
        bank_bass_note(note, vel);
        return;
    }
    uint32_t inc = note_inc(note);
    if (slide && bass.note != 0xffu) {           /* legato glide: keep the phase, chase the pitch */
        bass.tinc = inc;
        if (bass.gain < 128u)
            bass.inc = inc;
    } else {
        bass.inc = bass.tinc = inc;
    }
    bass.note = note;
    bass.tgt = 1024u + (int32_t)vel * 24u / 127u;
}

static void bass_release(void)
{
    if (cfg.bass_algo) {
        bank_bass_release();
        return;
    }
    bass.note = 0xffu;
    bass.tgt = 0;
}

static __attribute__((noinline)) void bass_render(int32_t *out, uint32_t n)
{
    uint32_t i;
    int32_t x, a, drv;
    if (cfg.bass_algo) {
        bank_bass_render(out, n);
        return;
    }
    for (i = 0; i < n; i++) {
        env_ramp(&bass.gain, bass.tgt);
        if (bass.gain == 0 && bass.tgt == 0)
            continue;
        if (bass.inc != bass.tinc)              /* glide */
            bass.inc += (int32_t)(bass.tinc - bass.inc) >> 7;
        bass.phase += bass.inc;
        x = tri_val(bass.phase);
        a = bass.fcut;
        bass.fz += (int32_t)((int64_t)a * (x - bass.fz) >> 15);
        x = bass.fz;
        drv = bass.drive;
        if (drv) {                              /* pre-gain into a bounded soft clipper (no DC pump) */
            int32_t gg = x + (int32_t)((int64_t)x * drv >> 8);
            if (gg > 32767)
                gg = 32767;
            else if (gg < -32767)
                gg = -32767;
            x += ((gg - x) * drv) >> 7;
        }
        out[i] += (int32_t)((int64_t)x * bass.gain >> 14);
    }
}

/* ---------------------------------------------------------- the lead ---- */
static struct {
    uint32_t phase[2], inc[2];     /* inc holds the base increments (detuned) */
    int32_t gain, tgt;
    uint32_t lfo_ph;
    int32_t lfo_rate, lfo_depth;
    int32_t fcut, fz;
    uint8_t note, midi;
    volatile uint8_t stop_req;
} lead;

static void lead_note(uint8_t note, uint8_t vel)
{
    if (cfg.lead_algo) {
        bank_lead_note(note, vel);
        return;
    }
    uint32_t base = note_inc(note);
    int32_t det = (int32_t)cfg.lead_detune;
    lead.inc[0] = base - (base >> 9) * (uint32_t)det / 25u;
    lead.inc[1] = base + (base >> 9) * (uint32_t)det / 25u;
    lead.note = note;
    lead.tgt = 1024u + (int32_t)vel * 24u / 127u;
    lead.lfo_ph = 0;
}

static void lead_release(void)
{
    if (cfg.lead_algo) {
        bank_lead_release();
        return;
    }
    lead.note = 0xffu;
    lead.tgt = 0;
}

static __attribute__((noinline)) void lead_render(int32_t *out, uint32_t n)
{
    uint32_t i;
    if (cfg.lead_algo) {
        bank_lead_render(out, n);
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t vib, x;
        uint32_t inc0, inc1;
        env_ramp(&lead.gain, lead.tgt);
        if (lead.gain == 0 && lead.tgt == 0)
            continue;
        lead.lfo_ph += (uint32_t)lead.lfo_rate;
        vib = (int32_t)(lead.lfo_ph >> 16) - 32768;
        vib = (vib < 0 ? -vib : vib) * 2 - 32768;
        vib = (vib * lead.lfo_depth) >> 9;
        inc0 = lead.inc[0] + (int32_t)((int64_t)lead.inc[0] * vib >> 24);
        inc1 = lead.inc[1] + (int32_t)((int64_t)lead.inc[1] * vib >> 24);
        lead.phase[0] += inc0;
        lead.phase[1] += inc1;
        x = (tri_val(lead.phase[0]) + tri_val(lead.phase[1])) >> 1;
        lead.fz += (int32_t)((int64_t)lead.fcut * (x - lead.fz) >> 15);
        out[i] += (int32_t)((int64_t)lead.fz * lead.gain >> 12) * (int32_t)cfg.lead_level / 127;
    }
}

/* ---------------------------------------------------------- the drums ---- */
#define DK_KICK 0u
#define DK_SNARE 1u
#define DK_HAT 2u
#define DK_OPEN 3u
#define DK_MET 4u
#define DK_STR 5u
#define DK_NOISE 6u
#define DK_ZAP 7u

static struct {
    uint32_t phase, phase2, inc, inc2, inc0, inc1;
    int32_t gain;
    int32_t dec;                 /* exponential decay per block, Q15-ish */
    int32_t amp;                 /* per-drum level, Q15-ish */
    int32_t lp;                  /* noise filter state (per drum) */
    uint8_t kind;
    uint8_t alive;
} drumv[DRUM_N];

static volatile uint8_t drum_req[DRUM_N];   /* main loop -> ISR */

/* kind -> {noise, sweep, twotone, inc0, inc1, dec, amp} */
static const struct { uint8_t fl; uint32_t i0, i1; int32_t dec, amp; } DRUM_DEF[DRUM_N] = {
    {0, 14608163u, 4382449u, 1400, 30000},    /* kick:  150 -> 45 Hz */
    {1, 18016400u, 18016400u, 1600, 22000},   /* snare: body 185 Hz + noise */
    {2, 0, 0, 3600, 16000},                   /* hat:   noise only, fast */
    {2, 0, 0, 1400, 17000},                   /* open:  noise only, slower */
    {4, 69600000u, 70100000u, 900, 20000},    /* metallic: two tones ~ 790 Hz beat */
    {4, 89000000u, 89700000u, 750, 20000},    /* strange: two tones ~ 1010 Hz */
    {0, 0, 0, 600, 14000},                    /* noise sweep: low-passed noise */
    {6, 7000000u, 103000000u, 1700, 18000},   /* zap: 80 -> 2500 Hz sweep */
};

static uint32_t nx = 0x12345678u;            /* white noise (LFSR) */
static int32_t noise_next(void)
{
    nx ^= nx << 13u;
    nx ^= nx >> 17u;
    nx ^= nx << 5u;
    return (int32_t)(nx >> 16) - 32768;
}

static void drum_start(uint8_t d);

/* the kit-aware drum trigger and render (the recipe bank replaces the naive engine when kit>0) */
static void drum_trigger(uint8_t d, uint8_t vel)
{
    if (cfg.kit)
        bk_trigger(d, d, vel);
    else
        drum_start(d);
}

static void drum_start(uint8_t d)
{
    drumv[d].kind = d;
    drumv[d].alive = 1;
    drumv[d].gain = 4096;
    drumv[d].inc = drumv[d].inc0 = DRUM_DEF[d].i0;
    drumv[d].inc2 = DRUM_DEF[d].i1;
    drumv[d].inc1 = DRUM_DEF[d].i1;
    drumv[d].phase = drumv[d].phase2 = 0;
    drumv[d].lp = 0;
    drumv[d].dec = DRUM_DEF[d].dec;
    drumv[d].amp = DRUM_DEF[d].amp;
}

static __attribute__((noinline)) void drum_render(int32_t *out, uint32_t n)
{
    uint32_t i, d;
    for (d = 0; d < DRUM_N; d++) {
        uint8_t kind;
        int32_t g;
        if (!drumv[d].alive)
            continue;
        g = drumv[d].gain;
        if (g < 24u) {
            drumv[d].alive = 0;
            continue;
        }
        drumv[d].gain = g - (g * drumv[d].dec >> 15);
        drumv[d].inc += (int32_t)(drumv[d].inc1 - drumv[d].inc) >> 6;
        kind = drumv[d].kind;
        for (i = 0; i < n; i++) {
            int32_t s = 0;
            drumv[d].phase += drumv[d].inc;
            if (kind == DK_HAT || kind == DK_OPEN) {       /* differentiator: bright hiss */
                int32_t no = noise_next(), hp = no - drumv[d].lp;
                drumv[d].lp += (no - drumv[d].lp) >> 4;
                s = hp;
            } else if (kind == DK_NOISE) {                 /* low-passed noise sweep */
                int32_t no = noise_next();
                drumv[d].lp += (no - drumv[d].lp) >> 7;
                s = drumv[d].lp;
            } else if (kind == DK_SNARE) {
                s = (tri_val(drumv[d].phase) >> 2) + noise_next() * 3 / 4;
            } else if (kind == DK_MET || kind == DK_STR) {
                drumv[d].phase2 += drumv[d].inc2;
                s = (tri_val(drumv[d].phase) + tri_val(drumv[d].phase2)) >> 1;
            } else {                                        /* kick, zap: sweeping triangle */
                s = tri_val(drumv[d].phase);
            }
            out[i] += (int32_t)((int64_t)s * drumv[d].gain >> 14) * (int32_t)cfg.drum_level / 127 *
                      drumv[d].amp / 16384;
        }
    }
}

/* ---------------------------------------------------------- effects (bus) ---- */
/* a delay and a spring-ish reverb on the mix bus, in the pool; sends come from cfg. The reverb
 * is a second, longer feedback buffer - simple, stable, cheap. The drive soft-clips the bus. */
#define FX_DLY_LEN 16384u
#define FX_REV_LEN 32768u
static int32_t dly[FX_DLY_LEN] __attribute__((section(".pool")));
static int32_t rev[FX_REV_LEN] __attribute__((section(".pool")));
static uint32_t dly_w, rev_w;

static void fx_apply(int32_t *mix, uint32_t n)
{
    uint32_t i;
    int32_t drv = cfg.fx_drive;
    uint32_t dt = cfg.fx_delay, rt = cfg.fx_reverb;
    for (i = 0; i < n; i++) {
        int32_t x = mix[i];
        if (drv) {                              /* bus drive: pre-gain into a bounded soft clipper */
            int32_t gg = x + (int32_t)((int64_t)x * drv >> 8);
            if (gg > 32767)
                gg = 32767;
            else if (gg < -32767)
                gg = -32767;
            x += ((gg - x) * drv) >> 7;
        }
        if (dt) {
            uint32_t r = (dly_w - (1u + dt * FX_DLY_LEN / 256u)) & (FX_DLY_LEN - 1u);
            int32_t wet = dly[r];
            dly[dly_w & (FX_DLY_LEN - 1u)] = x + (wet * 3 / 5);
            x += (wet * (int32_t)dt) >> 7;
            dly_w++;
        }
        if (rt) {
            uint32_t r = (rev_w - (1u + rt * FX_REV_LEN / 128u)) & (FX_REV_LEN - 1u);
            int32_t wet = rev[r];
            rev[rev_w & (FX_REV_LEN - 1u)] = x + (wet * 2 / 5);
            x += (wet * (int32_t)rt) >> 7;
            rev_w++;
        }
        mix[i] = x;
    }
}

/* ------------------------------------------------------- MIDI and keys ---- */
static void midi_h(uint32_t pkt)
{
    uint32_t st = (pkt >> 8) & 0xF0u, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
    if (pkt == MIDI_LOST) {
        bass_release();
        lead_release();
        return;
    }
    if (st == 0x90u && d2) {
        if (d1 < 48u) {
            bass.midi = (uint8_t)d1;
            bass_note((uint8_t)d1, 110, 0);
        } else {
            lead.midi = (uint8_t)d1;
            lead_note((uint8_t)d1, 110);
        }
    } else if ((st == 0x80u || st == 0x90u) && d1 < 48u && d1 == bass.midi) {
        uint8_t n = bass.note;
        bass.midi = 0xffu;
        if (n != 0xffu)
            bass_release();
    } else if ((st == 0x80u || st == 0x90u) && d1 >= 48u && d1 == lead.midi) {
        uint8_t n = lead.note;
        lead.midi = 0xffu;
        if (n != 0xffu)
            lead_release();
    }
}

static __attribute__((noinline)) void sound_audio(int32_t *out, uint32_t n)
{
    uint32_t pkt, i;
    int32_t mix[64];
    uint8_t d;
    uint32_t keys, k, lo = 0xFFFFFFFFu, hi = 0;
    uint8_t bwant = 0xffu, lwant = 0xffu;
    static uint32_t last_keys;

    if (bass.stop_req) {
        bass.stop_req = 0;
        bass_release();
        lead_release();
        for (d = 0; d < DRUM_N; d++)
            drumv[d].alive = 0;
    }
    for (d = 0; d < DRUM_N; d++)
        if (drum_req[d]) {
            drum_req[d] = 0;
            drum_trigger(d, 110);
        }
    while (midi_in_take(&pkt, 0)) {
        uint8_t st = (uint8_t)(pkt >> 8);
        if (st == 0xF8u)
            seq_midi_clock();
        else if (st == 0xFAu || st == 0xFBu)
            seq_ext_start();
        else if (st == 0xFCu)
            seq_ext_stop();
        else
            midi_h(pkt);
    }

    /* the keys: bass = the lowest held bass key, lead = the highest held lead key, drums = pads */
    keys = fm1_in.notes;
    for (k = KEY_BASS0; k < KEY_LEAD0 && k < 27u; k++)
        if ((keys >> k) & 1u) {
            if (k < lo)
                lo = k;
        }
    for (k = KEY_LEAD0; k < 27u; k++)
        if ((keys >> k) & 1u && k > hi)
            hi = k;
    if (lo != 0xFFFFFFFFu) {
        bwant = (uint8_t)(BASS_BASE + (lo - KEY_BASS0) + 12u * cfg.bass_oct);
        if (bwant != bass.note)
            bass_note(bwant, 115, (bass.note != 0xffu) && (last_keys >> lo & 1u) == 0u);
    } else if (bass.note != 0xffu && bass.midi == 0xffu) {
        bass_release();
    }
    if (hi)
        lwant = (uint8_t)(LEAD_BASE + (hi - KEY_LEAD0) + 12u * cfg.lead_oct);
    if (lwant != 0xffu && lwant != lead.note)
        lead_note(lwant, 110);
    else if (lwant == 0xffu && lead.note != 0xffu && lead.midi == 0xffu)
        lead_release();
    for (k = KEY_DRUM0; k < KEY_BASS0; k++)
        if ((keys >> k) & 1u && !((last_keys >> k) & 1u))
            drum_trigger((uint8_t)k, 110);
    last_keys = keys;

    bass.fcut = quad_co(cfg.bass_cut);
    bass.drive = cfg.bass_drive;
    lead.fcut = quad_co(cfg.lead_cut);
    lead.lfo_rate = (int32_t)cfg.lead_lfo * 220u;
    lead.lfo_depth = 64u;

    for (i = 0; i < n; i++)
        mix[i] = 0;
    bass_render(mix, n);
    lead_render(mix, n);
    if (cfg.kit)
        bk_render(mix, n);
    else
        drum_render(mix, n);
    fx_apply(mix, n);
    for (i = 0; i < n; i++) {
        int32_t s = (mix[i] * (int32_t)cfg.master) >> 7;
        if (s > 32767)
            s = 32767;
        else if (s < -32767)
            s = -32767;
        out[2u * i] = s;
        out[2u * i + 1u] = s;
    }
}

/* 0xff is "no note", so the release logic must start from it, not from the zero-init 0 */
static void sound_init(void)
{
    bass.note = bass.midi = 0xffu;
    lead.note = lead.midi = 0xffu;
}

static void sound_all_off(void)
{
    uint8_t d;
    bass_release();
    lead_release();
    for (d = 0; d < DRUM_N; d++)
        drumv[d].alive = 0;
    for (d = 0; d < BK_N; d++)
        bk[d].alive = 0;
}

static void sound_stop(void)
{
    bass.stop_req = 1;
    lead.stop_req = 1;
}
