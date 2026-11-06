/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* seq.c - FM-23's pattern sequencer. It runs in the audio ISR (counts samples, fires the steps
 * of the active pattern) and reads a private copy `pat_cur` of the 16-step pattern; the main loop
 * edits `ed_pat` (data.c) and asks for a copy with `seq_request_pattern_copy()` (volatile request,
 * taken at a block start). Transport (play/rec) is the same: volatile requests from the main loop.
 * Melodic steps are scale degrees; fire() maps them to MIDI notes through cfg.root/scale and the
 * octave settings, and reads each step's velocity. In song mode the chain advances one cell per
 * bar and loads the next pattern from the bank; the ARP plays over the lead lane when on. With
 * cfg.midi_clk the transport follows incoming MIDI clock (F8) instead of the internal tempo. */

#define SEQ_STEPS 16u
#define SEQ_CLK_16TH 6u            /* 24 MIDI clocks per quarter, a 16th is 6 */

static pattern_t pat_cur;
static volatile uint8_t pat_swap_req, seq_play_req, seq_rec_req;

static struct {
    uint32_t pos;          /* sample position inside the current 16th */
    uint8_t step;          /* 0..15, the step about to play */
    uint8_t play;
    uint8_t rec;
    uint8_t chain_idx;     /* where in the song chain we are */
    uint8_t arp_phase;
    uint32_t clk;          /* MIDI clock ticks since the last 16th */
    uint32_t ext_age;      /* samples since the last MIDI clock */
    uint8_t use_ext;       /* 1 = the transport follows MIDI clock */
    int32_t aut_cut, aut_lfo;   /* per-step automation offsets (0 = none) */
} seq;

static uint8_t bass_prev_active;

static void seq_bank_load(uint8_t idx)
{
    pat_cur = pat_bank[song_order[idx] < SONG_NPAT ? song_order[idx] : 0u];
}

/* the arpeggiator: the next chord tone over the lead lane (up / down / up-down / random) */
static void seq_arp_note(uint8_t step)
{
    uint8_t deg[4] = {1, 3, 5, 7};
    uint8_t n = 1, style = cfg.arp_style;
    if (style == 1u || style == 3u || style == 5u)
        n = deg[3u - (seq.arp_phase % 4u)];
    else if (style == 2u || style == 6u) {
        uint8_t p = seq.arp_phase % 6u;
        n = deg[p < 3u ? p : 5u - p];
    } else if (style == 4u)
        n = deg[seq.arp_phase % 4u];
    else
        n = deg[(seq.arp_phase + step) % 4u];
    seq.arp_phase++;
    {
        uint8_t root = cfg.root, iv = SCALE_IV[cfg.scale][n - 1u];
        uint8_t note = (uint8_t)(60 + root + iv + 12u * cfg.arp_oct);
        lead_note_src(note, 100, 0);
    }
}

/* fire one step: the drum gates (velocity-scaled), the bass degree (legato glide on), lead / arp */
static void seq_fire(uint8_t step)
{
    uint8_t d;
    for (d = 0; d < 4u; d++)
        if (pat_cur.drum[d][step] & 1u) {
            uint8_t v = pat_cur.vel[d][step] ? pat_cur.vel[d][step] : 96u;
            drum_trigger(d, v);
            if (!cfg.kit && v != 96u)
                drumv[d].amp = (int32_t)((int64_t)drumv[d].amp * (64u + v) / 160u);
        }
    {
        uint8_t b = pat_cur.bass[step];
        if (b && b <= 7u) {
            uint8_t root = cfg.root;
            uint8_t iv = SCALE_IV[cfg.scale][b - 1u];
            uint8_t n = (uint8_t)(36 + root + iv + 12u * cfg.bass_oct);
            bass_note_src(n, pat_cur.vel[4][step] ? pat_cur.vel[4][step] : 96u,
                      pat_cur.slide[4][step], 0);      /* TD-3-style SLIDE lane */
            bass_prev_active = 1;
        } else if (cfg.arp_on && cfg.arp_hold) {
            /* ARP HOLD: a constant ominous root drone under the beats */
            uint8_t root = cfg.root;
            uint8_t n = (uint8_t)(36 + root + 12u * cfg.bass_oct);
            bass_note_src(n, 72, bass_prev_active, 0);
            bass_prev_active = 1;
        } else {
            bass_prev_active = 0;
        }
    }
    if (cfg.arp_on)
        seq_arp_note(step);
    else {
        uint8_t l = pat_cur.lead[step];
        if (l && l <= 7u) {
            uint8_t root = cfg.root;
            uint8_t iv = SCALE_IV[cfg.scale][l - 1u];
            uint8_t n = (uint8_t)(60 + root + iv + 12u * cfg.lead_oct);
            lead_note_src(n, pat_cur.vel[5][step] ? pat_cur.vel[5][step] : 96u, 0);
        }
    }
    /* automation: read the per-step value for the selected target (0 when off) */
    seq.aut_cut = seq.aut_lfo = 0;
    if (pat_cur.at_on == 1u)
        seq.aut_cut = (int32_t)pat_cur.am[0][step] - 64;
    else if (pat_cur.at_on == 2u)
        seq.aut_lfo = (int32_t)pat_cur.am[1][step] - 64;
}

