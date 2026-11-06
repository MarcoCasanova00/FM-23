/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* data.c - FM-23's settings, the active pattern and the song chain, the stored records (store.c):
 * slot 0 the settings, slot 1 the active pattern, slot 10 the song bank + chain. All plain
 * structs, the version-independent fields first, new fields appended at the end. Load with
 * defaults first and the record over them, clamping every loaded value; save only on purpose
 * (SAVE), never from the ISR. */

#define CFG_SLOT 0u
#define PAT_SLOT 1u
#define SONG_SLOT 10u
#define NPAT 12u
#define NSCALES 15u
#define SONG_NPAT 8u

static const char *const SCALE_NAME[NSCALES] = {
    "PHRYGIAN", "PHRYG DOM", "DORIAN B2", "MINOR", "HARM MIN", "HUNG MIN",
    "LOCRIAN", "INSEN", "IWATO", "DBL HARM", "OCTA HW", "TRITONE",
    "PENT MIN", "BLUES", "CHROMATIC"
};
/* scale degrees as semitone offsets from the root; shorter scales pad with 0 (unused) */
static const uint8_t SCALE_IV[NSCALES][7] = {
    {0, 1, 3, 5, 7, 8, 10}, {0, 1, 4, 5, 7, 8, 10}, {0, 1, 3, 5, 7, 9, 10},
    {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 11}, {0, 2, 3, 6, 7, 8, 11},
    {0, 1, 3, 5, 6, 8, 10}, {0, 1, 5, 7, 10},       {0, 1, 5, 6, 10},
    {0, 1, 4, 5, 7, 8, 11}, {0, 1, 3, 4, 6, 7, 9},  {0, 1, 6, 7},
    {0, 3, 5, 7, 10},       {0, 3, 5, 6, 7, 10},     {0, 1, 2, 3, 4, 5, 6}
};
static const char *const ROOT_NAME[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

typedef struct {
    uint8_t root;        /* 0..11 */
    uint8_t scale;       /* 0..NSCALES-1 */
    uint8_t bpm;         /* 130..200 */
    uint8_t swing;       /* 0..100 (skeleton: kept, not yet applied) */
    uint8_t master;      /* 0..127 output level */
    uint8_t pat;         /* the selected pattern 0..NPAT-1 */
    uint8_t bass_cut;    /* 0..127 */
    uint8_t bass_reso;
    uint8_t bass_drive;
    uint8_t bass_accent;
    uint8_t bass_slide;
    uint8_t lead_detune; /* 0..100 */
    uint8_t lead_lfo;    /* 0..127 */
    uint8_t lead_cut;
    uint8_t lead_level;
    uint8_t kick_decay;
    uint8_t drum_level;
    uint8_t drum_hat;
    uint8_t drum_perc;
    uint8_t fx_delay;    /* 0..127 send */
    uint8_t fx_reverb;
    uint8_t fx_drive;
    uint8_t fx_type;     /* 0 tape, 1 BBD */
    uint8_t arp_style;   /* 0..7 */
    uint8_t arp_rate;
    uint8_t arp_oct;
    uint8_t arp_rhythm;
    uint8_t midi_clk;    /* 1 = MIDI clock in syncs the transport */
    uint8_t style;       /* the GEN style: 0 mental, 1 tribe, 2 hardtekk */
    uint8_t kit;         /* drum kit: 0 classic, 1 tribe foundation, 2 mental elastic, 3 hardtekk */
    uint8_t bass_algo;   /* bass voice: 0 classic, 1 B01 kick-derived, 2 B02 rubber FM, 3 B03 tribe saw */
    uint8_t lead_algo;   /* lead voice: 0 classic, 1 L01 hollow, 2 L02 snake, 3 L05 growl, 4 L07 ping */
    uint8_t song_on;     /* 1 = the transport plays the song chain */
    uint8_t arp_on;      /* 1 = the ARP page's arpeggiator plays */
    uint8_t bass_oct;    /* 0..4 */
    uint8_t lead_oct;    /* 0..4 */
    int32_t knob[4];     /* KNOB 1..4 last values (screen history) */
} cfg_t;

typedef struct {
    uint8_t drum[4][16]; /* lanes 0 kick, 1 snare, 2 hat, 3 perc: bit 0 gate */
    uint8_t vel[6][16];  /* per-step velocity 0..127 (drums scale the level, melodic the loudness) */
    uint8_t gate[6][16]; /* melodic note length in steps 1..4 (ties); TD-3-style GATE lane */
    uint8_t slide[6][16];/* 0/1: glide into this step's note (TD-3-style SLIDE lane, cross-pattern) */
    uint8_t bass[16];    /* 0 = rest, else a scale degree 1..7 */
    uint8_t lead[16];    /* 0 = rest, else a scale degree 1..7 */
} pattern_t;

static cfg_t cfg;
static pattern_t ed_pat;          /* the pattern the UI edits (main loop) */
static pattern_t pat_bank[SONG_NPAT];     /* the song's patterns */
static uint8_t song_order[SONG_NPAT];      /* chain: bank indexes in play order */
static uint8_t song_len;                  /* chain length (0 = no song) */
static int8_t st_status;          /* 1 SAVED, -1 FAILED, 0 nothing */

static void cfg_defaults(void)
{
    cfg.root = 2;
    cfg.scale = 0;
    cfg.bpm = 165;
    cfg.swing = 55;
    cfg.master = 110;
    cfg.pat = 0;
    cfg.bass_cut = 80;
    cfg.bass_reso = 60;
    cfg.bass_drive = 45;
    cfg.bass_accent = 90;
    cfg.bass_slide = 70;
    cfg.lead_detune = 14;
    cfg.lead_lfo = 50;
    cfg.lead_cut = 90;
    cfg.lead_level = 100;
    cfg.kick_decay = 85;
    cfg.drum_level = 110;
    cfg.drum_hat = 100;
    cfg.drum_perc = 105;
    cfg.fx_delay = 0;               /* sends default to zero: an FX tail must be asked for */
    cfg.fx_reverb = 0;
    cfg.fx_drive = 30;
    cfg.fx_type = 0;
    cfg.arp_style = 0;
    cfg.arp_rate = 4;
    cfg.arp_oct = 1;
    cfg.arp_rhythm = 0;
    cfg.midi_clk = 1;
    cfg.style = 0;
    cfg.kit = 0;
    cfg.bass_algo = 0;
    cfg.lead_algo = 0;
    cfg.song_on = 0;
    cfg.arp_on = 0;
    cfg.bass_oct = 1;
    cfg.lead_oct = 1;
    memset(cfg.knob, 0, sizeof cfg.knob);
}

/* clamp every field that can be out of range (an older version, another firmware, a damaged sector) */
static void cfg_clamp(void)
{
    if (cfg.root > 11u) cfg.root = 0;
    if (cfg.scale >= NSCALES) cfg.scale = 0;
    if (cfg.bpm < 130u) cfg.bpm = 130;
    if (cfg.bpm > 200u) cfg.bpm = 200;
    if (cfg.swing > 100u) cfg.swing = 55;
    if (cfg.master > 127u) cfg.master = 110;
    if (cfg.pat >= NPAT) cfg.pat = 0;
    if (cfg.bass_cut > 127u) cfg.bass_cut = 80;
    if (cfg.bass_reso > 127u) cfg.bass_reso = 60;
    if (cfg.bass_drive > 127u) cfg.bass_drive = 45;
    if (cfg.bass_accent > 127u) cfg.bass_accent = 90;
    if (cfg.bass_slide > 127u) cfg.bass_slide = 70;
    if (cfg.lead_detune > 100u) cfg.lead_detune = 14;
    if (cfg.lead_lfo > 127u) cfg.lead_lfo = 50;
    if (cfg.lead_cut > 127u) cfg.lead_cut = 90;
    if (cfg.lead_level > 127u) cfg.lead_level = 100;
    if (cfg.kick_decay > 127u) cfg.kick_decay = 85;
    if (cfg.drum_level > 127u) cfg.drum_level = 110;
    if (cfg.drum_hat > 127u) cfg.drum_hat = 100;
    if (cfg.drum_perc > 127u) cfg.drum_perc = 105;
    if (cfg.fx_delay > 127u) cfg.fx_delay = 0;
    if (cfg.fx_reverb > 127u) cfg.fx_reverb = 0;
    if (cfg.fx_drive > 127u) cfg.fx_drive = 30;
    if (cfg.fx_type > 1u) cfg.fx_type = 0;
    if (cfg.arp_style > 7u) cfg.arp_style = 0;
    if (cfg.arp_rate > 127u) cfg.arp_rate = 4;
    if (cfg.arp_oct > 4u) cfg.arp_oct = 1;
    if (cfg.arp_rhythm > 23u) cfg.arp_rhythm = 0;
    if (cfg.midi_clk > 1u) cfg.midi_clk = 1;
    if (cfg.style > 2u) cfg.style = 0;
    if (cfg.kit > 3u) cfg.kit = 0;
    if (cfg.bass_algo > 3u) cfg.bass_algo = 0;
    if (cfg.lead_algo > 4u) cfg.lead_algo = 0;
    if (cfg.song_on > 1u) cfg.song_on = 0;
    if (cfg.arp_on > 1u) cfg.arp_on = 0;
    if (cfg.bass_oct > 4u) cfg.bass_oct = 1;
    if (cfg.lead_oct > 4u) cfg.lead_oct = 1;
}

static void cfg_load(void)
{
    cfg_defaults();
    if (store_load(CFG_SLOT, APP_ID, &cfg, sizeof cfg) == (int32_t)sizeof cfg)
        cfg_clamp();
}

static int cfg_save(void)
{
    cfg_clamp();
    st_status = store_save(CFG_SLOT, APP_ID, &cfg, sizeof cfg) ? -1 : 1;
    return st_status > 0 ? 0 : -1;
}

/* a starter pattern: pounding 4/4 kick, snare on 2 & 4, 16th hats, a tribal metallic 3-cycle,
 * a Phrygian bass line with a slide, a sparse lead (the skeleton's default song) */
static void pat_default(void)
{
    uint8_t s, d;
    memset(&ed_pat, 0, sizeof ed_pat);
    for (d = 0; d < 6u; d++)
        for (s = 0; s < 16u; s++)
            ed_pat.gate[d][s] = 1;
    for (s = 0; s < 16u; s++) {
        if ((s & 3u) == 0u) {
            ed_pat.drum[0][s] = 1u;
            ed_pat.vel[0][s] = 124;
        }
        if (s & 1u) {
            ed_pat.drum[2][s] = 1u;
            ed_pat.vel[2][s] = 96;
        }
        if (s == 4u || s == 12u) {
            ed_pat.drum[1][s] = 1u;
            ed_pat.vel[1][s] = 120;
        }
        if (s % 3u == 2u) {
            ed_pat.drum[3][s] = 1u;
            ed_pat.vel[3][s] = s == 5u ? 118 : 104;
        }
        if (s == 0u || s == 8u) {
            ed_pat.bass[s] = 1u;                         /* root */
            ed_pat.vel[4][s] = 115;
        } else if (s == 3u || s == 11u) {
            ed_pat.bass[s] = 2u;                         /* b2 (weeping tension) */
            ed_pat.vel[4][s] = 102;
        } else if (s == 15u) {
            ed_pat.bass[s] = 5u;                         /* fifth */
            ed_pat.vel[4][s] = 115;
        }
        if (s == 2u || s == 10u) {
            ed_pat.lead[s] = 4u;                         /* fourth */
            ed_pat.vel[5][s] = 100;
        } else if (s == 6u) {
            ed_pat.lead[s] = 6u;                         /* b6 */
            ed_pat.vel[5][s] = 96;
        } else if (s == 14u) {
            ed_pat.lead[s] = 1u;                         /* root */
            ed_pat.vel[5][s] = 110;
        }
    }
}

static void pat_load(void)
{
    uint8_t s, d;
    pat_default();
    if (store_load(PAT_SLOT, APP_ID, &ed_pat, sizeof ed_pat) != (int32_t)sizeof ed_pat)
        return;
    for (d = 0; d < 4u; d++)
        for (s = 0; s < 16u; s++)
            ed_pat.drum[d][s] &= 1u;
    for (d = 0; d < 6u; d++)
        for (s = 0; s < 16u; s++) {
            ed_pat.vel[d][s] &= 0x7Fu;
            if (ed_pat.gate[d][s] < 1u || ed_pat.gate[d][s] > 4u)
                ed_pat.gate[d][s] = 1;
            ed_pat.slide[d][s] &= 1u;
        }
    for (s = 0; s < 16u; s++) {
        if (ed_pat.bass[s] > 7u) ed_pat.bass[s] = 0;
        if (ed_pat.lead[s] > 7u) ed_pat.lead[s] = 0;
    }
}

static int pat_save(void)
{
    st_status = store_save(PAT_SLOT, APP_ID, &ed_pat, sizeof ed_pat) ? -1 : 1;
    return st_status > 0 ? 0 : -1;
}

/* -------------------------------------------------- the song bank and chain ---- */
static void song_default(void)
{
    uint8_t i;
    song_len = 0;
    for (i = 0; i < SONG_NPAT; i++)
        song_order[i] = i;
}

static void song_load(void)
{
    struct {
        uint8_t order[SONG_NPAT];
        uint8_t len;
    } s;
    song_default();
    if (store_load(SONG_SLOT, APP_ID, &s, sizeof s) == (int32_t)sizeof s) {
        uint8_t i;
        for (i = 0; i < SONG_NPAT && i < s.len; i++)
            song_order[i] = s.order[i] < SONG_NPAT ? s.order[i] : i;
        song_len = s.len > SONG_NPAT ? SONG_NPAT : s.len;
        for (i = 0; i < SONG_NPAT; i++)
            if (store_load(PAT_SLOT + 1u + i, APP_ID, &pat_bank[i], sizeof pat_bank[i]) !=
                (int32_t)sizeof pat_bank[i])
                pat_default();
    }
}

static int song_save(void)
{
    struct {
        uint8_t order[SONG_NPAT];
        uint8_t len;
    } s;
    uint8_t i;
    for (i = 0; i < SONG_NPAT; i++)
        s.order[i] = song_order[i];
    s.len = song_len;
    st_status = store_save(SONG_SLOT, APP_ID, &s, sizeof s) ? -1 : 1;
    for (i = 0; i < SONG_NPAT; i++)
        if (store_save(PAT_SLOT + 1u + i, APP_ID, &pat_bank[i], sizeof pat_bank[i]))
            st_status = -1;
    return st_status > 0 ? 0 : -1;
}

static void data_init(void)
{
    st_status = 0;
    cfg_load();
    pat_load();
    song_load();
}
