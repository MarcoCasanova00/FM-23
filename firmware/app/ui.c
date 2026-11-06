/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00 */
/* ui.c - FM-23's pages and input. Pages: HOME (performance pads + transport), SEQ (the pattern
 * matrix), ARP (settings, sound later), EDIT (per-engine parameters, sub-tabs), FX (sends), GLO
 * (root, scale, MIDI clock). Every control has a visible effect: knobs edit the values on screen,
 * SELECT/ALGO move a cursor and edit its row, PRESET cycles patterns, the keys play pads/notes
 * (HOME) or toggle steps (SEQ), PLAY/REC toggle the transport, OCT shifts the octaves. Drawing is
 * lazy (a signature per page; a full redraw on app_redraw, a page change or any parameter edit). */

enum { PG_HOME, PG_SEQ, PG_ARP, PG_EDIT, PG_FX, PG_GLO, PG_GEN, PG_ENV, PG_LFO, NPG };
static const char *const PG_NAME[NPG] = {"HOME", "SEQ", "ARP", "EDIT", "FX", "GLO", "GEN",
                                         "ENV", "LFO"};
static const char *const SUB_NAME[3] = {"BASS", "LEAD", "DRUMS"};
static const char *const DRUM_NAME[8] = {"KICK", "SNARE", "HAT", "OPEN", "MET", "STR", "NOISE", "ZAP"};
static const char *const LANE_NAME[6] = {"KICK", "SNAR", "HAT", "PERC", "BASS", "LEAD"};
static const char *const STYLE_NAME[3] = {"MENTAL", "TRIBE", "HARDTEKK"};
static const char *const BASS_NAMES[5] = {"CLASSIC", "B01 OFFBEAT", "B02 RUBBER FM", "B03 TRIBE SAW", "B04 ACID"};
static const char *const LEAD_NAMES[8] = {"CLASSIC", "L01 HOLLOW", "L02 SNAKE", "L05 GROWL", "L07 PING",
                                          "L03 NASAL", "L04 SYNC", "L06 SCREAM"};
static const char *const KIT_NAMES[4] = {"CLASSIC", "TRIBE", "MENTAL", "HARDTEKK"};
static const char *const MACRO_NAMES[4] = {"OFF", "RUBBER", "METAL", "SCREAM"};

static uint8_t page = PG_HOME, sub = 0, sel = 0, lane = 0;
static uint8_t gen_done;
static uint8_t seq_part, seq_step_idx, ui_auto_val = 64;
static uint32_t last_buttons, ui_dirty = 1;
static uint32_t last_notes_ui, last_sig = 0xFFFFFFFFu;

typedef struct {
    const char *name;
    uint8_t *v;
    uint16_t lo, hi;
    uint16_t *w;          /* optional 16-bit target (e.g. the seed) */
} row_t;

static row_t rows[NPG][4];

static void ui_num(int32_t v, char *b)
{
    char t[12];
    uint32_t n = 0, u = v < 0 ? (uint32_t)-v : (uint32_t)v;
    if (v < 0)
        *b++ = '-';
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < sizeof t);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void ui_rows_fill(void)
{
    rows[PG_HOME][0] = (row_t){"TEMPO", &cfg.bpm, 130, 200};
    rows[PG_HOME][1] = (row_t){"SWING", &cfg.swing, 0, 100};
    rows[PG_HOME][2] = (row_t){"MASTER", &cfg.master, 0, 127};
    rows[PG_HOME][3] = (row_t){"PATTERN", &cfg.pat, 0, NPAT - 1u};
    rows[PG_SEQ][0] = (row_t){"LANE", &lane, 0, 5};
    rows[PG_SEQ][1] = (row_t){"TEMPO", &cfg.bpm, 130, 200};
    rows[PG_SEQ][2] = (row_t){"SWING", &cfg.swing, 0, 100};
    rows[PG_SEQ][3] = (row_t){"PATTERN", &cfg.pat, 0, NPAT - 1u};
    rows[PG_ARP][0] = (row_t){"ON", &cfg.arp_on, 0, 1};
    rows[PG_ARP][1] = (row_t){"STYLE", &cfg.arp_style, 0, 7};
    rows[PG_ARP][2] = (row_t){"HOLD", &cfg.arp_hold, 0, 1};
    rows[PG_ARP][3] = (row_t){"OCT", &cfg.arp_oct, 0, 4};
    rows[PG_FX][0] = (row_t){"DELAY", &cfg.fx_delay, 0, 127};
    rows[PG_FX][1] = (row_t){"REVERB", &cfg.fx_reverb, 0, 127};
    rows[PG_FX][2] = (row_t){"DRIVE", &cfg.fx_drive, 0, 127};
    rows[PG_FX][3] = (row_t){"TYPE", &cfg.fx_type, 0, 1};
    rows[PG_GLO][0] = (row_t){"ROOT", &cfg.root, 0, 11};
    rows[PG_GLO][1] = (row_t){"SCALE", &cfg.scale, 0, NSCALES - 1u};
    rows[PG_GLO][2] = (row_t){"MIDI CLK", &cfg.midi_clk, 0, 1};
    rows[PG_GLO][3] = (row_t){"-", 0, 0, 0};
    rows[PG_GEN][0] = (row_t){"SEED", 0, 0, 9999, &cfg.seed};
    rows[PG_GEN][1] = (row_t){"SCALE", &cfg.scale, 0, NSCALES - 1u};
    rows[PG_GEN][2] = (row_t){"STYLE", &cfg.style, 0, 2};
    rows[PG_GEN][3] = (row_t){"MODE", &cfg.song_on, 0, 1};
}

