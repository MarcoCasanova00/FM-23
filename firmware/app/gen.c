/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* gen.c - the GEN page's engine: the SANTO CENTER musical grammar written straight into the
 * sequencer's active pattern (ed_pat). No files, no DAW - it is the instrument. The grammar:
 * a 4/4 pounding kick with rest masks for the other voices, accents on the syncopated hits,
 * tribal metallic 3-cycle / strange 5-cycle polyrhythms, 2-4-note melodic cells from the
 * selected scale (root, b2, b3, 5th), and a phrase that resolves its last note to the root.
 * Styles: 0 mental, 1 tribe, 2 hardtekk. The UI (ui.c PG_GEN) calls gen_pattern() and asks the
 * sequencer for a copy (seq_request_pattern_copy). */

#define GEN_KICK 0u
#define GEN_SNARE 1u
#define GEN_HAT 2u
#define GEN_PERC 3u

static void gen_clear(void)
{
    memset(&ed_pat, 0, sizeof ed_pat);
}

/* a drum step: 0 rest, 1 gate, 3 gate + accent */
static void gen_drum(uint8_t lane, uint8_t step, uint8_t acc)
{
    ed_pat.drum[lane][step] = acc ? 3u : 1u;
}

static void gen_pattern(void)
{
    uint8_t s;
    gen_clear();

    /* the 4/4 kick, always; hardtekk adds a driving pickup before the bar turn */
    for (s = 0; s < 16u; s += 4u)
        gen_drum(GEN_KICK, s, 1);
    if (cfg.style == 2u)
        gen_drum(GEN_KICK, 14, 0);

    /* snare on 2 and 4; hardtekk also on the half-bar */
    gen_drum(GEN_SNARE, 4, 1);
    gen_drum(GEN_SNARE, 12, 1);
    if (cfg.style == 2u)
        gen_drum(GEN_SNARE, 8, 0);

    /* hats: 16th offbeats (mental/hardtekk) or a rolling 8th bed (tribe) */
    for (s = 1; s < 16u; s += 2u)
        gen_drum(GEN_HAT, s, 0);
    if (cfg.style == 2u)
        for (s = 0; s < 16u; s += 2u)
            gen_drum(GEN_HAT, s, s == 8u);

    /* polyrhythmic percussion: tribal metallic 3-cycle, strange 5-cycle on the odd steps */
    if (cfg.style != 1u) {
        for (s = 2; s < 16u; s += 3u)
            gen_drum(GEN_PERC, s, s == 5u);
    }
    if (cfg.style == 1u || cfg.style == 2u) {
        for (s = 1; s < 16u; s += 5u)
            gen_drum(GEN_PERC, s, 0);
    }

    /* the melodic cells: the bass is the 303-voice (2-4-note cells, rest masks where the kick
     * pounds, slide resolved and the phrase ends on the root); the lead sings a sparse phrase */
    if (cfg.style == 0u) {
        uint8_t b[16] = {1, 0, 0, 2, 0, 0, 1, 0, 0, 0, 5, 0, 2, 0, 0, 1};
        uint8_t l[16] = {0, 0, 4, 0, 0, 0, 0, 6, 0, 0, 0, 4, 0, 0, 1, 0};
        for (s = 0; s < 16u; s++) {
            ed_pat.bass[s] = b[s];
            ed_pat.lead[s] = l[s];
        }
    } else if (cfg.style == 1u) {
        uint8_t b[16] = {1, 0, 0, 0, 1, 0, 0, 0, 5, 0, 0, 0, 1, 0, 0, 1};
        uint8_t l[16] = {0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 6, 0, 0, 0, 2, 0};
        for (s = 0; s < 16u; s++) {
            ed_pat.bass[s] = b[s];
            ed_pat.lead[s] = l[s];
        }
    } else {
        uint8_t b[16] = {1, 0, 2, 0, 1, 0, 5, 0, 2, 0, 1, 0, 2, 0, 5, 1};
        uint8_t l[16] = {0, 0, 4, 0, 0, 0, 6, 0, 0, 0, 4, 0, 0, 0, 1, 0};
        for (s = 0; s < 16u; s++) {
            ed_pat.bass[s] = b[s];
            ed_pat.lead[s] = l[s];
        }
    }
}
