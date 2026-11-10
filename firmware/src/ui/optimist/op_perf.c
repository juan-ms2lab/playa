/* SPDX-License-Identifier: GPL-3.0-only */
/* JTS22 PLAYA, a performance build for the M-VAVE FM-1: created by JFS with help from Claude (Anthropic), 2026.
 * Built on Optimist by w0ts and contributors (github.com/w0ts/optimist), GPL-3.0-only; this file is part of that work
 * and under the same licence (LICENSE). */
/* PERFORM (JTS22, builder item PERF; io/midi/perf.c): the home screen of the performance build, after the user's sketch
 * (JTS22/intf.png): layer A's sound and engine, the SPLIT, layer B's, the drum kit and the song, the four effect values.
 * Rows (SELECT), as on the screen top down: A, MODE, NOTE, B, DRUMS, PLAYER (tracks 1, 2, the drum track, track 3: the
 * song's part). On a track's row the track is selected (PRESETS, ALGORITHM act on it as everywhere) and KNOB 1..4 are its
 * REV CHO DLY FILT (the sketch's order; the drum track: its kit's sends, its FILTER). On MODE and NOTE (the user,
 * 2026-11-10: "allow select to move to the mode and to the note") KNOB 1..4 are MODE NOTE WIDTH DIR, and PRESETS steps the
 * mode, or the note a semitone (ALGORITHM: an octave): pf_enc. */
static void track_select(uint32_t i);                   /* op_input.c */
static void pfm_draw(void);                              /* op_perfdraw.c */
static void pf_seq_draw(void);
#define PF_FULL (ui.scr == SCR_PERF || ui.scr == SCR_STEP)   /* (op_draw.c: the whole screen, no header, no cards) */
static void cv_tall(uint32_t y, uint32_t h, uint16_t bg, void (*fn)(void));   /* op_graph.c */
enum { PFR_A, PFR_MODE, PFR_NOTE, PFR_B, PFR_DR, PFR_N };   /* (the song's row gone with the player: the user, 2026-11-10) */
static const uint8_t PF_TRK[4] = {0, 1, TRK_DRUM, 2};  /* (MIX's four, in order) */
#define PF_NONE 0xFFu
static const uint8_t PF_TRKR[PFR_N] = {0, PF_NONE, PF_NONE, 1, TRK_DRUM};   /* a row's track */
#define PF_SPLITR(r) (PF_TRKR[(r) % PFR_N] == PF_NONE)
static const char *const PF_ROW[PFR_N] = {"LAYER A", "MODE", "NOTE", "LAYER B", "DRUMS"};
static const char *const N_PFMODE[] = {"LAYER", "SPLIT", "BLEND", "SINGLE"};
static const char *const N_PFDIR[] = {"A-B", "B-A"};
static const param_desc_t PF_SPLIT_D[4] = {PE("MODE", N_PFMODE, PERF_BLEND), PD("NOTE", F_INT, 0, 127, 60),
                                           PD("WIDTH", F_INT, 0, 24, 12), PE("DIR", N_PFDIR, 0)};
static int16_t pfm_v[4];                                 /* (perf's bytes as the cards' values) */
static const uint8_t PF_FX[4] = {P_REV, P_CHOR, P_DLY, P_TFLT};

/* The pages (the user, 2026-11-10: "one screen per button (some scrollable)"): each panel button one page of the selected
 * track, four cards a bank (KNOB 1..4), SELECT scrolls the banks. HOME PERFORM, SEL (the panel's SCL) MIX: the four
 * levels; FX REV CHO DLY and the low-pass sweep; ENV the ADSR; LFO rate, wave, its two destinations; EDIT the engine's
 * eight values; ARP the arpeggiator; GLO tuning, tempo, the master's compressor and limiter; SAVE the four performance
 * slots (SELECT one, SAVE tapped: stored, SAVE held: loaded). ENV and LFO draw their shape above the four boxes (the
 * user: "a graph with controls in four boxes in the bottom like the home screen"). ALGORITHM is the selected track's
 * level everywhere (pf_enc). PLAY, REC, SEQ: nothing (the sequencer is gone; a looper in v1.5). */