static row_t *ui_edit_rows(void)
{
    static row_t e[4];
    if (sub == 0) {
        e[0] = (row_t){"ALGO", &cfg.bass_algo, 0, 4};
        e[1] = (row_t){"CUTOFF", &cfg.bass_cut, 0, 127};
        e[2] = (row_t){"DRIVE", &cfg.bass_drive, 0, 127};
        e[3] = (row_t){"ACCENT", &cfg.bass_accent, 0, 127};
    } else if (sub == 1) {
        e[0] = (row_t){"ALGO", &cfg.lead_algo, 0, 7};
        e[1] = (row_t){"LFO", &cfg.lead_lfo, 0, 127};
        e[2] = (row_t){"CUTOFF", &cfg.lead_cut, 0, 127};
        e[3] = (row_t){"LEVEL", &cfg.lead_level, 0, 127};
    } else {
        e[0] = (row_t){"KICK DECAY", &cfg.kick_decay, 0, 127};
        e[1] = (row_t){"LVL", &cfg.drum_level, 0, 127};
        e[2] = (row_t){"KIT", &cfg.kit, 0, 3};
        e[3] = (row_t){"MACRO", &cfg.macro, 0, 3};
    }
    return e;
}

static void ui_gen_hook(uint8_t row)
{
    if (row <= 2u) {                        /* root / scale / style: regenerate the pattern */
        gen_pattern();
        seq_request_pattern_copy();
        gen_done = 1;
    } else if (row == 3u) {                 /* mode: on = a song from the chain */
        if (cfg.song_on)
            gen_song();
        seq_request_pattern_copy();
        gen_done = 1;
    }
}

static void ui_row_edit(row_t *r, int32_t s)
{
    int32_t nv;
    if (r->w) {
        nv = (int32_t)*r->w + s;
        if (nv < (int32_t)r->lo)
            nv = r->lo;
        if (nv > (int32_t)r->hi)
            nv = r->hi;
        *r->w = (uint16_t)nv;
        st_status = 0;
        ui_dirty = 1;
        if (page == PG_GEN)
            ui_gen_hook(r - rows[PG_GEN]);
        return;
    }
    if (!r->v)
        return;
    nv = (int32_t)*r->v + s;
    if (nv < (int32_t)r->lo)
        nv = r->lo;
    if (nv > (int32_t)r->hi)
        nv = r->hi;
    *r->v = (uint8_t)nv;
    st_status = 0;
    ui_dirty = 1;
    if (page == PG_GEN)                        /* the GEN rows regenerate live */
        ui_gen_hook(r - rows[PG_GEN]);
}

/* ------------------------------------------------------------ drawing ---- */
static void ui_text(uint32_t y, const char *s, uint32_t c)
{
    draw_text_line(0, y, 240, &AF_S, s, c, T_BG, 1);
}

