/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* gen.c - the GEN page's engine: the SANTO CENTER musical grammar written straight into the
 * sequencer's patterns (ed_pat, and the song bank pat_bank). No files, no DAW - it is the
 * instrument. The grammar: a 4/4 pounding kick with rest masks for the other voices, accents on
 * the syncopated hits, tribal metallic 3-cycle / strange 5-cycle polyrhythms, 2-4-note melodic
 * cells from the selected scale (root, b2, b3, 5th), and a phrase that resolves its last note
 * to the root. Styles: 0 mental, 1 tribe, 2 hardtekk. gen_song() fills the song bank with the
 * TD-3-style A/A'/B/B' cell chain (SANTO CENTER's track chain). */

#define GEN_KICK 0u
#define GEN_SNARE 1u
#define GEN_HAT 2u
#define GEN_PERC 3u
#define GEN_BASS 4u
#define GEN_LEAD 5u

static void gen_clear(pattern_t *p)
{
    memset(p, 0, sizeof *p);
}

/* a drum step: gate on, velocity */
static void gen_drum(pattern_t *p, uint8_t lane, uint8_t step, uint8_t vel)
{
    p->drum[lane][step] = 1u;
    p->vel[lane][step] = vel;
}

/* one pattern in a style; fills *p (a fresh cell). */
static void gen_one(pattern_t *p, uint8_t style, uint8_t variant)
{
    uint8_t s;
    uint8_t bass[16], lead[16];
    gen_clear(p);

    for (s = 0; s < 16u; s += 4u)
        gen_drum(p, GEN_KICK, s, 124);
    if (style == 2u)
        gen_drum(p, GEN_KICK, 14, 105);

    gen_drum(p, GEN_SNARE, 4, 120);
    gen_drum(p, GEN_SNARE, 12, 120);
    if (style == 2u)
        gen_drum(p, GEN_SNARE, 8, 102);

    for (s = 1; s < 16u; s += 2u)
        gen_drum(p, GEN_HAT, s, 96);
    if (style == 2u)
        for (s = 0; s < 16u; s += 2u)
            gen_drum(p, GEN_HAT, s, s == 8u ? 108 : 92);

    if (style != 1u)
        for (s = 2; s < 16u; s += 3u)
            gen_drum(p, GEN_PERC, s, s == 5u ? 118 : 104);
    if (style == 1u || style == 2u)
        for (s = 1; s < 16u; s += 5u)
            gen_drum(p, GEN_PERC, s, 104);

    /* the melodic cells; variant shifts 2-4 pitches (the A/A'/B/B' trick) */
    if (style == 0u) {
        uint8_t b0[16] = {1, 0, 0, 2, 0, 0, 1, 0, 0, 0, 5, 0, 2, 0, 0, 1};
        uint8_t l0[16] = {0, 0, 4, 0, 0, 0, 0, 6, 0, 0, 0, 4, 0, 0, 1, 0};
        for (s = 0; s < 16u; s++)
            bass[s] = b0[s], lead[s] = l0[s];
    } else if (style == 1u) {
        uint8_t b1[16] = {1, 0, 0, 0, 1, 0, 0, 0, 5, 0, 0, 0, 1, 0, 0, 1};
        uint8_t l1[16] = {0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 6, 0, 0, 0, 2, 0};
        for (s = 0; s < 16u; s++)
            bass[s] = b1[s], lead[s] = l1[s];
    } else {
        uint8_t b2[16] = {1, 0, 2, 0, 1, 0, 5, 0, 2, 0, 1, 0, 2, 0, 5, 1};
        uint8_t l2[16] = {0, 0, 4, 0, 0, 0, 6, 0, 0, 0, 4, 0, 0, 0, 1, 0};
        for (s = 0; s < 16u; s++)
            bass[s] = b2[s], lead[s] = l2[s];
    }
    if (variant & 1u)                       /* A' / B': 2-4 changed pitches */
        bass[3] = 1u, bass[6] = 5u, bass[10] = 1u;
    if (variant & 2u)
        lead[6] = 1u, lead[11] = 5u;
    for (s = 0; s < 16u; s++) {
        p->bass[s] = bass[s];
        p->lead[s] = lead[s];
        p->vel[GEN_BASS][s] = !bass[s] ? 0u : ((s == 0u || s == 15u) ? 115u : 100u);
        p->vel[GEN_LEAD][s] = !lead[s] ? 0u : 100u;
        p->gate[GEN_BASS][s] = 1;
        p->gate[GEN_LEAD][s] = (lead[s] && s == 6u && style == 0u) ? 2u : 1u;
        p->slide[GEN_BASS][s] = (style == 0u && (s == 3u || s == 11u)) ||
                                (style == 1u && s == 7u) ||
                                (style == 2u && (s == 4u || s == 14u)) ? 1u : 0u;
        p->slide[GEN_LEAD][s] = 0;
    }
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
