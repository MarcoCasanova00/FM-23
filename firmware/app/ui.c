/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M-Vave-OP1 contributors
 * Copyright (C) 2026 MarcoCasanova00
 * UI patterns borrowed from zynthian-ui (GPL-3.0): layer/engine screens,
 * dynamic knob roles (SELECT moves the cursor, ALGO edits, KNOB 1..4 edit the
 * visible rows), and a consistent list page model. The implementation here is
 * original for the FM-1's 240x240 LCD.
 *
 * FM-23 UI design (from zero, around the firmware's functions):
 *  - every parameter page is the same list: 4 rows (6 on the SOUND tabs),
 *    SELECT = cursor, ALGO/edit knobs = values, names not numbers;
 *  - SOUND (EDIT) is instrument-first: BASS / LEAD / DRUMS / LAYER tabs;
 *  - MOD holds the envelope + LFO of the voice selected in SOUND;
 *  - HOME is the performance view: transport, keyboard instrument (PERF),
 *    tempo/swing/master, and the live drum grid + bass/lead strip;
 *  - SEQ is the pattern/chain view; GEN the generator; FX sends; GLO globals.
 */

enum { PG_HOME, PG_SEQ, PG_ARP, PG_EDIT, PG_FX, PG_GLO, PG_GEN, PG_MOD, NPG };
static const char *const PG_NAME[NPG] = {"HOME", "SEQ", "ARP", "SOUND", "FX", "GLO", "GEN",
                                         "MOD"};
static const char *const SUB_NAME[4] = {"BASS", "LEAD", "DRUMS", "LAYER"};
static const char *const DRUM_NAME[8] = {"KICK", "SNARE", "HAT", "OPEN", "MET", "STR", "NOISE", "ZAP"};
static const char *const LANE_NAME[6] = {"KICK", "SNAR", "HAT", "PERC", "BASS", "LEAD"};
static const char *const STYLE_NAME[3] = {"MENTAL", "TRIBE", "HARDTEKK"};
static const char *const BASS_NAMES[5] = {"CLASSIC", "B01 OFFBEAT", "B02 RUBBER FM", "B03 TRIBE SAW", "B04 ACID"};
static const char *const LEAD_NAMES[8] = {"CLASSIC", "L01 HOLLOW", "L02 SNAKE", "L05 GROWL", "L07 PING",
                                          "L03 NASAL", "L04 SYNC", "L06 SCREAM"};
static const char *const KIT_NAMES[4] = {"CLASSIC", "TRIBE", "MENTAL", "HARDTEKK"};
static const char *const MACRO_NAMES[4] = {"OFF", "RUBBER", "METAL", "SCREAM"};
static const char *const PERF_NAMES[3] = {"DRUMS", "BASS", "LEAD"};
static const char *const VOX_NAMES[3] = {"RETRIG", "LEGATO", "UNISON"};
static const char *const ARCH_NAMES[3] = {"SINGLE", "DUAL", "STACK"};

static uint8_t page = PG_HOME, sub = 0, sel = 0, lane = 0;
static uint8_t mod_view;                 /* PG_MOD: 0 envelope, 1 LFO */
static uint8_t gen_done;
static uint8_t seq_part, seq_step_idx, ui_auto_val = 64;
static uint8_t gen_pulse;            /* frames left in the GENERATED flash (trigger feedback) */
#define GEN_PULSE_FRAMES 10u
static uint32_t last_buttons, ui_dirty = 1;
static uint32_t last_notes_ui, last_sig = 0xFFFFFFFFu;

typedef struct {
    const char *name;
    uint8_t *v;
    uint16_t lo, hi;
    uint16_t *w;          /* optional 16-bit target (e.g. the seed) */
    const char *const *names;  /* display label per index (v - lo), or 0 for a number */
} row_t;

static void ui_env_draw(void);
static void ui_lfo_draw(void);

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
    rows[PG_HOME][3] = (row_t){"INSTRUMENT", &cfg.perf, 0, 2, 0, PERF_NAMES};
    rows[PG_SEQ][0] = (row_t){"LANE", &lane, 0, 5};
    rows[PG_SEQ][1] = (row_t){"TEMPO", &cfg.bpm, 130, 200};
    rows[PG_SEQ][2] = (row_t){"AUTO", &ed_pat.at_on, 0, 2};
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
    static row_t e[6];
    if (sub == 0) {                       /* BASS */
        e[0] = (row_t){"ENGINE", &cfg.bass_algo, 0, 4, 0, BASS_NAMES};
        e[1] = (row_t){"CUTOFF", &cfg.bass_cut, 0, 127};
        e[2] = (row_t){"DRIVE", &cfg.bass_drive, 0, 127};
        e[3] = (row_t){"ACCENT", &cfg.bass_accent, 0, 127};
        e[4] = (row_t){"PORTA", &cfg.porta[0], 0, 127};
        e[5] = (row_t){"ARCH", &cfg.arch[0], 0, 2, 0, ARCH_NAMES};
    } else if (sub == 1) {                /* LEAD */
        e[0] = (row_t){"ENGINE", &cfg.lead_algo, 0, 7, 0, LEAD_NAMES};
        e[1] = (row_t){"CUTOFF", &cfg.lead_cut, 0, 127};
        e[2] = (row_t){"LEVEL", &cfg.lead_level, 0, 127};
        e[3] = (row_t){"VOICE", &cfg.voice_mode[1], 0, 2, 0, VOX_NAMES};
        e[4] = (row_t){"PORTA", &cfg.porta[1], 0, 127};
        e[5] = (row_t){"ARCH", &cfg.arch[1], 0, 2, 0, ARCH_NAMES};
    } else if (sub == 2) {                /* DRUMS */
        e[0] = (row_t){"KIT", &cfg.kit, 0, 3, 0, KIT_NAMES};
        e[1] = (row_t){"MACRO", &cfg.macro, 0, 3, 0, MACRO_NAMES};
        e[2] = (row_t){"LVL", &cfg.drum_level, 0, 127};
        e[3] = (row_t){"DECAY", &cfg.kick_decay, 0, 127};
        e[4] = (row_t){"-", 0, 0, 0};
        e[5] = (row_t){"-", 0, 0, 0};
    } else {                              /* LAYER: feel, not instruments */
        e[0] = (row_t){"ANALOG B", &cfg.analog[0], 0, 127};
        e[1] = (row_t){"ANALOG L", &cfg.analog[1], 0, 127};
        e[2] = (row_t){"-", 0, 0, 0};
        e[3] = (row_t){"-", 0, 0, 0};
        e[4] = (row_t){"-", 0, 0, 0};
        e[5] = (row_t){"-", 0, 0, 0};
    }
    return e;
}