static void ui_header(void)
{
    char b[40];
    uint32_t i = 0;
    draw_text_line(0, 4, 240, &AF_L, PG_NAME[page], T_TEXT, T_BG, 1);
    b[i++] = 'B';
    b[i++] = 'P';
    b[i++] = 'M';
    b[i++] = ' ';
    ui_num(cfg.bpm, b + i);
    while (b[i])
        i++;
    b[i++] = ' ';
    if (seq_playing())
        b[i++] = '>';
    else
        b[i++] = '.';
    if (seq_rec())
        b[i++] = 'R';
    b[i++] = ' ';
    b[i++] = 'P';
    b[i++] = '0' + cfg.pat / 10u;
    b[i++] = '0' + cfg.pat % 10u;
    b[i] = 0;
    ui_text(44, b, T_MID);
    ui_text(60, st_status > 0 ? "SAVED" : st_status < 0 ? "FAILED" : usb.config ? "USB" : "", T_MID);
}

static void ui_page_home(void)
{
    uint32_t r, c;
    cv_begin(232, 112, T_BG);
    for (r = 0; r < 2u; r++)
        for (c = 0; c < 4u; c++) {
            uint32_t idx = r * 4u + c, x = 4 + c * 58u, y = 4 + r * 56u;
            cv_rect(x, y, 52, 48, (fm1_in.notes >> idx) & 1u ? T_ACCENT : T_RAISE);
            if (idx < 8u)
                cv_text_in(x, y + 34, 52, &AF_S, DRUM_NAME[idx], T_TEXT, T_BG);
        }
    cv_blit(4, 68);
    ui_text(188, "OCT BASS/LEAD 1/1", T_MID);
}

static void ui_page_seq(void)
{
    uint32_t l, s;
    char b[40];
    cv_begin(232, 96, T_BG);
    for (l = 0; l < 6u; l++) {
        uint32_t y = 4 + l * 16u;
        cv_text(4, y, &AF_S, LANE_NAME[l], l == lane ? T_ACCENT : T_MID);
        for (s = 0; s < 16u; s++) {
            uint32_t x = 44 + s * 11u, on;
            on = (l < 4u) ? (ed_pat.drum[l][s] & 1u) : (l == 4u ? ed_pat.bass[s] : ed_pat.lead[s]);
            cv_rect(x, y, 9, 9,
                    seq_playing() && s == seq_step_now() ? T_ACCENT :
                    on ? T_THEME : T_RAISE);
        }
    }
    cv_blit(4, 68);
    if (seq_part) {
        uint32_t n = 0, i;
        b[n++] = 'C';
        b[n++] = ':';
        b[n++] = ' ';
        for (i = 0; i < song_len && i < SONG_NPAT && n + 2u < sizeof b; i++) {
            b[n++] = (char)('0' + song_order[i] % 10u);
            b[n++] = ' ';
        }
        b[n] = 0;
        ui_text(168, b, T_THEME);
    } else {
        ui_num(lane, b);
        ui_text(168, b, T_MID);
    }
}

static void ui_page_list(row_t *r)
{
    uint32_t i;
    char b[40];
    cv_begin(232, 4u * 18u + 4u, T_BG);
    for (i = 0; i < 4u; i++) {
        uint32_t y = 2 + i * 18u, n = 0, j;
        const char *nm = r[i].name;
        for (j = 0; nm[j] && n + 1u < sizeof b; j++)
            b[n++] = nm[j];
        b[n++] = ' ';
        if (r[i].w) {
            ui_num(*r[i].w, b + n);
            while (b[n])
                n++;
        } else if (r[i].v) {
            ui_num(*r[i].v, b + n);
            while (b[n])
                n++;
        } else {
            b[n++] = '-';
            b[n] = 0;
        }
        if (i == sel) {               /* the selected row inverts: a solid bar, ink text */
            cv_rect(2, y, 228, 16, T_SEL);
            cv_text(8, y + 1, &AF_S, b, T_INK);
        } else {
            cv_text(8, y + 1, &AF_S, b, T_TEXT);
        }
    }
    cv_blit(4, 92);
}

static void ui_page_arp(void)
{
    ui_text(76, "ARP  SEQUENCED LEAD", T_MID);
    ui_page_list(rows[PG_ARP]);
}

static void ui_page_fx(void)
{
    ui_text(76, "FX  SEND LEVELS", T_MID);
    ui_page_list(rows[PG_FX]);
}

static void ui_page_gen(void)
{
    char b[32];
    uint32_t n = 0, j;
    const char *sn = STYLE_NAME[cfg.style];
    ui_page_list(rows[PG_GEN]);
    b[n++] = 'S';
    b[n++] = 'T';
    b[n++] = 'Y';
    b[n++] = 'L';
    b[n++] = 'E';
    b[n++] = ' ';
    for (j = 0; sn[j] && n + 1u < sizeof b; j++)
        b[n++] = sn[j];
    b[n] = 0;
    ui_text(176, b, T_MID);
    if (cfg.song_on && song_len)
        ui_text(192, "SONG CHAIN ON: A A' B A' B' A B' A'", T_THEME);
    else
        ui_text(192, gen_done ? "PATTERN GENERATED" : "TWIST ROOT/SCALE/STYLE", gen_done ? T_THEME : T_MID);
}