/* advance one step; the bar boundary switches the song cell */
static void seq_advance(void)
{
    if (seq.step == 15u && cfg.song_on && song_len) {
        seq.chain_idx = (uint8_t)((seq.chain_idx + 1u) % song_len);
        seq_bank_load(seq.chain_idx);
    }
    seq.step = (uint8_t)((seq.step + 1u) & (SEQ_STEPS - 1u));
    seq_fire(seq.step);
}

static void seq_start(void)
{
    seq.play = 1;
    seq.pos = 0;
    seq.step = 0;
    seq.chain_idx = 0;
    seq.arp_phase = 0;
    seq.clk = 0;
    seq.use_ext = 0;
    bass_prev_active = 0;
    if (cfg.song_on && song_len)
        seq_bank_load(0);
    else
        pat_cur = ed_pat;          /* a plain PLAY plays the edited pattern (also on a fresh boot) */
    seq_fire(0);
}

static void seq_stop(void)
{
    seq.play = 0;
    seq.use_ext = 0;
    sound_all_off();
}

/* take the main loop's requests at the start of a block */
static void seq_take(void)
{
    if (pat_swap_req) {
        pat_swap_req = 0;
        pat_cur = ed_pat;
    }
    if (seq_play_req) {
        seq_play_req = 0;
        if (seq.play)
            seq_stop();
        else
            seq_start();
    }
    if (seq_rec_req) {
        seq_rec_req = 0;
        seq.rec ^= 1u;
    }
}

/* one MIDI clock (F8): 6 ticks = a 16th; the transport follows once they arrive */
static void seq_midi_clock(void)
{
    if (!cfg.midi_clk)
        return;
    seq.ext_age = 0;
    seq.use_ext = 1;
    if (!seq.play)
        return;
    seq.clk++;
    if (seq.clk >= SEQ_CLK_16TH) {
        seq.clk = 0;
        seq_advance();
    }
}

static void seq_ext_start(void) { seq_start(); }
static void seq_ext_stop(void) { seq_stop(); }

static __attribute__((noinline)) void seq_clock(uint32_t n)
{
    uint32_t len;
    if (!seq.play)
        return;
    if (seq.use_ext) {              /* MIDI clock drives; fall back if it stops */
        seq.ext_age += n;
        if (seq.ext_age > (uint32_t)FS * 2u)
            seq.use_ext = 0;
        else
            return;
    }
    len = (uint32_t)FS * 60u / cfg.bpm / 4u;   /* samples per 16th */
    if (!len)
        len = 1;
    if (cfg.swing != 50u) {                     /* swing: shift the offbeats */
        int32_t d = (int32_t)((int64_t)len * (cfg.swing > 50u ? cfg.swing - 50u : 50u - cfg.swing) / 200u);
        if (seq.step & 1u)
            len = len > (uint32_t)d ? len - (uint32_t)d : 1u;
        else
            len += (uint32_t)d;
    }
    seq.pos += n;
    while (seq.pos >= len) {
        seq.pos -= len;
        seq_advance();
    }
}

static int32_t seq_aut_cut(void) { return seq.aut_cut; }
static int32_t seq_aut_lfo(void) { return seq.aut_lfo; }
static uint8_t seq_playing(void) { return seq.play; }
static uint8_t seq_step_now(void) { return seq.step; }
static uint8_t seq_rec(void) { return seq.rec; }
static uint8_t seq_chain_idx(void) { return seq.chain_idx; }
static uint8_t seq_use_ext(void) { return seq.use_ext; }
static uint8_t seq_step_vel(uint8_t lane, uint8_t step) { return pat_cur.vel[lane][step]; }

static void seq_request_pattern_copy(void) { pat_swap_req = 1; }
static void seq_toggle_play(void) { seq_play_req = 1; }
static void seq_toggle_rec(void) { seq_rec_req = 1; }
