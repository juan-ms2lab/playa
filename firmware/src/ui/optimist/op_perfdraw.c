/* SPDX-License-Identifier: GPL-3.0-only */
/* JTS22 PLAYA, a performance build for the M-VAVE FM-1: created by JFS with help from Claude (Anthropic), 2026.
 * Built on Optimist by w0ts and contributors (github.com/w0ts/optimist), GPL-3.0-only; this file is part of that work
 * and under the same licence (LICENSE). */
/* PERFORM's panel (op_perf.c), the performance look (the user, 2026-11-10: "high contrast but with colour, blue
 * background, white pills, reverse text, bolder fonts ... the mode and the note very visible"; after Golana's
 * cards): a blue screen, white pills with blue text, the selection yellow, Inter Tight bold (perf_aa.c). PERFORM:
 * layer A, the mode and the split note large, layer B, the drum kit and the song, the four knobs' cards. MIX: the
 * four levels as tall cards. The whole screen above the help band, no header. After op_draw.c (OP_FOOT_Y, hu). */
#define PF_BOX_Y 188                                    /* the four knobs' boxes: 52 rows, on the screen's foot */
#define PF_H 240u                                       /* (the whole screen: no help line, no load lines on these pages) */


static void pf_cellv(uint32_t r, uint32_t k, cell_t *c)
{
    memset(c, 0, sizeof *c);
    pfm_cell(r, k, c);
}
/* a sound's pill: the engine's tag at the left, the name in the rest */
static void pf_sound(int32_t y, uint32_t t, const char *name, int sel, int dim)
{
    uint16_t pc = sel ? PF_SEL : dim ? PF_TAG : PF_PILL, ec = trk_col(t);   /* (the engine's tag in its own colour: colors.c ENG_COL) */
    const char *eng = trk_eng_name(&trk[t]);
    int32_t tw = aa_w(&AF_PS, eng) + 14;
    aa_box(0, y, 240, 38, 19, pc, PF_BG);                              /* (38 rows: the user, 2026-11-10, "2 pixels taller") */
    aa_box(7, y + 8, tw, 22, 11, ec, pc);
    AA_MID(7, y + 8, tw, 22, PS, eng, ink_on(ec), ec);
    AA_MID(7 + tw, y, 240 - 14 - tw, 38, PM, name, PF_BG, pc);
}
static void pf_paint_perf(uint32_t r)
{
    static const char *const MODE[PERF_NMODE] = {"LAYER", "SPLIT", "BLEND", "SINGLE"};
    char a[14], b[14], d[14], nn[5];
    uint32_t k, m = perf.mode == PERF_BLEND && !perf.width ? PERF_HARD : perf.mode;
    uint16_t mc = r == PFR_MODE ? PF_SEL : PF_PILL, nc = r == PFR_NOTE ? PF_SEL : m == PERF_LAYER || m == PERF_SINGLE ? PF_TAG : PF_PILL;
    snd_name(0, a), snd_name(1, b), snd_name(TRK_DRUM, d);
    /* the column (the user, 2026-11-10): A 2 .. 40, the mode and the note a band of 40, B 80 .. 118, 18 rows, the drums
     * 136 .. 170, 18 rows, the knobs' boxes 188 to the foot */
    pf_sound(2, 0, a, r == PFR_A, 0);                 /* layer A */
    /* the mode and the split note: text on the screen, the pills' size, the selected one in the selection's colour (the
     * user, 2026-11-10: "just text (no pill) same size as text inside the pills"); LAYER, SINGLE: the note light, unused */
    AA_MID(0, 40, 120, 40, PM, MODE[m % PERF_NMODE], mc, PF_BG);
    note_name(nn, perf.split);
    AA_MID(120, 40, 120, 40, PM, nn, nc, PF_BG);
    pf_sound(80, 1, b, r == PFR_B, m == PERF_SINGLE);  /* layer B (SINGLE: shown, not playing: dim) */
    aa_box(0, 136, 240, 34, 17, r == PFR_DR ? PF_SEL : PF_PILL, PF_BG);   /* the drum kit, the whole width */
    AA_MID(0, 136, 240, 34, PM, d, PF_BG, r == PFR_DR ? PF_SEL : PF_PILL);
    for (k = 0; k < 4u; k++) {                                          /* KNOB 1..4: REV CHO DLY FILT, or the split's */
        cell_t c;
        int32_t x = (int32_t)k * 61;
        pf_cellv(r, k, &c);
        aa_box(x, PF_BOX_Y, 57, 240 - PF_BOX_Y, 10, PF_PILL, PF_BG);   /* (to the screen's last row: the user, "always on
                                                                         * the bottom pixel") */
        if (!c.label)
            continue;
        AA_MID(x, PF_BOX_Y + 4, 57, 16, PS, c.label, PF_BG, PF_PILL);
        if (aa_w(&AF_PM, c.val) <= 53)
            AA_MID(x, PF_BOX_Y + 20, 57, 28, PM, c.val, PF_BG, PF_PILL);
        else
            AA_MID(x, PF_BOX_Y + 20, 57, 28, PS, c.val, PF_BG, PF_PILL);   /* (a long value: the smaller face) */
    }
}
static void pf_paint_mix(void)
{
    static const char *const L[4] = {"A", "B", "DR", "COMP"};
    uint32_t k;
    for (k = 0; k < 4u; k++) {                                          /* a tall card a level: the name, the value, the bar */
        cell_t c;
        int32_t x = (int32_t)k * 61, top = 70, hh = (int32_t)PF_H - top - 8, f = 0;   /* (MIX: the cards to the foot) */
        uint16_t cc = PF_PILL;
        pf_cellv(0, k, &c);
        if (k == 3u) {                                                  /* COMP: the bar the compression (THRS down), grey when off */
            if (!song.g[G_CTHR])
                cc = PF_TAG;
            f = (int32_t)(-song.g[G_CTHR]) * hh / 30;
        } else if (c.label && c.gmax > c.gmin) {
            f = (int32_t)(c.gv - c.gmin) * hh / (c.gmax - c.gmin);
        }
        aa_box(x, 0, 57, (int32_t)PF_H, 12, cc, PF_BG);
        if (aa_w(&AF_PM, L[k]) <= 53)
            AA_MID(x, 4, 57, 32, PM, L[k], PF_BG, cc);
        else
            AA_MID(x, 4, 57, 32, PS, L[k], PF_BG, cc);
        AA_MID(x, 38, 57, 22, PS, c.label ? c.val : "-", PF_BG, cc);
        aa_box(x + 10, top, 37, hh, 8, cc == PF_TAG ? PF_PILL : PF_TAG, cc);   /* (off: an empty, light track) */
        if (f > 0)
            aa_box(x + 10, top + hh - f, 37, f, f < 16 ? f / 2 : 8, cc == PF_TAG ? PF_TAG : PF_BG, cc == PF_TAG ? PF_BG : PF_TAG);
    }
}
/* a page of cards: the title (the page, the track in its engine's colour, the bank), four cards in two rows */
static void pf_card(int32_t x, int32_t y, int32_t w, int32_t h, const cell_t *c)
{
    int32_t bx = x + 10, bw = w - 20, by = y + h - 14;
    aa_box(x, y, w, h, 12, PF_PILL, PF_BG);
    if (!c->label) {
        AA_MID(x, y, w, h, PM, "-", PF_TAG, PF_PILL);
        return;
    }
    aa_text(x + 10, y + 6, &AF_PS, c->label, PF_BG, PF_PILL);
    if (aa_w(&AF_PM, c->val) <= w - 12)
        AA_MID(x, y + 22, w, h - 40, PM, c->val, PF_BG, PF_PILL);
    else
        AA_MID(x, y + 22, w, h - 40, PS, c->val, PF_BG, PF_PILL);
    aa_box(bx, by, bw, 6, 3, PF_TAG, PF_PILL);                         /* the value's bar */
    if (c->gmax > c->gmin) {
        int32_t span = c->gmax - c->gmin, v = c->gv - c->gmin;
        if (c->gmin < 0 && c->gmax > 0) {                              /* bipolar: from the centre */
            int32_t z = (-c->gmin) * bw / span, e = v * bw / span;
            cv_rect(bx + (e < z ? e : z), by, e < z ? z - e : e - z + 1, 6, PF_BG);
        } else if (v > 0) {
            aa_box(bx, by, v * bw / span < 6 ? 6 : v * bw / span, 6, 3, PF_BG, PF_TAG);
        }
    }
}
/* the four controls as boxes along the foot, as PERFORM's (ENV, LFO) */
static void pf_boxes(uint32_t r)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        cell_t c;
        int32_t x = (int32_t)k * 61;
        pf_cellv(r, k, &c);
        aa_box(x, PF_BOX_Y, 57, 240 - PF_BOX_Y, 10, PF_PILL, PF_BG);
        if (!c.label)
            continue;
        AA_MID(x, PF_BOX_Y + 4, 57, 16, PS, c.label, PF_BG, PF_PILL);
        if (aa_w(&AF_PM, c.val) <= 53)
            AA_MID(x, PF_BOX_Y + 20, 57, 28, PM, c.val, PF_BG, PF_PILL);
        else
            AA_MID(x, PF_BOX_Y + 20, 57, 28, PS, c.val, PF_BG, PF_PILL);
    }
}
/* a curve y[0..n) (rows from the top of the box) drawn 3 px thick, filled under it light */
static void pf_curve(int32_t x0, int32_t y0, int32_t h, const uint8_t *y, uint32_t n, int fill)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = y[i], b = i ? y[i - 1] : y[i], lo = a < b ? a : b, hi = a < b ? b : a;
        if (fill)
            cv_rect(x0 + (int32_t)i, y0 + a, 1, h - a, PF_TAG);
        cv_rect(x0 + (int32_t)i, y0 + lo - 1, 1, hi - lo + 3, PF_PILL);   /* (1 wide: the next column's fill
                                                                         * would cover a 2nd) */
    }
}
static int32_t pf_norm(const cell_t *c, int32_t full)   /* the value over its range, 0 .. full */
{
    return c->label && c->gmax > c->gmin ? (int32_t)(c->gv - c->gmin) * full / (c->gmax - c->gmin) : 0;
}
#define PF_GY 44
#define PF_GH 134
static void pf_graph_env(void)
{
    cell_t c[4];
    uint8_t y[232];
    int32_t k, a, d, rl, s, w = 232, hold = 36, x, top = 8, bot = PF_GH - 4, sus;
    for (k = 0; k < 4; k++)
        pf_cellv(0, (uint32_t)k, &c[k]);
    a = 6 + pf_norm(&c[0], 56), d = 6 + pf_norm(&c[1], 56), s = pf_norm(&c[2], 1000), rl = 6 + pf_norm(&c[3], 70);
    k = a + d + hold + rl;
    if (k > w)
        a = a * w / k, d = d * w / k, hold = hold * w / k, rl = w - a - d - hold;
    sus = bot - s * (bot - top) / 1000;
    for (x = 0; x < w; x++) {
        int32_t v;
        if (x < a)
            v = bot - (bot - top) * x / a;
        else if (x < a + d)
            v = top + (sus - top) * (x - a) / d;
        else if (x < a + d + hold)
            v = sus;
        else if (x < a + d + hold + rl)
            v = sus + (bot - sus) * (x - a - d - hold) / rl;
        else
            v = bot;
        y[x] = (uint8_t)v;
    }
    pf_curve(4, PF_GY, PF_GH, y, (uint32_t)w, 1);
}
static int32_t pf_sin(uint32_t ph)                      /* ph: a cycle in 65536 -> -1000 .. 1000 */
{
    uint32_t q = (ph >> 14) & 3u, x = (ph & 16383u) >> 1;   /* (x: Q13 in the quarter) */
    int32_t v = (int32_t)perf_qsin(q & 1u ? 8192u - x : x) * 1000 / 32768;
    return q >= 2u ? -v : v;
}
static void pf_graph_lfo(void)
{
    static const uint8_t SH[8] = {70, 20, 95, 45, 5, 80, 35, 60};   /* (S&H, SLEW: a fixed row of steps) */
    cell_t rate;
    uint8_t y[232];
    uint32_t i, cyc, wave = (uint32_t)TSEL->p[P_LWAVE];
    int32_t mid = PF_GH / 2, amp = PF_GH / 2 - 8;
    page_cell(&PG_BANKS[1], 0, &rate);
    cyc = 1u + (uint32_t)pf_norm(&rate, 5);
    for (i = 0; i < 232u; i++) {
        uint32_t ph = i * cyc * 65536u / 232u, p16 = ph & 65535u, st = (ph >> 16) * 3u + (p16 >> 14) * 0u;
        int32_t v;
        switch (wave) {
        case 0: v = pf_sin(p16); break;
        case 1: v = p16 < 32768u ? (int32_t)p16 * 2000 / 32768 - 1000 : 1000 - (int32_t)(p16 - 32768u) * 2000 / 32768; break;
        case 2: v = 1000 - (int32_t)p16 * 2000 / 65536; break;
        case 3: v = p16 < 32768u ? 1000 : -1000; break;
        case 4: v = SH[(st + (p16 >> 15)) % 8u] * 20 - 1000; break;
        default: {
            int32_t a = SH[(st + (p16 >> 15)) % 8u] * 20 - 1000, b = SH[(st + (p16 >> 15) + 1u) % 8u] * 20 - 1000;
            v = a + (b - a) * (int32_t)(p16 & 32767u) / 32768;
            break;
        }
        }
        y[i] = (uint8_t)(mid - v * amp / 1000);
    }
    cv_rect(4, PF_GY + mid, 232, 1, PF_TAG);
    pf_curve(4, PF_GY, PF_GH, y, 232u, 0);
}
static void pf_paint_save(uint32_t r)
{
    uint32_t i, k, first = r < 3u ? 0u : r + 1u >= PROJ_SLOTS ? PROJ_SLOTS - (PROJ_SLOTS < 4u ? PROJ_SLOTS : 4u) : r - 2u;
    char b[16];
    for (i = 0; i < 4u && first + i < PROJ_SLOTS; i++) {   /* four at a time, the selected one kept in view */
        int used, sel;
        uint16_t pc;
        int32_t y = 44 + (int32_t)i * 40;
        k = first + i;
        used = project_slot_used(k), sel = k == r;
        pc = sel ? PF_SEL : used ? PF_PILL : PF_TAG;
        str_cpy(b, "SLOT ", sizeof b);
        if (k + 1u >= 10u)
            b[5] = (char)('0' + (k + 1u) / 10u), b[6] = (char)('0' + (k + 1u) % 10u), b[7] = 0;
        else
            b[5] = (char)('1' + k), b[6] = 0;
        aa_box(0, y, 240, 36, 18, pc, PF_BG);
        aa_text(16, y + 2, &AF_PM, b, PF_BG, pc);
        AA_MID(130, y, 100, 36, PS, used ? "STORED" : "EMPTY", PF_BG, pc);
    }
    AA_MID(0, 210, 240, 24, PS, "SAVE: STORE   HOLD SAVE: LOAD", PF_PILL, PF_BG);
}
static void pf_paint_page(void)
{
    static const char *const TN[NTRK] = {"LAYER A", "LAYER B", "PLAYER", "DRUMS"};
    uint32_t sel = song.sel % NTRK, n = pfm_rows(), r = ui.row[SCR_PERF] % n, k;
    uint16_t ec = trk_col(sel);
    int32_t tw = aa_w(&AF_PS, TN[sel]) + 16;
    aa_box(0, 0, 240, 34, 17, PF_PILL, PF_BG);                          /* the title */
    aa_text(12, 2, &AF_PM, pf_page == PG_FX ? PG_BANKS[PG_B0[PG_FX] + r % 5u].title : PG_NAME[pf_page], PF_BG, PF_PILL);   /* (FX: the effect's own name) */
    if (PG_TRACKS(pf_page) || pf_page == PG_FX) {                       /* (the track's pages: which one) */
        aa_box(233 - tw, 6, tw, 22, 11, ec, PF_PILL);
        AA_MID(233 - tw, 6, tw, 22, PS, TN[sel], ink_on(ec), ec);
    }
    for (k = 0; n > 1u && pf_page != PG_SAVE && k < n; k++)                                   /* the banks: a dot each, this one filled */
        aa_box(120 - (int32_t)n * 6 + (int32_t)k * 12, 38, 8, 8, 4, k == r ? PF_PILL : PF_TAG, PF_BG);
    if (pf_page == PG_SAVE) {
        pf_paint_save(r);
        return;
    }
    if (PG_TRACKS(pf_page) && is_drum(TSEL)) {
        aa_box(0, 92, 240, 40, 20, PF_PILL, PF_BG);
        AA_MID(0, 92, 240, 40, PM, "LAYERS ONLY", PF_BG, PF_PILL);
        return;
    }
    if (pf_page == PG_ENV || pf_page == PG_LFO) {      /* the shape, the four controls under it */
        if (pf_page == PG_ENV)
            pf_graph_env();
        else
            pf_graph_lfo();
        pf_boxes(r);
        return;
    }
    for (k = 0; k < 4u; k++) {
        cell_t c;
        pf_cellv(r, k, &c);
        pf_card((int32_t)(k & 1u) * 122, 50 + (int32_t)(k >> 1) * 96, 118, 94, &c);   /* (the lower two to the foot) */
    }
}
static void pf_paint(void)
{
    if (pf_mix)
        pf_paint_mix();
    else if (pf_page == PG_PERF)
        pf_paint_perf(ui.row[SCR_PERF] % PFR_N);
    else
        pf_paint_page();
}
static void pfm_draw(void)
{
    char a[14], b[14], d[14];
    uint32_t h = 0x9E5Fu + pf_page * 977u + song.sel * 31u, r = ui.row[SCR_PERF] % PFR_N, i, k;
    if (pf_page == PG_PERF && !PF_SPLITR(r) && song.sel != PF_TRKR[r] && !(ui.toast_t && pop_kind) && !op_armed())
        track_select(PF_TRKR[r]);                        /* (the row's track: PRESETS, ALGORITHM act on it; not while PRESETS'
                                                         * list previews on the track it opened on) */
    snd_name(0, a), snd_name(1, b), snd_name(TRK_DRUM, d);
    for (i = 0; a[i]; i++)
        h = hu(h, (uint8_t)a[i]);
    for (i = 0; b[i]; i++)
        h = hu(h, (uint8_t)b[i]);
    for (i = 0; d[i]; i++)
        h = hu(h, (uint8_t)d[i]);
    for (k = 0; k < 4u; k++) {
        cell_t c;
        pf_cellv(r, k, &c);
        for (i = 0; c.val[i]; i++)
            h = hu(h, (uint8_t)c.val[i]);
        h = hu(h, (uint32_t)(uint16_t)c.gv);
        h = hu(h, c.label ? (uint32_t)c.label[0] : 0u);
    }
    if (pf_page != PG_PERF)
        r = ui.row[SCR_PERF];                           /* (a page's bank or slot) */
    if (pf_page == PG_SAVE)
        for (k = 0; k < PROJ_SLOTS; k++)
            h = hu(h, (uint32_t)project_slot_used(k));
    h = hu(hu(h, (uint32_t)(uint16_t)TSEL->p[P_LWAVE]), perf.theme);
    h = hu(hu(hu(h, r), trk[0].engine | (uint32_t)trk[1].engine << 8),
           perf.mode | (uint32_t)perf.split << 8 | (uint32_t)perf.width << 16 | (uint32_t)perf.rev << 24);
    if (ui.sig[2] == h)
        return;
    ui.sig[2] = h;
    cv_tall(0, PF_H, PF_BG, pf_paint);
}