static void ui_page_env_lfo(const char *tag, uint32_t pg)
{
    char b[24];
    uint32_t n = 0, j;
    const char *sn = SUB_NAME[sub];
    for (j = 0; tag[j] && n + 1u < sizeof b; j++)
        b[n++] = tag[j];
    b[n++] = ' ';
    for (j = 0; sn[j] && n + 1u < sizeof b; j++)
        b[n++] = sn[j];
    b[n] = 0;
    ui_text(76, b, T_THEME);
    ui_page_list(rows[pg]);
}

static void ui_page_env(void)
{
    ui_page_env_lfo("ENV", PG_ENV);
}

static void ui_page_lfo(void)
{
    ui_page_env_lfo("LFO", PG_LFO);
}

static void ui_page_glo(void)
{
    char b[48];
    uint32_t n;
    ui_page_list(rows[PG_GLO]);
    n = 0;
    b[n++] = 'R';
    b[n++] = 'O';
    b[n++] = 'O';
    b[n++] = 'T';
    b[n++] = ' ';
    {
        const char *rn = ROOT_NAME[cfg.root];
        uint32_t j;
        for (j = 0; rn[j] && n + 1u < sizeof b; j++)
            b[n++] = rn[j];
    }
    b[n++] = ' ';
    b[n++] = 'S';
    b[n++] = 'C';
    b[n++] = 'A';
    b[n++] = 'L';
    b[n++] = 'E';
    b[n++] = ' ';
    {
        const char *sn = SCALE_NAME[cfg.scale];
        uint32_t j;
        for (j = 0; sn[j] && n + 1u < sizeof b; j++)
            b[n++] = sn[j];
    }
    b[n] = 0;
    ui_text(176, b, T_MID);
}

static void ui_draw(void)
{
    lcd_fill(0, 0, 240, 240, T_BG);
    ui_header();
    if (page == PG_HOME)
        ui_page_home();
    else if (page == PG_SEQ)
        ui_page_seq();
    else if (page == PG_ARP)
        ui_page_arp();
    else if (page == PG_EDIT) {
        char b[40];
        uint32_t n = 0, j;
        const char *sn = SUB_NAME[sub], *nm = "?";
        b[n++] = 'E';
        b[n++] = 'D';
        b[n++] = 'I';
        b[n++] = 'T';
        b[n++] = ' ';
        for (j = 0; sn[j] && n + 1u < sizeof b; j++)
            b[n++] = sn[j];
        b[n++] = ' ';
        b[n++] = 0xB7;                     /* middle dot */
        b[n++] = ' ';
        if (sub == 0u)
            nm = BASS_NAMES[cfg.bass_algo];
        else if (sub == 1u)
            nm = LEAD_NAMES[cfg.lead_algo];
        else
            nm = KIT_NAMES[cfg.kit];
        for (j = 0; nm[j] && n + 1u < sizeof b; j++)
            b[n++] = nm[j];
        if (sub == 2u) {
            b[n++] = ' ';
            b[n++] = 0xB7;
            b[n++] = ' ';
            nm = MACRO_NAMES[cfg.macro];
            for (j = 0; nm[j] && n + 1u < sizeof b; j++)
                b[n++] = nm[j];
        }
        b[n] = 0;
        ui_text(76, b, T_THEME);
        ui_page_list(ui_edit_rows());
    } else if (page == PG_FX)
        ui_page_fx();
    else if (page == PG_GEN)
        ui_page_gen();
    else if (page == PG_ENV)
        ui_page_env();
    else if (page == PG_LFO)
        ui_page_lfo();
    else
        ui_page_glo();
}

static uint32_t ui_sig(void)
{
    uint32_t h = 2166136261u, i;
    uint32_t v[12] = {page, sub, sel, lane, seq_playing(), seq_step_now(), seq_rec(),
                      fm1_in.notes, fm1_in.buttons, cfg.bpm, cfg.pat, (uint32_t)st_status};
    for (i = 0; i < 12u; i++)
        h = (h ^ v[i]) * 16777619u;
    return h;
}

