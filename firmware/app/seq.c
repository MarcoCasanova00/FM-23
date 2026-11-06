/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* seq.c - FM-23's pattern sequencer. It runs in the audio ISR (counts samples, fires the steps
 * of the active pattern) and reads a private copy `pat_cur` of the 16-step pattern; the main loop
 * edits `ed_pat` (data.c) and asks for a copy with `seq_request_pattern_copy()` (volatile request,
 * taken at a block start). Transport (play/rec) is the same: volatile requests from the main loop.
 * Melodic steps are scale degrees; fire() maps them to MIDI notes through cfg.root/scale and the
 * octave settings. */

#define SEQ_STEPS 16u
#define SEQ_BASS_OCT 1u

static pattern_t pat_cur;
static volatile uint8_t pat_swap_req, seq_play_req, seq_rec_req;

static struct {
    uint32_t pos;          /* sample position inside the current 16th */
    uint8_t step;          /* 0..15, the step about to play */
    uint8_t play;
    uint8_t rec;
} seq;

static uint8_t bass_prev_active;

/* fire one step: the drum gates, the bass degree (legato glide on), the lead degree */
static void seq_fire(uint8_t step)
{
    uint8_t d;
    for (d = 0; d < 4u; d++)
        if (pat_cur.drum[d][step] & 1u)
            drum_start(d);
    {
        uint8_t b = pat_cur.bass[step];
        if (b && b <= 7u) {
            uint8_t root = cfg.root;
            uint8_t iv = SCALE_IV[cfg.scale][b - 1u];
            uint8_t n = (uint8_t)(36 + root + iv + 12u * cfg.bass_oct);
            bass_note(n, 112, bass_prev_active);
            bass_prev_active = 1;
        } else {
            bass_prev_active = 0;
        }
    }
    {
        uint8_t l = pat_cur.lead[step];
        if (l && l <= 7u) {
            uint8_t root = cfg.root;
            uint8_t iv = SCALE_IV[cfg.scale][l - 1u];
            uint8_t n = (uint8_t)(60 + root + iv + 12u * cfg.lead_oct);
            lead_note(n, 105);
        }
    }
}

static void seq_start(void)
{
    seq.play = 1;
    seq.pos = 0;
    seq.step = 0;
    bass_prev_active = 0;
    seq_fire(0);
}

static void seq_stop(void)
{
    seq.play = 0;
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

static __attribute__((noinline)) void seq_clock(uint32_t n)
{
    uint32_t len;
    if (!seq.play)
        return;
    len = (uint32_t)FS * 60u / cfg.bpm / 4u;   /* samples per 16th */
    if (!len)
        len = 1;
    seq.pos += n;
    while (seq.pos >= len) {
        seq.pos -= len;
        seq.step = (uint8_t)((seq.step + 1u) & (SEQ_STEPS - 1u));
        seq_fire(seq.step);
    }
}

static uint8_t seq_playing(void) { return seq.play; }
static uint8_t seq_step_now(void) { return seq.step; }
static uint8_t seq_rec(void) { return seq.rec; }

static void seq_request_pattern_copy(void) { pat_swap_req = 1; }
static void seq_toggle_play(void) { seq_play_req = 1; }
static void seq_toggle_rec(void) { seq_rec_req = 1; }
