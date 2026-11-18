/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* gen.c - the GEN page: the SANTO CENTER grammar written straight into the sequencer's patterns.
 * The musical rules live in theory.c (th_fill); this file turns them into cells and the song
 * chain (the TD-3 A/A'/B/B' track-chain order, with cross-pattern legato armed). */

#define GEN_BASS 4u
#define GEN_LEAD 5u

/* style-appropriate sound per cell: the song chain switches engines/kits per section */
static void gen_snd(pattern_t *p, uint8_t style)
{
    static const uint8_t SB[3] = {4, 3, 1};   /* bass engines: acid / tribe saw / kick-derived */
    static const uint8_t SL[3] = {7, 2, 6};   /* lead engines: scream / snake / sync razor */
    static const uint8_t SK[3] = {2, 1, 3};   /* kits: mental / tribe / hardtekk */
    static const uint8_t SM[3] = {0, 1, 3};   /* macros: off / rubber / scream */
    p->snd_bass = SB[style];
    p->snd_lead = SL[style];
    p->snd_kit = SK[style];
    p->snd_macro = SM[style];
}

static void gen_one(pattern_t *p, uint8_t style, uint8_t variant)
{
    th_fill(p, style, variant);
    gen_snd(p, style);
}

static void gen_pattern(void)
{
    gen_one(&ed_pat, cfg.style, 0);
}

/* the TD-3 Mental Tekno track chain: A A' B A' B' A B' A' (8 cells) */
static void gen_song(void)
{
    static const uint8_t ORDER[SONG_NPAT] = {0, 1, 2, 1, 3, 0, 2, 1};
    static const uint8_t STYLE[SONG_NPAT] = {0, 1, 2, 1, 2, 0, 2, 1};
    static const uint8_t VAR[SONG_NPAT] = {0, 1, 0, 1, 3, 0, 2, 1};
    uint8_t i;
    for (i = 0; i < SONG_NPAT; i++) {
        gen_one(&pat_bank[i], STYLE[i], VAR[i]);
        song_order[i] = ORDER[i] < SONG_NPAT ? ORDER[i] : i;
        if (i)
            pat_bank[i].slide[GEN_BASS][0] = 1;   /* cross-pattern legato into the new cell */
    }
    song_len = SONG_NPAT;
    cfg.song_on = 1;
    gen_one(&ed_pat, STYLE[ORDER[0]], VAR[ORDER[0]]);
}
