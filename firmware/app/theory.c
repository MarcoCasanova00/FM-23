/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* theory.c - the genre-appropriate pattern grammar behind GEN (docs/SOUND_MISSION.md §6,8).
 * The sequencer plays scale degrees (data.c SCALE_IV). This module maps the musical grammar to
 * cells:
 *
 *  MENTAL (0): Phrygian-dominant cell - root / b2 / b3 / 5, sparse 2-4-note bass, rest
 *    masks aligned to the 4/4 kick, b2 as the "weeping" tension, metallic 3-cycle percussion.
 *  TRIBE (1): rolling tribe - more root/5th motion, 5-cycle "strange" percussion, offbeat
 *    bounce, airy hat bed.
 *  HARDTEKK (2): root / b2 / octave emphasis, faster motion, pickup kick, denser hats.
 *
 * Everything is deterministic (no RNG) so tests and users get reproducible results. Variants
 * A'/B' shift 2-4 pitches (the track-chain principle). A phrase always resolves its last note
 * to the root. */

#define TH_BASS 4u
#define TH_LEAD 5u

static void th_drum(pattern_t *p, uint8_t lane, uint8_t step, uint8_t vel)
{
    p->drum[lane][step] = 1u;
    p->vel[lane][step] = vel;
}

/* deterministic PRNG seeded by cfg.seed (0 = canonical, no variation) */
static uint32_t th_rng;
static void th_seed(uint32_t s) { th_rng = s ? s : 0x12345678u; }
static uint32_t th_rand(void)
{
    th_rng ^= th_rng << 13u;
    th_rng ^= th_rng >> 17u;
    th_rng ^= th_rng << 5u;
    return th_rng;
}

/* seeded melodic/per-radius variation within the genre grammar; always keeps the root
 * foundation and the resolving final step (s == 0 and s == 15 are never changed). */
static void th_vary(pattern_t *p, uint8_t style)
{
    uint8_t s;
    for (s = 1; s < 15u; s++) {
        if (!p->bass[s] || th_rand() % 100u >= 30u)
            continue;
        if (style == 0u) {
            static const uint8_t A[4] = {1, 2, 3, 5};
            p->bass[s] = A[th_rand() % 4u];
        } else if (style == 1u) {
            static const uint8_t B[4] = {1, 4, 5, 6};
            p->bass[s] = B[th_rand() % 4u];
        } else {
            static const uint8_t C[3] = {1, 2, 5};
            p->bass[s] = C[th_rand() % 3u];
        }
    }
    for (s = 1; s < 15u; s++) {
        if (!p->lead[s] || th_rand() % 100u >= 25u)
            continue;
        {
            static const uint8_t D[4] = {1, 4, 5, 6};
            p->lead[s] = D[th_rand() % 4u];
        }
    }
    for (s = 0; s < 16u; s++) {
        if (p->vel[2][s]) {
            int32_t v = (int32_t)p->vel[2][s] + ((int32_t)(th_rand() % 13u) - 6);
            p->vel[2][s] = v < 60 ? 60 : v > 127 ? 127 : (uint8_t)v;
        }
    }
}

static void th_fill(pattern_t *p, uint8_t style, uint8_t variant)
{
    uint8_t s;
    uint8_t bass[16], lead[16];
    memset(p, 0, sizeof *p);
    th_seed(cfg.seed);

    /* ---- drums: the 4/4 anchor + genre percussion ---- */
    for (s = 0; s < 16u; s += 4u)
        th_drum(p, 0, s, 124);                     /* kick, accented */
    if (style == 2u)
        th_drum(p, 0, 14, 105);                    /* hardtekk pickup kick */
    th_drum(p, 1, 4, 120);
    th_drum(p, 1, 12, 120);
    if (style == 2u)
        th_drum(p, 1, 8, 102);
    for (s = 1; s < 16u; s += 2u)
        th_drum(p, 2, s, 96);                      /* offbeat hats */
    if (style == 2u)
        for (s = 0; s < 16u; s += 2u)
            th_drum(p, 2, s, s == 8u ? 108 : 92);
    /* polyrhythmic percussion: tribal 3-cycle (mental/hardtekk), strange 5-cycle (tribe) */
    if (style != 1u)
        for (s = 2; s < 16u; s += 3u)
            th_drum(p, 3, s, s == 5u ? 118 : 104);
    if (style == 1u || style == 2u)
        for (s = 1; s < 16u; s += 5u)
            th_drum(p, 3, s, 104);

    /* ---- melodic grammar ---- */
    if (style == 0u) {
        /* Mental: root / b2 / b3 / 5; rest at the kick's offbeats; b2 slides at 3, 11 */
        uint8_t b0[16] = {1, 0, 0, 2, 0, 0, 1, 0, 0, 0, 5, 0, 2, 0, 0, 1};
        uint8_t l0[16] = {0, 0, 4, 0, 0, 0, 0, 6, 0, 0, 0, 4, 0, 0, 1, 0};
        for (s = 0; s < 16u; s++)
            bass[s] = b0[s], lead[s] = l0[s];
    } else if (style == 1u) {
        /* Tribe: root / 5 motion, airy; 5-cycle percussion carries the pull */
        uint8_t b1[16] = {1, 0, 0, 0, 1, 0, 0, 0, 5, 0, 0, 0, 1, 0, 0, 1};
        uint8_t l1[16] = {0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 6, 0, 0, 0, 2, 0};
        for (s = 0; s < 16u; s++)
            bass[s] = b1[s], lead[s] = l1[s];
    } else {
        /* Hardtekk: root / b2 / 5, busier; pickup prepares the turn */
        uint8_t b2[16] = {1, 0, 2, 0, 1, 0, 5, 0, 2, 0, 1, 0, 2, 0, 5, 1};
        uint8_t l2[16] = {0, 0, 4, 0, 0, 0, 6, 0, 0, 0, 4, 0, 0, 0, 1, 0};
        for (s = 0; s < 16u; s++)
            bass[s] = b2[s], lead[s] = l2[s];
    }
    /* variants A'/B': 2-4 changed pitches */
    if (variant & 1u)
        bass[3] = 1u, bass[6] = 5u, bass[10] = 1u;
    if (variant & 2u)
        lead[6] = 1u, lead[11] = 5u;

    /* velocities, gate (ties), and TD-3-style slides; the phrase resolves to the root */
    for (s = 0; s < 16u; s++) {
        p->bass[s] = bass[s];
        p->lead[s] = lead[s];
        p->vel[TH_BASS][s] = !bass[s] ? 0u : ((s == 0u || s == 15u) ? 115u : 100u);
        p->vel[TH_LEAD][s] = !lead[s] ? 0u : 100u;
        p->gate[TH_BASS][s] = 1;
        p->gate[TH_LEAD][s] = (lead[s] && s == 6u && style == 0u) ? 2u : 1u;
        p->slide[TH_BASS][s] = (style == 0u && (s == 3u || s == 11u)) ||
                                (style == 1u && s == 7u) ||
                                (style == 2u && (s == 4u || s == 14u)) ? 1u : 0u;
        p->slide[TH_LEAD][s] = 0;
    }
    /* seeded variation runs on the finished cell, so bass/lead are actually varied */
    if (cfg.seed)
        th_vary(p, style);
}