/* ---- the drums' steps (SEQ: Optimist's STEP on the drum track, drawn here; its keys as STEP's: the black keys pick the
 * lane, the white keys set the window's sixteen steps; OCT- / OCT+ and SEQ again: the bar) */
static void pf_seq_paint(void)
{
    const track_t *t = TDRUM;
    uint32_t l = lane_selected(), page = st.page, k, n, play = song.playing ? t->seq_idx : 0xFFFFu;
    char b[16];
    aa_box(0, 0, 240, 34, 17, PF_PILL, PF_BG);
    aa_text(12, 2, &AF_PM, "SEQ", PF_BG, PF_PILL);
    {                                                   /* the length in bars (of four steps), the screen of sixteen shown */
        uint32_t np = step_pages(t), bars = trk_len(t) / 4u;
        str_cpy(b, "16 BARS", sizeof b);
        if (bars < 10u)
            b[0] = (char)('0' + bars), str_cpy(b + 1, " BARS", sizeof b - 1);
        else
            b[1] = (char)('0' + bars % 10u);
        aa_box(130, 6, 64, 22, 11, PF_TAG, PF_PILL);
        AA_MID(130, 6, 64, 22, PS, b, PF_BG, PF_TAG);
        b[0] = (char)('1' + page % 10u), b[1] = '/', b[2] = (char)('0' + np % 10u), b[3] = 0;
        aa_box(198, 6, 36, 22, 11, PF_SEL, PF_PILL);
        AA_MID(198, 6, 36, 22, PS, b, PF_BG, PF_SEL);
    }
    aa_box(0, 40, 240, 32, 16, trk_col(TRK_DRUM), PF_BG);              /* the lane */
    AA_MID(0, 40, 240, 32, PM, lane_name(l), ink_on(trk_col(TRK_DRUM)), trk_col(TRK_DRUM));
    for (k = 0; k < 16u; k++) {                                         /* its sixteen steps: two rows of eight, as the keys */
        uint32_t i = page * 16u + k;
        int on = dstep_has(&t->dstep[i % NSTEP], l), now = i == play;
        int32_t x = (int32_t)(k % 8u) * 30, y = 78 + (int32_t)(k / 8u) * 40;
        aa_box(x + 1, y, 28, 36, 8, now ? PF_SEL : on ? PF_PILL : PF_TAG, PF_BG);
        if (!(k % 4u))
            cv_rect(x + 12, y + 30, 6, 3, on || now ? PF_BG : PF_PILL);   /* (a beat's first step) */
    }
    for (n = 0; n < DRUM_LANES; n++)                                    /* every lane, small */
        for (k = 0; k < 16u; k++) {
            uint32_t i = page * 16u + k;
            int on = dstep_has(&t->dstep[i % NSTEP], n);
            if (i == play || on || n == l)
                cv_rect((int32_t)k * 15 + 1, 162 + (int32_t)n * 7, 13, 5, i == play ? PF_SEL : on ? (n == l ? PF_SEL : PF_PILL) : PF_TAG);
            else
                cv_rect((int32_t)k * 15 + 6, 164 + (int32_t)n * 7, 3, 1, PF_TAG);   /* (an empty step: a dot) */
        }
}
static void pf_seq_draw(void)
{
    const track_t *t = TDRUM;
    uint32_t h = 0x5E9u + perf.theme, k, l;
    for (l = 0; l < DRUM_LANES; l++)
        for (k = 0; k < 16u; k++)
            h = hu(h, (uint32_t)dstep_has(&t->dstep[(st.page * 16u + k) % NSTEP], l));
    h = hu(hu(hu(hu(h, lane_selected()), st.page), song.playing ? t->seq_idx : 0xFFFFu), (uint32_t)t->p[P_SLEN]);
    if (ui.sig[2] == h)
        return;
    ui.sig[2] = h;
    cv_tall(0, PF_H, PF_BG, pf_seq_paint);
}