enum { PG_PERF, PG_MIX, PG_FX, PG_ENV, PG_LFO, PG_EDIT, PG_ARP, PG_GLO, PG_SAVE, PG_N };
static const char *const PG_NAME[PG_N] = {"PERFORM", "MIX", "FX", "ENVELOPE", "LFO", "EDIT", "ARP", "GLOBAL", "SAVE"};
static const page_t PG_BANKS[] = {
    {"ADSR", FAM_ENV, SC_TRACK, GR_ADSR, {P_ATK, P_DEC, P_SUS, P_REL}},
    {"LFO", FAM_LFO, SC_TRACK, GR_LFO, {P_LRATE, P_LWAVE, P_L1TRIG, P_L1POL}},
    {"LFO DEST", FAM_LFO, SC_TRACK, GR_NONE, {P_L1DA, P_L1AA, P_L1DB, P_L1AB}},
    {"EDIT 1", FAM_EDIT, SC_ENGINE, GR_NONE, {P_E0, P_E1, P_E2, P_E3}},
    {"EDIT 2", FAM_EDIT, SC_ENGINE, GR_NONE, {P_E4, P_E5, P_E6, P_E7}},
    {"ARP", FAM_ARP, SC_TRACK, GR_ARP, {P_AMODE, P_ARATE, P_AOCT, P_AGATE}},
    {"ARP 2", FAM_ARP, SC_TRACK, GR_NONE, {P_ASWING, P_APROB, P_AHOLD, P_AORDER}},
    {"GLOBAL", FAM_GLO, SC_GLOBAL, GR_NONE, {G_TUNE, G_BPM, 0xFF, 0xFF}},
    {"COMP", FAM_GLO, SC_GLOBAL, GR_NONE, {G_CTHR, G_CRAT, G_CATK, G_CREL}},
    {"LIMIT", FAM_GLO, SC_GLOBAL, GR_NONE, {G_CGAIN, G_CCEIL, 0xFF, 0xFF}},
    {"SENDS", FAM_FX, SC_TRACK, GR_NONE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (FX 1: the track's four, pfm_desc) */
    {"REVERB", FAM_FX, SC_GLOBAL, GR_NONE, {G_RSIZE, G_RDAMP, 0xFF, 0xFF}},
    {"CHORUS", FAM_FX, SC_GLOBAL, GR_NONE, {G_CRATE, G_CDEPTH, 0xFF, 0xFF}},
    {"DELAY", FAM_FX, SC_GLOBAL, GR_NONE, {G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX}},
    {"FILTER", FAM_FX, SC_TRACK, GR_NONE, {0xFF, 0xFF, 0xFF, 0xFF}},  /* (FX 5: the sweep and RESO, pfm_desc) */
};
/* (the user, 2026-11-10: "one screen for each fx (room size, delay ms etc)"; FX pressed again: the next) */
static const uint8_t PG_B0[PG_N] = {0, 0, 10, 0, 1, 3, 5, 7, 0};  /* a page's first bank in PG_BANKS, and how many */
static const uint8_t PG_BN[PG_N] = {0, 0, 5, 1, 2, 2, 2, 5, 0};   /* (GLOBAL's 4th and 5th: the knobs' CCs, pf_ccd) */
#define PF_FXC(r) (pf_page == PG_FX && ((r) % 5u == 0u || (r) % 5u == 4u))   /* FX's own screens: SENDS, FILTER */
#define PF_CC_ROW(r) (pf_page == PG_GLO && (r) % 5u >= 3u)   /* GLOBAL's CC 1-4, CC 5-8 (pf_ccd below) */
#define PG_TRACKS(pg) ((pg) == PG_ENV || (pg) == PG_LFO || (pg) == PG_EDIT || (pg) == PG_ARP)   /* the selected track's */
static uint8_t pf_page, pf_prow[PG_N];
#define pf_mix (pf_page == PG_MIX)
static const page_t *pf_bank(uint32_t r)                /* the bank of row r on a page of banks, 0 = not one */
{
    if (!PG_BN[pf_page] || (PG_TRACKS(pf_page) && is_drum(TSEL)) || PF_FXC(r) || PF_CC_ROW(r))
        return 0;                                       /* (the drum track: its sound is the kit's, not here) */
    return &PG_BANKS[PG_B0[pf_page] + r % PG_BN[pf_page]];
}
static uint32_t pfm_rows(void)
{
    return pf_page == PG_PERF ? PFR_N : pf_page == PG_SAVE ? PROJ_SLOTS : PG_BN[pf_page] ? PG_BN[pf_page] : 1u;
}
static void pfm_name(uint32_t r, char *b)
{
    const page_t *pg = pf_bank(r);
    str_cpy(b, pf_page == PG_PERF ? PF_ROW[r % PFR_N] : pg ? pg->title : PG_NAME[pf_page], 10);
}
static void op_engine_step(int32_t s);                  /* op_screens.c */
static void pre_toast_engine(void);                     /* op_preset.c */
static void pf_go(uint32_t pg)                          /* to page pg, its row as it was left */
{
    pf_prow[pf_page] = ui.row[SCR_PERF];
    pf_page = (uint8_t)(pg % PG_N);
    if (ui.scr != SCR_PERF)
        op_enter(SCR_PERF);
    ui.row[SCR_PERF] = pf_prow[pf_page] < pfm_rows() ? pf_prow[pf_page] : 0u;
    ui.sig[2] = 0;
}
/* GLOBAL's THEME (KNOB 3 on its first bank; the user, 2026-11-10: "blue white yellow is default, but add a black,
 * white, pink and a cream brown white") */
static const char *const N_PFTHEME[] = {"BLUE", "MONOCHROME", "CREAM", "PLAYA", "ROLO"};
static const param_desc_t PF_THEME_D = PE("THEME", N_PFTHEME, 0);
static int16_t pf_thv;
#define PF_THEME_CELL(r, k) (pf_page == PG_GLO && (r) == 0u && (k) == 2u)
/* GLOBAL's CC 1-4, CC 5-8: the controller knobs' CCs (perf.cc, io/midi/perf.c), named by what each knob does */
static const param_desc_t PF_CC_D[8] = {PD("KNOB 1", F_INT, 0, 127, 70), PD("KNOB 2", F_INT, 0, 127, 71),
                                        PD("KNOB 3", F_INT, 0, 127, 72), PD("KNOB 4", F_INT, 0, 127, 73),
                                        PD("SELECT", F_INT, 0, 127, 74), PD("PRESETS", F_INT, 0, 127, 75),
                                        PD("ALGO", F_INT, 0, 127, 76), PD("LEVEL", F_INT, 0, 127, 77)};
static int16_t pf_ccv;
/* FILT as a low-pass sweep (the user: "filter is a sweepable low pass"): 0 closed .. 100 open = the track FILTER off;
 * P_TFLT's low-pass half (-64 .. 0) underneath, its high-pass half not reached from here */
static int16_t pf_lp;
static const param_desc_t PF_LP_D = PD("FILT", F_PCT, 0, 100, 100);
static const param_desc_t *pfm_desc(uint32_t r, uint32_t k, int16_t **vp)
{
    uint32_t t;
    *vp = 0;
    k &= 3u;
    if (pf_mix && k == 3u) {                            /* MIX's 4th: the master compressor's THRS (GLOBAL > COMP; the user,
                                                         * 2026-11-10), 0 dB: off */
        *vp = &song.g[G_CTHR];
        return &GP[G_CTHR];
    }
    if (pf_mix)
        return mix_desc(P_LEVEL, PF_TRK[k], vp);        /* (the drum track's: its own LEVEL, GLO > DRUMS) */
    if (pf_page == PG_FX && r % 5u == 4u) {            /* FILTER: the sweep, its resonance (GLOBAL: every track's) */
        t = song.sel % NTRK;
        if (k == 1u && FELUCCA_TFLT_RESO) {
            *vp = &song.g[G_TFRES];
            return &GP[G_TFRES];
        }
        if (k != 0u)
            return 0;
        k = 3u;
    } else if (pf_page == PG_FX)
        t = song.sel % NTRK;                            /* (FX: the selected track's four) */
    else if (PF_SPLITR(r)) {
        pfm_v[0] = perf.mode, pfm_v[1] = perf.split, pfm_v[2] = perf.width, pfm_v[3] = perf.rev;
        *vp = &pfm_v[k];
        return &PF_SPLIT_D[k];
    } else
        t = PF_TRKR[r % PFR_N];
    if (k == 3u && (t != TRK_DRUM || FELUCCA_TRK_FILT)) {   /* FILT: the low-pass sweep */
        int32_t f = trk[t].p[P_TFLT];
        pf_lp = (int16_t)(f >= 0 ? 100 : 100 + f * 100 / 64);
        *vp = &pf_lp;
        return &PF_LP_D;
    }
    if (t == TRK_DRUM && k < 3u) {                      /* (the drum track's sends are its kit's: REV DLY CHO there) */
        static const param_desc_t DKS_D[3] = {PD("REV", F_PCT, 0, 127, DKS_DEF), PD("CHO", F_PCT, 0, 127, DKS_DEF),
                                              PD("DLY", F_PCT, 0, 127, DKS_DEF)};
        static const uint8_t DK[3] = {0, 2, 1};
        *vp = &dsend_kit[DK[k]];
        return &DKS_D[k];
    }
    return mix_desc(PF_FX[k], t, vp);                   /* (the mixer's: none where the build has not the value) */
}
static void pfm_cell(uint32_t r, uint32_t k, cell_t *c)
{
    int16_t *vp;
    const page_t *pg = pf_bank(r);
    if (PF_THEME_CELL(r, k)) {
        pf_thv = perf.theme;
        cell_param(c, &PF_THEME_D, &pf_thv);
        return;
    }
    if (PF_CC_ROW(r)) {                                 /* (a knob's CC: its number, OFF at 0) */
        uint32_t i = (r % 5u - 3u) * 4u + (k & 3u);
        pf_ccv = perf.cc[i];
        cell_param(c, &PF_CC_D[i], &pf_ccv);
        if (!pf_ccv)
            str_cpy(c->val, "OFF", sizeof c->val);
        return;
    }
    if (pg) {
        page_cell(pg, k, c);
        return;
    }
    if (PG_BN[pf_page] && !PF_FXC(r))
        return;                                         /* (a bank page on the drum track: nothing) */
    const param_desc_t *d = pfm_desc(r % PFR_N, k, &vp);
    cell_param(c, d, vp);
    if (pf_page == PG_PERF && PF_SPLITR(r) && k == 1u)
        note_name(c->val, perf.split);                  /* (NOTE as a note: C4) */
    if (c->d)
        c->col = PF_SPLITR(r) ? C_HI : trk_col(PF_TRKR[r % PFR_N]);
}
static void pfm_turn(uint32_t r, uint32_t k, int32_t s, int fine)
{
    int16_t *vp;
    const param_desc_t *d;
    const page_t *pg = pf_bank(r);
    if (PF_THEME_CELL(r, k)) {
        pf_thv = perf.theme;
        val_turn(&PF_THEME_D, &pf_thv, k, s, fine);
        perf.theme = (uint8_t)pf_thv;
        ui.sig[2] = 0;
        return;
    }
    if (PF_CC_ROW(r)) {
        uint32_t i = (r % 5u - 3u) * 4u + (k & 3u);
        pf_ccv = perf.cc[i];
        val_turn(&PF_CC_D[i], &pf_ccv, k, s, fine);
        perf.cc[i] = (uint8_t)pf_ccv;
        return;
    }
    if (pg) {
        page_turn(pg, k, s, fine);
        return;
    }
    if (PG_BN[pf_page] && !PF_FXC(r))
        return;
    d = pfm_desc(r % PFR_N, k, &vp);
    val_turn(d, vp, k, s, fine);
    if (vp == &pf_lp) {                                 /* (back to P_TFLT: 100 off, below it the low-pass) */
        uint32_t t = pf_page == PG_FX ? song.sel % NTRK : PF_TRKR[r % PFR_N];   /* (FILTER's knob 1: the sweep too) */
        trk[t].p[P_TFLT] = (int16_t)(pf_lp >= 100 ? 0 : -((100 - pf_lp) * 64 + 99) / 100);
    }
    if (pf_page == PG_PERF && PF_SPLITR(r)) {                      /* (back into perf: read by the audio interrupt, a byte each) */
        perf.mode = (uint8_t)pfm_v[0], perf.split = (uint8_t)pfm_v[1], perf.width = (uint8_t)pfm_v[2], perf.rev = (uint8_t)pfm_v[3];
    }
}
static int pfm_yes(uint32_t r, uint32_t k, uint32_t ok)
{
    (void)r, (void)k, (void)ok;
    return 0;
}

/* PRESETS and ALGORITHM on MODE and NOTE (op_input.c op_knobs, before their usual jobs): 1 = taken */
static int pf_enc(void)
{
    uint32_t r = ui.row[SCR_PERF] % PFR_N;
    int32_t p, a, v;
    if ((v = perf_enc_take(PERF_LEVEL)) != 0) {         /* the controller's LEVEL knob (K8): the selected track's */
        int16_t *vp;
        const param_desc_t *d = mix_desc(P_LEVEL, song.sel % NTRK, &vp);
        val_turn(d, vp, 0, v, 0);
        ui.sig[2] = 0;
    }
    if (ui.scr == SCR_STEP && is_drum(TSEL)) {          /* SEQ: ALGORITHM the length, four bars (of four steps) a detent, 4 .. 16
                                                         * bars (the user, 2026-11-10: "default at 4, increase by 4s") */
        if ((a = panel_enc(EN_ALGO)) == 0)
            return 0;
        v = (int32_t)TDRUM->p[P_SLEN] / 16 * 16 + 16 * a;
        TDRUM->p[P_SLEN] = (int16_t)(v < 16 ? 16 : v > 64 ? 64 : v);
        if (st.page >= step_pages(TDRUM))
            st.page = (uint8_t)(step_pages(TDRUM) - 1u);
        return 1;
    }
    if (ui.scr != SCR_PERF)
        return 0;
    if (pf_page != PG_PERF || !PF_SPLITR(r)) {          /* ALGORITHM: the selected track's engine (the drums: the kit; the user,
                                                         * 2026-11-10: "use to skip to next engine") */
        if ((a = panel_enc(EN_ALGO)) == 0)
            return 0;
        op_engine_step(a);
        if (!op_armed())
            pre_toast_engine();
        return 1;
    }
    p = panel_enc(EN_PRESET), a = panel_enc(EN_ALGO);
    if (!p && !a)
        return 0;
    if (r == PFR_MODE) {                                /* LAYER SPLIT BLEND SINGLE, round */
        v = ((int32_t)perf.mode + p + a) % (int32_t)PERF_NMODE;
        perf.mode = (uint8_t)(v < 0 ? v + (int32_t)PERF_NMODE : v);
    } else {
        v = (int32_t)perf.split + p + 12 * a;
        perf.split = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    }
    return 1;
}
/* a panel button tapped (op_input.c op_tap, first): 1 = taken */
static void prj_load_go(uint32_t s);                    /* op_edits.c */
static int prj_save_go(uint32_t s);
static int project_slot_used(uint32_t p);
static void step_scroll(int32_t d);                     /* op_step.c */
static void pf_page_tap(uint32_t pg)                    /* a page's button: the page, again: its next screen */
{
    if (ui.scr == SCR_PERF && pf_page == pg) {
        ui.row[SCR_PERF] = (uint8_t)((ui.row[SCR_PERF] + 1u) % pfm_rows());
        ui.sig[2] = 0;
    } else {
        pf_go(pg);
    }
}
static int pf_tap(uint32_t b, int hold)
{
    switch (b) {
    case B_SEQ:                                         /* the drums' steps (the user, 2026-11-10: "restore the sequencer for the
                                                         * drums"): STEP on the drum track; again: the next bar */
        if (ui.scr == SCR_STEP && is_drum(TSEL)) {
            step_scroll(st.page + 1u < step_pages(TDRUM) ? 1 : -(int32_t)st.page);   /* (the next screen of sixteen, round) */
            return 1;
        }
        track_select(TRK_DRUM);
        op_enter(SCR_STEP);
        return 1;
    case B_ARP: pf_page_tap(PG_ARP); return 1;
    case B_GLO: pf_page_tap(PG_GLO); return 1;
    case B_SAVE:
        if (ui.scr != SCR_PERF || pf_page != PG_SAVE)
            pf_go(PG_SAVE);
        else if (!hold)
            (void)prj_save_go(ui.row[SCR_PERF] % PROJ_SLOTS);   /* (SAVE tapped on SAVE: stored, "SAVED") */
        else if (project_slot_used(ui.row[SCR_PERF] % PROJ_SLOTS))
            prj_load_go(ui.row[SCR_PERF] % PROJ_SLOTS);         /* (held: that performance back) */
        else
            ui_message("EMPTY SLOT");
        ui.sig[2] = 0;
        return 1;
    case B_HOME: pf_page_tap(PG_PERF); return 1;
    case B_SCL: pf_page_tap(PG_MIX); return 1;               /* (printed SEL on the user's FM-1) */
    case B_FX: pf_page_tap(PG_FX); return 1;
    case B_ENV: pf_page_tap(PG_ENV); return 1;
    case B_LFO: pf_page_tap(PG_LFO); return 1;
    case B_EDIT: pf_page_tap(PG_EDIT); return 1;
    case B_REC: return 1;                               /* (no recording: the looper, v1.5) */
    default: return 0;
    }
}
/* the button of the page shown, lit (op_input.c op_screen_btn) */
static uint32_t pf_btn(void)
{
    static const uint8_t B[PG_N] = {B_HOME, B_SCL, B_FX, B_ENV, B_LFO, B_EDIT, B_ARP, B_GLO, B_SAVE};
    return ui.scr == SCR_STEP ? B_SEQ : B[pf_page % PG_N];
}