static void ui_gen_hook(uint8_t row)
{
    if (row <= 2u) {                        /* seed / scale / style: regenerate the pattern */
        gen_pattern();
        seq_request_pattern_copy();
        gen_done = 1;
        gen_pulse = GEN_PULSE_FRAMES;
    } else if (row == 3u) {                 /* mode: on = a song from the chain */
        if (cfg.song_on)
            gen_song();
        seq_request_pattern_copy();
        gen_done = 1;
        gen_pulse = GEN_PULSE_FRAMES;
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
    /* breadcrumb: page context (instrument / voice) */
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
    else if (loop_state() == 2u)
        b[i++] = 'L';
    else if (loop_state() == 1u)
        b[i++] = 'l';
    b[i++] = ' ';
    {
        const char *perf = PERF_NAMES[cfg.perf];
        uint32_t j;
        for (j = 0; perf[j] && i + 1u < sizeof b; j++)
            b[i++] = perf[j];
    }
    b[i++] = ' ';
    b[i++] = 'P';
    b[i++] = '0' + cfg.pat / 10u;
    b[i++] = '0' + cfg.pat % 10u;
    b[i] = 0;
    ui_text(28, b, T_MID);
    if (page == PG_EDIT || page == PG_MOD) {
        /* SOUND/MOD: show the voice + engine */
        uint32_t n = 0;
        const char *sn = SUB_NAME[sub], *nm = "?";
        b[n++] = 'E';
        b[n++] = 'D';
        b[n++] = 'I';
        b[n++] = 'T';
        b[n++] = ' ';
        for (uint32_t j = 0; sn[j] && n + 1u < sizeof b; j++)
            b[n++] = sn[j];
        b[n++] = ' ';
        b[n++] = 0xB7;
        b[n++] = ' ';
        if (sub == 0u)
            nm = BASS_NAMES[cfg.bass_algo];
        else if (sub == 1u)
            nm = LEAD_NAMES[cfg.lead_algo];
        else if (sub == 2u)
            nm = KIT_NAMES[cfg.kit];
        else
            nm = PERF_NAMES[cfg.perf];
        for (uint32_t j = 0; nm[j] && n + 1u < sizeof b; j++)
            b[n++] = nm[j];
        if (sub == 1u) {
            b[n++] = ' ';
            b[n++] = 0xB7;
            b[n++] = ' ';
            nm = VOX_NAMES[cfg.voice_mode[1]];
            for (uint32_t j = 0; nm[j] && n + 1u < sizeof b; j++)
                b[n++] = nm[j];
        }
        b[n] = 0;
        ui_text(44, b, T_THEME);
    }
}

static void ui_page_list(row_t *r)
{
    uint32_t i, nrows = 4;
    char b[40];
    if (page == PG_EDIT)
        nrows = 6;                              /* SOUND tabs have 6 rows */
    cv_begin(232, nrows * 18u + 4u, T_BG);
    for (i = 0; i < nrows; i++) {
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
            if (r[i].names) {
                uint32_t idx = (uint32_t)*r[i].v - r[i].lo;
                const char *nm = r[i].names[idx < (uint32_t)(r[i].hi - r[i].lo + 1u) ? idx : 0];
                uint32_t j;
                for (j = 0; nm[j] && n + 1u < sizeof b; j++)
                    b[n++] = nm[j];
                b[n] = 0;
            } else {
                ui_num(*r[i].v, b + n);
                while (b[n])
                    n++;
            }
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
    cv_blit(4, 76);
}

static void ui_page_home(void)
{
    uint32_t r, c, idx, k;

    /* drums: the 2x4 pad grid (keys 0..7) */
    cv_begin(232, 112, T_BG);
    for (r = 0; r < 2u; r++)
        for (c = 0; c < 4u; c++) {
            idx = r * 4u + c;
            uint32_t x = 4 + c * 58u, y = 4 + r * 56u;
            cv_rect(x, y, 52, 48, (fm1_in.notes >> idx) & 1u ? T_ACCENT : 0x18E3u);
            cv_text_in(x, y + 34, 52, &AF_S, DRUM_NAME[idx], T_TEXT, T_BG);
        }
    cv_blit(4, 68);

    /* melodic strip: bass (8..17) and lead (18..26), colored by the instrument */
    {
        uint16_t kb = cfg.perf == 1u ? 0x0320u : cfg.perf == 2u ? 0x780Fu : 0x18E3u;
        cv_begin(232, 30, T_BG);
        cv_text(4, 0, &AF_S, cfg.perf == 1u ? "BASS" : cfg.perf == 2u ? "LEAD" : "DRUMS", kb);
        for (k = 8; k < 18; k++) {
            uint32_t x = 4 + (k - 8) * 12;
            cv_rect(x, 14, 10, 16, (fm1_in.notes >> k) & 1u ? T_ACCENT : kb);
        }
        for (k = 18; k < 27; k++) {
            uint32_t x = 126 + (k - 18) * 12;
            cv_rect(x, 14, 10, 16, (fm1_in.notes >> k) & 1u ? T_ACCENT : kb);
        }
        cv_blit(4, 184);
    }

    /* transport hint */
    ui_text(220, seq_playing() ? "> PLAYING  REC=loop pedal" : ". STOPPED  PLAY=start  REC=loop pedal", T_MID);
}

static void ui_page_seq(void)
{
    uint32_t l, s;
    char b[80];
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
        /* melodic content summary */
        uint32_t n = 0;
        b[n++] = 'B'; b[n++] = 'A'; b[n++] = 'S'; b[n++] = 'S'; b[n++] = ':';
        for (s = 0; s < 16u; s++) if (ed_pat.bass[s]) { b[n++] = '0' + s / 10u; b[n++] = '0' + s % 10u; b[n++] = ' '; }
        b[n++] = ' '; b[n++] = 'L'; b[n++] = 'E'; b[n++] = 'A'; b[n++] = 'D'; b[n++] = ':';
        for (s = 0; s < 16u; s++) if (ed_pat.lead[s]) { b[n++] = '0' + s / 10u; b[n++] = '0' + s % 10u; b[n++] = ' '; }
        b[n] = 0;
        ui_text(168, b, T_THEME);
    }
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
    if (gen_pulse)
        ui_text(192, "GENERATED", T_ACCENT);
    else if (cfg.song_on && song_len)
        ui_text(192, "SONG CHAIN ON: A A' B A' B' A B' A'", T_THEME);
    else
        ui_text(192, gen_done ? "PATTERN GENERATED" : "TWIST SEED/SCALE/STYLE", gen_done ? T_THEME : T_MID);
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
    ui_page_env_lfo("ENV", PG_MOD);
    ui_env_draw();
}

static void ui_page_lfo(void)
{
    ui_page_env_lfo("LFO", PG_MOD);
    ui_lfo_draw();
}

/* the envelope curve on ENV: simulated with the same attack/decay/release math as the
 * engine (linear attack, D60 exponential decay, linear release), filled under the curve */
static void ui_env_draw(void)
{
    int32_t W = 224, H = 40, x0 = 4, y0 = 132;
    int32_t atk = bank_ms(cfg.env_a[sub]);
    int32_t d60 = bank_ms(cfg.env_d[sub]);
    int32_t rel = bank_ms(cfg.env_r[sub]);
    int32_t lvl = 256, a, r, dec, gateOff, totS;
    int32_t g = 0, s = 0, x, attacking = 1, on = 1;
    a = FS * atk / 1000; if (a < 1) a = 1;
    r = FS * rel / 1000; if (r < 1) r = 1;
    dec = dsp_env_dec_step_q16(d60);
    gateOff = a + FS * d60 / 1000;
    totS = gateOff + r;
    cv_begin(W, H, T_BG);
    cv_rect(0, H - 1, W, 1, T_MID);
    for (x = 0; x < W; x++) {
        int32_t target = (int32_t)(((int64_t)(x + 1) * totS) / W) -
                         (int32_t)(((int64_t)x * totS) / W);
        int32_t i;
        if (target < 1) target = 1;
        for (i = 0; i < target; i++) {
            if (on) {
                if (attacking) {
                    g += lvl / a;
                    if (g >= lvl) { g = lvl; attacking = 0; }
                } else {
                    g = (int32_t)(((int64_t)g * dec) >> 16);
                }
                if (s >= gateOff) { on = 0; attacking = 0; }
            } else {
                g -= lvl / r;
                if (g < 0) g = 0;
            }
            s++;
        }
        {
            int32_t yy = H - 2 - (int32_t)(((int64_t)g * (H - 4)) / lvl);
            if (yy < 0) yy = 0;
            if (yy >= H) yy = H - 1;
            cv_rect(x, yy, 1, H - yy, T_THEME);
        }
    }
    cv_blit(x0, y0);
}

/* the LFO shape on LFO: the selected wave drawn for two cycles, amplitude from DEPTH */
static void ui_lfo_draw(void)
{
    int32_t W = 224, H = 40, x0 = 4, y0 = 132;
    uint8_t sh = cfg.lfo_shape[sub];
    int32_t amp = 1 + (int32_t)cfg.lfo_depth[sub] * (H / 2 - 3) / 127;
    int32_t x;
    cv_begin(W, H, T_BG);
    cv_rect(0, H / 2, W, 1, T_MID);
    for (x = 0; x < W; x++) {
        uint32_t ph = (uint32_t)(((uint64_t)x * 0x80000000ull) / W);  /* two cycles */
        int32_t v, yy;
        if (sh == 1u) {
            int32_t t = (int32_t)(ph >> 16) - 32768;
            v = (t < 0 ? -t : t) * 2 - 32768;
        } else if (sh == 2u) {
            v = (ph & 0x80000000u) ? 32767 : -32767;
        } else if (sh == 3u) {
            uint32_t st = (uint32_t)x * 8u / W, seed = st * 2654435761u;
            v = (int32_t)((seed >> 16) & 0xFFFFu) - 32768;
        } else {
            v = dsp_sin(ph);
        }
        yy = H / 2 - (int32_t)(((int64_t)v * amp) / 32767);
        if (yy < 0) yy = 0;
        if (yy >= H) yy = H - 1;
        cv_rect(x, yy, 1, 3, T_THEME);
    }
    cv_blit(x0, y0);
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
    else if (page == PG_EDIT)
        ui_page_list(ui_edit_rows());
    else if (page == PG_FX)
        ui_page_fx();
    else if (page == PG_GEN)
        ui_page_gen();
    else if (page == PG_MOD && !mod_view)
        ui_page_env();
    else if (page == PG_MOD)
        ui_page_lfo();
    else
        ui_page_glo();
}

static uint32_t ui_sig(void)
{
    uint32_t h = 2166136261u, i;
    uint32_t v[13] = {page, sub, sel, lane, seq_playing(), seq_step_now(), seq_rec(),
                      fm1_in.notes, fm1_in.buttons, cfg.bpm, cfg.pat, (uint32_t)st_status,
                      cfg.perf};
    for (i = 0; i < 13u; i++)
        h = (h ^ v[i]) * 16777619u;
    return h;
}

/* ------------------------------------------------------------ input ---- */
static void ui_leds(void)
{
    uint32_t b, k, keys = fm1_in.notes;
    for (b = 0; b < 14u; b++)
        fm1_led_key(panel.btn[b], (int)(((fm1_in.buttons >> b) & 1u) ||
                                        (b == B_PLAY && seq_playing()) || (b == B_REC && (seq_rec() || loop_state()))));
    for (k = 0; k < 27u; k++) {
        int on = (int)((keys >> k) & 1u);
        if (page == PG_SEQ && k < 16u) {
            if (lane < 4u)
                on |= (ed_pat.drum[lane][k] & 1u);
            else if (lane == 4u)
                on |= (ed_pat.bass[k] != 0);
            else if (lane == 5u)
                on |= (ed_pat.lead[k] != 0);
        }
        if (seq_playing() && k == seq_step_now())
            on = 1;
        fm1_led_key(14u + k, on);
    }
    if (gen_pulse)                    /* GENERATED flash: the step LEDs blink once */
        for (k = 0; k < 16u; k++)
            fm1_led_key(14u + k, 1);
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
        /* MOD rows are filled every frame so a first visit is never null */
        if (!mod_view) {
            rows[PG_MOD][0] = (row_t){"ATTACK", &cfg.env_a[sub], 0, 127};
            rows[PG_MOD][1] = (row_t){"RELEASE", &cfg.env_r[sub], 0, 127};
            rows[PG_MOD][2] = (row_t){"DECAY D60", &cfg.env_d[sub], 0, 127};
            rows[PG_MOD][3] = (row_t){"PITCH tauP", &cfg.env_p[sub], 0, 127};
        } else {
            rows[PG_MOD][0] = (row_t){"ON", &cfg.lfo_on[sub], 0, 1};
            rows[PG_MOD][1] = (row_t){"RATE", &cfg.lfo_rate[sub], 0, 127};
            rows[PG_MOD][2] = (row_t){"DEPTH", &cfg.lfo_depth[sub], 0, 127};
            rows[PG_MOD][3] = (row_t){"SHAPE", &cfg.lfo_shape[sub], 0, 3};
        }
    }
    if (page == PG_SEQ) {                 /* SEQ rows depend on the edit mode */
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
        sub = (uint8_t)((sub + 1u) % 4u);
        ui_dirty = 1;
    }
    if (pressed & panel_mask(B_ENV)) {
        page = PG_MOD;
        mod_view = 0;
        ui_dirty = 1;
    }
    if (pressed & panel_mask(B_LFO)) {
        page = PG_MOD;
        mod_view = 1;
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
    if (pressed & panel_mask(B_REC)) {
        if (page == PG_SEQ)
            seq_toggle_rec();               /* SEQ: step-edit REC */
        else
            loop_toggle();                  /* everywhere else: the loop pedal */
        ui_dirty = 1;
    }
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
            uint32_t nrows = (page == PG_EDIT) ? 6u : 4u;
            sel = (uint8_t)((sel + (uint32_t)s) % nrows);
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
    if (page == PG_GEN) {
        /* any pad press is the GENERATE trigger: make a fresh cell (or song) now */
        uint32_t keys = fm1_in.notes, edges = keys & ~last_notes_ui;
        if (edges) {
            if (cfg.song_on)
                gen_song();
            else
                gen_pattern();
            seq_request_pattern_copy();
            gen_done = 1;
            gen_pulse = GEN_PULSE_FRAMES;
            ui_dirty = 1;
            last_notes_ui = keys;
        }
    }

    if (gen_pulse)
        gen_pulse--;

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