/* ------------------------------------------------------------ input ---- */
static void ui_leds(void)
{
    uint32_t b, k, keys = fm1_in.notes;
    for (b = 0; b < 14u; b++)
        fm1_led_key(panel.btn[b], (int)(((fm1_in.buttons >> b) & 1u) ||
                                        (b == B_PLAY && seq_playing()) || (b == B_REC && seq_rec())));
    for (k = 0; k < 27u; k++) {
        int on = (int)((keys >> k) & 1u);
        if (page == PG_SEQ && k < 16u && lane < 4u && (ed_pat.drum[lane][k] & 1u))
            on = 1;
        if (seq_playing() && k == seq_step_now())
            on = 1;
        fm1_led_key(14u + k, on);
    }
}

static void seq_cycle_vel(uint8_t l, uint8_t s)
{
    uint8_t v = ed_pat.vel[l][s];
    ed_pat.vel[l][s] = v < 104u ? 110u : v < 122u ? 127u : 90u;
}

static void ui_frame(void)
{
    uint32_t pressed = fm1_in.buttons & ~last_buttons, i;
    last_buttons = fm1_in.buttons;

    {
        /* ENV / LFO rows are filled every frame so a first visit is never null */
        rows[PG_ENV][0] = (row_t){"ATTACK", &cfg.env_a[sub], 0, 127};
        rows[PG_ENV][1] = (row_t){"RELEASE", &cfg.env_r[sub], 0, 127};
        rows[PG_ENV][2] = (row_t){"DECAY D60", &cfg.env_d[sub], 0, 127};
        rows[PG_ENV][3] = (row_t){"PITCH tauP", &cfg.env_p[sub], 0, 127};
        rows[PG_LFO][0] = (row_t){"ON", &cfg.lfo_on[sub], 0, 1};
        rows[PG_LFO][1] = (row_t){"RATE", &cfg.lfo_rate[sub], 0, 127};
        rows[PG_LFO][2] = (row_t){"DEPTH", &cfg.lfo_depth[sub], 0, 127};
        rows[PG_LFO][3] = (row_t){"SHAPE", &cfg.lfo_shape[sub], 0, 3};
    }
    if (page == PG_SEQ) {                 /* the SEQ rows depend on the edit mode */
        if (seq_part) {
            rows[PG_SEQ][0] = (row_t){"PART", &seq_part, 0, 1};
            rows[PG_SEQ][1] = (row_t){"SONG", &cfg.song_on, 0, 1};
            rows[PG_SEQ][2] = (row_t){"STEP", &seq_step_idx, 0,
                                       song_len ? (uint8_t)(song_len - 1u) : 0u};
            rows[PG_SEQ][3] = (row_t){"LENGTH", &song_len, 1, SONG_NPAT};
        } else {
            rows[PG_SEQ][0] = (row_t){"LANE", &lane, 0, 6};
            rows[PG_SEQ][1] = (row_t){"TEMPO", &cfg.bpm, 130, 200};
            rows[PG_SEQ][2] = (row_t){"AUTO", &ed_pat.at_on, 0, 2};
            rows[PG_SEQ][3] = (row_t){"PATTERN", &cfg.pat, 0, NPAT - 1u};
        }
    }

    if (pressed & panel_mask(B_HOME))
        page = PG_HOME, ui_dirty = 1;
    if (pressed & panel_mask(B_SEQ))
        page = PG_SEQ, ui_dirty = 1;
    if (pressed & panel_mask(B_ARP))
        page = PG_ARP, ui_dirty = 1;
    if (pressed & panel_mask(B_FX))
        page = PG_FX, ui_dirty = 1;
    if (pressed & panel_mask(B_GLO))
        page = PG_GLO, ui_dirty = 1;
    if (pressed & panel_mask(B_SCL))
        page = PG_GEN, ui_dirty = 1;
    if (pressed & panel_mask(B_EDIT)) {
        page = PG_EDIT;
        sub = (uint8_t)((sub + 1u) % 3u);
        ui_dirty = 1;
    }
    if (pressed & panel_mask(B_ENV)) {
        page = PG_ENV;
        ui_dirty = 1;
    }
    if (pressed & panel_mask(B_LFO)) {
        page = PG_LFO;
        ui_dirty = 1;
    }
    if (pressed & panel_mask(B_SAVE)) {
        if (page == PG_SEQ) {
            pat_save(), seq_request_pattern_copy();
            if (seq_part)
                song_save();
        } else if (page == PG_GEN) {
            cfg_save();
            if (cfg.song_on)
                song_save();
        } else {
            cfg_save();
        }
        ui_dirty = 1;
    }
    if (pressed & panel_mask(B_PLAY))
        seq_toggle_play();
    if (pressed & panel_mask(B_REC))
        seq_toggle_rec();
    if (pressed & panel_mask(B_OCTDN)) {
        if (cfg.bass_oct)
            cfg.bass_oct--;
        if (cfg.lead_oct)
            cfg.lead_oct--;
        ui_dirty = 1;
    }
    if (pressed & panel_mask(B_OCTUP)) {
        if (cfg.bass_oct < 4u)
            cfg.bass_oct++;
        if (cfg.lead_oct < 4u)
            cfg.lead_oct++;
        ui_dirty = 1;
    }

    if (page == PG_SEQ && !seq_part && sel == 2u) {
        int32_t s = panel_enc(EN_K1);       /* KNOB 1 = the automation value to write */
        if (s) {
            int32_t nv = (int32_t)ui_auto_val + s * 4;
            ui_auto_val = nv < 0 ? 0 : nv > 127 ? 127 : (uint8_t)nv;
            ui_dirty = 1;
        }
    }
    for (i = 0; i < 4u; i++) {
        int32_t s = panel_enc(EN_K1 + i);
        if (s) {
            row_t *r = (page == PG_EDIT) ? ui_edit_rows() : rows[page];
            if (page == PG_SEQ && !seq_part && sel == 2u && i == 0u)
                continue;                   /* K1 is captured for automation here, not LANE */
            ui_row_edit(&r[i], s);
        }
    }
    {
        int32_t s = panel_enc(EN_SELECT);
        if (s) {
            sel = (uint8_t)((sel + (uint32_t)s) & 3u);
            ui_dirty = 1;
        }
        s = panel_enc(EN_ALGO);
        if (s) {
            row_t *r = (page == PG_EDIT) ? ui_edit_rows() : rows[page];
            ui_row_edit(&r[sel], s);
        }
        s = panel_enc(EN_PRESET);
        if (s) {
            cfg.pat = (uint8_t)((cfg.pat + (s > 0 ? 1u : NPAT - 1u)) % NPAT);
            ui_dirty = 1;
        }
    }
    if (page == PG_SEQ && !seq_part && lane == 6u) {
        /* the LANE row tops out at 6: entering it latches the song-chain editor */
        lane = 0;
        seq_part = 1;
        ui_dirty = 1;
    }
    if (page == PG_SEQ && seq_part && seq_step_idx >= song_len) {
        seq_step_idx = song_len ? (uint8_t)(song_len - 1u) : 0u;
        ui_dirty = 1;
    }
    if (page == PG_SEQ && !seq_part && sel == 2u && ui_dirty) {
        seq_request_pattern_copy();         /* keep the ISR copy in step with the AUTO edits */
    }

    if (page == PG_SEQ) {
        uint32_t k, keys = fm1_in.notes, edges = keys & ~last_notes_ui;
        if (seq_part) {
            for (k = 0; k < SONG_NPAT; k++)
                if ((edges >> k) & 1u && !seq_rec()) {
                    song_order[seq_step_idx] = (uint8_t)k;
                    ui_dirty = 1;
                }
        } else {
            for (k = 0; k < 16u; k++)
                if ((edges >> k) & 1u) {
                    if (seq_rec()) {          /* REC + step = write automation (or cycle velocity) */
                        if (sel == 2u && ed_pat.at_on)
                            ed_pat.am[ed_pat.at_on - 1u][k] = ui_auto_val;
                        else
                            seq_cycle_vel(lane, (uint8_t)k);
                    } else if (lane < 4u) {
                        ed_pat.drum[lane][k] ^= 1u;
                    } else {
                        uint8_t *v = lane == 4u ? &ed_pat.bass[k] : &ed_pat.lead[k];
                        *v = *v ? 0u : 1u;
                    }
                    gen_done = 0;
                    seq_request_pattern_copy();
                    ui_dirty = 1;
                }
        }
        last_notes_ui = keys;
    }

    ui_leds();
    if (app_redraw || ui_dirty) {
        app_redraw = 0;
        ui_dirty = 0;
        ui_draw();
        return;
    }
    if (ui_sig() != last_sig) {
        last_sig = ui_sig();
        ui_draw();
    }
}
