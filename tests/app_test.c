/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* Host test of the FM-23 app (firmware/app/) on the host platform (tests/host_platform.h):
 * every page drawn and the layout lint clean, lazy redraw, the LEDs, the stored settings and the
 * active pattern (SAVE, power-on, another app's data, out-of-range values), the transport
 * (play/stop), and the sound (bass key pitch, lead pitch, drums, note-off, a lost MIDI stream,
 * app_audio_stop, no clicks). Screens in build/shots/. A template: change these checks with the
 * app and prove each new one can fail (break the code once, watch it go red).
 *   cc -O1 -Ibuild/gen -Ifirmware/src -Ifirmware/app -Itests -o build/host/app_test tests/app_test.c -lm
 *   build/host/app_test */
#include "host_platform.h"
#include <math.h>

static int failed;
static void check(const char *what, int ok)
{
    printf("%-60s %s\n", what, ok ? "ok" : "FAIL");
    failed += !ok;
}

static uint32_t ink_rows(uint32_t y0, uint32_t y1)   /* rows in y0..y1 with anything but the background */
{
    uint32_t x, y, n = 0;
    for (y = y0; y < y1; y++)
        for (x = 0; x < 240u; x++)
            if (host_fb[y * 240u + x] != T_BG) {
                n++;
                break;
            }
    return n;
}

static void clean_input(void)
{
    uint32_t i;
    fm1_in.notes = fm1_in.buttons = 0;
    for (i = 0; i < 7u; i++)
        fm1_in.enc_steps[i] = 0;
}

static void press_label(uint32_t label, int down)
{
    host_press(label, down);
}

static void tap_label(uint32_t label)
{
    press_label(label, 1);
    host_frame();
    press_label(label, 0);
    host_frame();
}

static void go_page(uint32_t pg)
{
    static const uint32_t B_PAGE[7] = {B_HOME, B_SEQ, B_ARP, B_EDIT, B_FX, B_GLO, B_SCL};
    tap_label(B_PAGE[pg]);
}

/* ---------------------------------------------------------------- screen --- */
static void screen(void)
{
    uint32_t pg;
    clean_input();
    host_boot();
    host_frame();
    check("boot: the name drawn (large font)", ink_rows(4, 40) > 5u);
    {
        const char *c = APP_NAME;
        int all = 1;
        for (; *c; c++)
            all &= glyph_at(&AF_L, (uint8_t)*c) >= 0;
        check("boot: every character of APP_NAME is in the large font", all);
    }
    host_png("build/shots/home.png");

    /* widest values, then every page: nothing clipped, a PNG each (transport buttons left out so
     * holding every button does not toggle PLAY/REC while navigating) */
    fm1_in.notes = (1u << 27) - 1u;
    fm1_in.buttons = ((1u << 14) - 1u) & ~panel_mask(B_PLAY) & ~panel_mask(B_REC);
    cfg.bpm = 200;
    cfg.scale = NSCALES - 1u;
    cfg.pat = NPAT - 1u;
    cfg.swing = 100;
    cfg.master = 127;
    cfg.bass_cut = cfg.bass_reso = cfg.bass_drive = cfg.bass_accent = 127;
    cfg.lead_detune = 100;
    cfg.lead_lfo = cfg.lead_cut = cfg.lead_level = 127;
    cfg.kick_decay = cfg.drum_level = cfg.drum_hat = cfg.drum_perc = 127;
    cfg.fx_delay = cfg.fx_reverb = cfg.fx_drive = 127;
    cfg.arp_style = 7;
    cfg.arp_rate = 127;
    cfg.arp_oct = 4;
    cfg.arp_rhythm = 23;
    cfg.midi_clk = 1;
    cfg.bass_oct = cfg.lead_oct = 4;
    lane = 5;
    sel = 3;
    ui_dirty = 1;
    for (pg = 0; pg < 7u; pg++) {
        go_page(pg);
        check("layout: nothing cut or outside its canvas", host_clipped == 0u);
        if (host_clipped)
            printf("  (%u, the last: %s)\n", host_clipped, host_clip_what);
    }
    host_png("build/shots/seq.png");
    fm1_in.notes = fm1_in.buttons = 0;
    clean_input();
    host_frame();                                       /* the cleaned state draws once... */
    host_frame();
    check("lazy: nothing changed, nothing sent to the panel", host_blits == 0u);
    memset(host_fb, 0, sizeof host_fb);                 /* the platform drew over the screen */
    app_redraw = 1;
    host_frame();
    check("app_redraw: the whole screen drawn again", ink_rows(4, 40) > 5u && !app_redraw);

    /* a drum pad key lights its LED and the pad fills with the accent */
    go_page(PG_HOME);
    fm1_in.notes = 1u;                                  /* key 0 = KICK pad */
    host_frame();
    check("a pad held: its LED lit (id 14), its pad accent", host_led[14] && host_fb[96u * 240u + 30u] == T_ACCENT);
    check("a pad held: its button LEDs dark", !host_led[panel.btn[B_PLAY]]);
    host_png("build/shots/home-held.png");
    fm1_in.notes = 0;
    host_frame();
    check("pad released: the LED dark", !host_led[14]);
}

/* -------------------------------------------------------------- GEN --- */
static void gen_test(void)
{
    clean_input();
    host_nor_erase();
    lane = 0;
    ui_dirty = 1;
    host_frame();
    go_page(PG_GEN);
    cfg.root = 2;
    cfg.scale = 0;
    cfg.style = 0;
    sel = 3;
    ui_dirty = 1;
    host_frame();
    host_turn(EN_ALGO, 1);                      /* the GENERATE row */
    host_frame();
    check("GEN: kick 4/4 generated", (ed_pat.drum[0][0] & 1u) && (ed_pat.drum[0][8] & 1u));
    check("GEN: snare on 2 & 4", (ed_pat.drum[1][4] & 1u) && (ed_pat.drum[1][12] & 1u));
    check("GEN: bass cell b2 + root resolution", ed_pat.bass[0] == 1 && ed_pat.bass[3] == 2 &&
          ed_pat.bass[15] == 1);
    {
        int32_t sbuf[2u * CTL];
        host_render(sbuf, CTL);
    }
    check("GEN: the ISR copy follows", (pat_cur.drum[0][0] & 1u) == 1u);
    cfg.style = 2;
    ui_dirty = 1;
    host_frame();
    host_turn(EN_ALGO, 1);
    host_frame();
    check("GEN: hardtekk adds a pickup kick", (ed_pat.drum[0][14] & 1u) == 1u);
}

/* ---------------------------------------------------------------- flash --- */
static void power_cycle(void)                           /* the app's RAM gone, the flash kept */
{
    clean_input();
    memset(&cfg, 0, sizeof cfg);
    memset(&ed_pat, 0, sizeof ed_pat);
    host_boot();
    host_frame();
    go_page(PG_HOME);                                   /* the UI state is not reset at boot */
}

static void persistence(void)
{
    cfg_t wild;
    host_nor_erase();
    clean_input();
    host_boot();
    host_frame();
    go_page(PG_HOME);
    check("boot: the default settings", cfg.bpm == 165 && cfg.root == 2 && cfg.scale == 0);

    host_turn(EN_K1, 5);                               /* HOME knob 1 = tempo */
    host_frame();
    check("a knob turned: tempo follows", cfg.bpm == 170);
    tap_label(B_SAVE);
    check("SAVE: stored, SAVED shown", st_status == 1 && host_erases > 0u);
    host_turn(EN_K1, -2);
    host_frame();
    check("a knob turned after SAVE: SAVED gone", st_status == 0 && cfg.bpm == 168);
    power_cycle();
    check("power-on: the saved tempo back", cfg.bpm == 170);

    {
        int32_t other[4] = {111, 222, 333, 444};
        store_save(CFG_SLOT, 0x12345678u, other, sizeof other);
        power_cycle();
        check("another app's record: ignored (defaults)", cfg.bpm == 165 && cfg.root == 2);
    }
    memset(&wild, 0, sizeof wild);
    wild.bpm = 255;
    wild.root = 20;
    wild.scale = 99;
    wild.master = 250;
    wild.bass_oct = 9;
    wild.bass_cut = 200;
    store_save(CFG_SLOT, APP_ID, &wild, sizeof wild);
    power_cycle();
    check("stored values out of range: clamped", cfg.bpm == 200 && cfg.root == 0 &&
          cfg.scale == 0 && cfg.master == 110 && cfg.bass_oct == 1 && cfg.bass_cut == 80);

    flash_ok = 0;
    tap_label(B_SAVE);
    check("no flash: SAVE fails visibly", st_status == -1);
    flash_ok = 1;

    /* the active pattern: the default has a kick on step 0; toggling step 0 in SEQ edits it,
     * SAVE stores it, a power cycle brings it back */
    clean_input();
    lane = 0;
    ui_dirty = 1;
    host_frame();
    go_page(PG_SEQ);
    fm1_in.notes = 1u;                                  /* step 0 on, lane 0 (kick) */
    host_frame();
    fm1_in.notes = 0;
    host_frame();
    check("SEQ: step 0 of the kick lane toggled off its gate", (ed_pat.drum[0][0] & 1u) == 0u);
    {
        int32_t sbuf[2u * CTL];
        host_render(sbuf, CTL);                         /* the ISR takes the pattern copy */
    }
    check("SEQ: the ISR copy follows", (pat_cur.drum[0][0] & 1u) == 0u);
    tap_label(B_SAVE);
    power_cycle();
    check("pattern save: survives a power cycle", (ed_pat.drum[0][0] & 1u) == 0u);
}

/* ---------------------------------------------------------------- sound --- */
#define SECS 1u
static int32_t buf[2u * FS * SECS];

static double pitch(const int32_t *b, uint32_t frames)
{
    uint32_t i, n = 0, first = 0, last = 0;
    for (i = 1; i < frames; i++)
        if (b[2u * (i - 1u)] < 0 && b[2u * i] >= 0) {
            if (!n)
                first = i;
            last = i;
            n++;
        }
    return n > 1u ? (double)(n - 1u) * FS / (double)(last - first) : 0.0;
}

static int32_t peak(const int32_t *b, uint32_t frames)
{
    int32_t m = 0;
    uint32_t i;
    for (i = 0; i < 2u * frames; i++)
        m = abs(b[i]) > m ? abs(b[i]) : m;
    return m;
}

static int32_t max_step(const int32_t *b, uint32_t frames)
{
    int32_t m = 0;
    uint32_t i;
    for (i = 1; i < frames; i++)
        m = abs(b[2u * i] - b[2u * (i - 1u)]) > m ? abs(b[2u * i] - b[2u * (i - 1u)]) : m;
    return m;
}

static int same_lr(const int32_t *b, uint32_t frames)
{
    uint32_t i;
    for (i = 0; i < frames; i++)
        if (b[2u * i] != b[2u * i + 1u])
            return 0;
    return 1;
}

static void sound(void)
{
    double f;
    clean_input();
    /* a sane starting point (the persistence tests leave out-of-range test records behind) */
    cfg.master = 127;
    cfg.bass_cut = 90;
    cfg.bass_reso = 60;
    cfg.bass_drive = 45;
    cfg.bass_accent = 90;
    cfg.bass_slide = 70;
    cfg.lead_detune = 0;
    cfg.lead_lfo = 0;
    cfg.lead_cut = 100;
    cfg.lead_level = 127;
    cfg.drum_level = 120;
    cfg.kick_decay = 85;
    cfg.drum_hat = 100;
    cfg.drum_perc = 105;
    cfg.bass_oct = 1;
    cfg.lead_oct = 0;
    host_render(buf, 1024);
    check("silence with nothing held", peak(buf, 1024) == 0);

    fm1_in.notes = 1u << 8;                             /* key 8: bass note = 48 (C3, 130.81 Hz) */
    host_render(buf, FS / CTL * CTL);
    f = pitch(buf, FS / CTL * CTL);
    check("bass key: 130.8 Hz", fabs(f - 130.81) < 0.75);
    check("bass key: peak sane, attack no click", peak(buf, FS / CTL * CTL) < 32767 &&
          max_step(buf, 2048) < 1500);
    check("mono: left = right", same_lr(buf, FS / CTL * CTL));

    fm1_in.notes = (1u << 8) | (1u << 9);              /* two bass keys: the lowest plays */
    host_render(buf, 8192);
    check("two bass keys: the lowest plays", fabs(pitch(buf + 2u * 4096u, 4096) - 130.81) < 2.0);
    fm1_in.notes = 0;
    host_render(buf, 2048);
    check("bass released: silent within 2048 frames", peak(buf + 2u * 1024u, 1024) == 0);

    fm1_in.notes = 1u << 18;                            /* key 18: lead note = 60 (C4, 261.63 Hz) */
    host_render(buf, FS / CTL * CTL);
    f = pitch(buf, FS / CTL * CTL);
    check("lead key: 261.6 Hz", fabs(f - 261.63) < 1.5);
    fm1_in.notes = 0;
    host_render(buf, 2048);
    check("lead released: silent", peak(buf + 2u * 1024u, 1024) == 0);

    fm1_in.notes = 1u;                                  /* drum pad 0: a kick */
    host_render(buf, 2048);
    check("kick pad: sounds", peak(buf, 2048) > 3000);
    fm1_in.notes = 0;
    host_render(buf, 8192);
    check("kick pad released: ends in silence", peak(buf + 2u * 4096u, 4096) == 0);

    host_midi(0x90, 69, 100);                           /* A4 over USB-MIDI -> the lead */
    host_render(buf, FS / CTL * CTL);
    f = pitch(buf, FS / CTL * CTL);
    check("MIDI A4: 440 Hz", fabs(f - 440.0) < 1.5);
    host_midi(0x80, 60, 0);                             /* another note's off: A4 keeps going */
    host_render(buf, 1024);
    check("note-off of another note: A4 goes on", peak(buf + 2u * 512u, 512) > 200);
    host_midi(0x90, 69, 0);                             /* note-on velocity 0 = off */
    host_render(buf, 1024);
    check("note-on velocity 0: off", peak(buf + 2u * 512u, 512) == 0);

    midi_in_overflow = 1;                               /* a broken stream */
    host_render(buf, 1024);
    check("MIDI ring overflow: every note released", peak(buf + 2u * 512u, 512) == 0 && !midi_in_overflow);

    fm1_in.notes = 1u << 8;
    host_render(buf, 1024);
    fm1_in.notes = 0;
    host_render(buf, 256);
    app_audio_stop();                              /* before any new note: forgets everything */
    host_render(buf, 1024);
    check("app_audio_stop: silent at once", peak(buf, 1024) == 0);
    host_midi(0x90, 64, 100);                      /* and still works after a stop */
    host_render(buf, 1024);
    check("MIDI after stop: plays again", peak(buf + 2u * 512u, 512) > 200);
    host_midi(0x80, 64, 0);
    host_render(buf, 512);

    /* the transport: PLAY starts the pattern (the default pattern kicks), PLAY again stops it */
    clean_input();
    tap_label(B_PLAY);
    host_render(buf, 20000);                            /* > a few 16ths at 165 BPM */
    check("transport: playing sounds", peak(buf, 20000) > 3000);
    host_frame();
    check("transport: PLAY LED lit", host_led[panel.btn[B_PLAY]]);
    tap_label(B_PLAY);
    host_render(buf, 2048);
    host_frame();
    check("transport: stopped, PLAY LED out", !host_led[panel.btn[B_PLAY]]);
}

int main(void)
{
    if (system("mkdir -p build/shots"))
        return 1;
    screen();
    gen_test();
    persistence();
    sound();
    puts(failed ? "APP TEST FAILED" : "app test passed");
    return failed ? 1 : 0;
}
