/* SPDX-License-Identifier: GPL-3.0-only */
/* JTS22 PLAYA, a performance build for the M-VAVE FM-1: created by JFS with help from Claude (Anthropic), 2026.
 * Built on Optimist by w0ts and contributors (github.com/w0ts/optimist), GPL-3.0-only; this file is part of that work
 * and under the same licence (LICENSE). */
/* PERFORM's renderer (JTS22): anti-aliased Inter Tight text and pills on the canvas (display/gfx.c cv_px). The glyph
 * tables are made by tools/gen_aa_font.py (perf_font.h: PS 14 px / 700 labels, PM 24 px / 800 names and values, PL
 * 44 px / 800 capitals and figures: the mode and the note). The text part is Golana's (github.com/Gadlele/Golana
 * dbae4640, firmware/src/gfx.c, Leo Kuroshita, Hügelton Instruments, GPL-3.0-only): glyph lookup, kerning, the phase
 * placement, cv_alpha, the 16-step blend ramp; without its Huffman faces, hooks and themes. Regenerate the fonts:
 *   DYLD_LIBRARY_PATH=/opt/homebrew/lib tools/builder/venv/bin/python tools/gen_aa_font.py \
 *     firmware/src/ui/optimist/perf_font.h --huff "" --face "PS=assets/fonts/InterTight[wght].ttf@700:14:32-126/2" \
 *     --face "PM=...@800:24:32-126/2" --face "PL=...@800:44:32-32+<# + - . / 0-9 A-Z as 0xNN,...>/1"
 * (Pillow with Raqm: brew install libraqm) */
typedef struct { uint16_t off, adv; int8_t bx; uint8_t by, bw, bh; } aag_t;
typedef struct {
    uint8_t h, asc, first, last, nex;   /* line height, baseline; ASCII range; extras */
    uint16_t nk;                        /* kerning pairs */
    const aag_t *g;
    const uint8_t *data;
    const uint16_t *kkey;               /* a << 8 | b, sorted */
    const int8_t *kd;                   /* 1/16 px */
    const uint8_t *ex;                  /* codes of the extra glyphs */
    uint8_t psh;                        /* log2 of the phases per glyph */
    const uint8_t *hc;                  /* (Huffman: none here) */
} aafont_t;
#include "perf_font.h"

/* the 16 steps from bg to fg, as stored (byte-swapped); a few pairs kept */
#define AA_NRAMP 6u
static struct { uint16_t fg[AA_NRAMP], bg[AA_NRAMP], v[AA_NRAMP][16]; uint8_t n, next; } aa_rc;
static const uint16_t *aa_ramp(uint16_t fg, uint16_t bg)
{
    static const uint8_t SH[3] = {11, 5, 0};
    static const uint8_t MK[3] = {31, 63, 31};
    uint32_t i, a, k;
    for (i = 0; i < aa_rc.n; i++)
        if (aa_rc.fg[i] == fg && aa_rc.bg[i] == bg)
            return aa_rc.v[i];
    i = aa_rc.next;
    aa_rc.next = (uint8_t)((i + 1u) % AA_NRAMP);
    if (aa_rc.n < AA_NRAMP)
        aa_rc.n++;
    aa_rc.fg[i] = fg, aa_rc.bg[i] = bg;
    for (a = 0; a < 16u; a++) {
        uint32_t out = 0;
        int32_t w = (int32_t)(a * 256u / 15u);
        for (k = 0; k < 3u; k++) {
            int32_t x = (bg >> SH[k]) & MK[k], y = (fg >> SH[k]) & MK[k];
            out |= (uint32_t)(x + (((y - x) * w + 128) >> 8)) << SH[k];
        }
        aa_rc.v[i][a] = swap16(out);
    }
    return aa_rc.v[i];
}

/* w x h nibbles, rows back to back, at canvas (x, y) */
static void aa_alpha(int32_t x, int32_t y, uint32_t w, uint32_t h, const uint8_t *d, const uint16_t *rv)
{
    uint32_t k = 0, gx, gy;
    y += cv_oy;
    for (gy = 0; gy < h; gy++) {
        int32_t py = y + (int32_t)gy;
        uint16_t *row;
        if ((uint32_t)py >= cv_h) {
            k += w;
            continue;
        }
        row = cv_px + (uint32_t)py * cv_w;
        for (gx = 0; gx < w; gx++, k++) {
            uint32_t v = (d[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            int32_t px = x + (int32_t)gx;
            if (v && (uint32_t)px < cv_w)
                row[px] = rv[v];
        }
    }
}

static int32_t aa_glyph_at(const aafont_t *f, uint32_t ch)
{
    uint32_t i;
    if (ch >= f->first && ch <= f->last)
        return (int32_t)(ch - f->first);
    for (i = 0; i < f->nex; i++)
        if (f->ex[i] == ch)
            return (int32_t)(f->last - f->first + 1u + i);
    return -1;
}
static uint32_t aa_fold(const aafont_t *f, uint32_t c) { return c >= 'a' && c <= 'z' && f->last < 'a' ? c - 32u : c; }
static uint32_t aa_glyph(const aafont_t *f, uint32_t ch)   /* the entry at phase 0; missing: '?', else the first */
{
    int32_t k = aa_glyph_at(f, aa_fold(f, ch));
    if (k < 0 && (k = aa_glyph_at(f, '?')) < 0)
        k = 0;
    return (uint32_t)k << f->psh;
}
static int32_t aa_kern(const aafont_t *f, uint32_t a, uint32_t b)   /* 1/16 px */
{
    uint32_t key = (a << 8) | b, lo = 0, hi = f->nk;
    while (lo < hi) {
        uint32_t m = (lo + hi) / 2u;
        if (f->kkey[m] == key)
            return f->kd[m];
        if (f->kkey[m] < key)
            lo = m + 1u;
        else
            hi = m;
    }
    return 0;
}
static int32_t aa_w(const aafont_t *f, const char *s)
{
    int32_t pen = 0;
    uint32_t prev = 0;
    for (; *s; s++) {
        uint32_t c = aa_fold(f, (uint8_t)*s);
        if (prev)
            pen += aa_kern(f, prev, c);
        pen += f->g[aa_glyph(f, c)].adv;
        prev = c;
    }
    return (pen + 8) >> 4;
}
/* y = the line's top (the baseline y + f->asc); fg on bg; returns the pen x at the end */
static int32_t aa_text(int32_t x, int32_t y, const aafont_t *f, const char *s, uint16_t fg, uint16_t bg)
{
    const uint16_t *rv = aa_ramp(fg, bg);
    int32_t pen = 0, err = 0;
    const int32_t step = 16 >> f->psh;
    uint32_t prev = 0;
    for (; *s; s++) {
        uint32_t c = aa_fold(f, (uint8_t)*s);
        const aag_t *g;
        int32_t pos, d0, d1;
        if (prev)
            pen += aa_kern(f, prev, c);
        g = &f->g[aa_glyph(f, c)];
        if (f->psh && g[1].off != g->off) {
            pos = pen - (pen & (step - 1));          /* the phase at or left of the pen, or the one right: the truer gap */
            d0 = pos - pen - err;
            d1 = d0 + step;
            if ((d0 < 0 ? -d0 : d0) > (d1 < 0 ? -d1 : d1))
                pos += step;
            g += ((uint32_t)pos >> (4u - f->psh)) & ((1u << f->psh) - 1u);
        } else {
            pos = (pen + 8) & ~15;
        }
        err = pos - pen;
        if (g->bw)
            aa_alpha(x + (pos >> 4) + g->bx, y + g->by, g->bw, g->bh, f->data + g->off, rv);
        pen += g->adv;
        prev = c;
    }
    return x + ((pen + 8) >> 4);
}
/* s centred in the box w x h: across by its advance, up and down by the capitals' band (the ink of H) */
static void aa_mid(int32_t x, int32_t y, int32_t w, int32_t h, const aafont_t *f, int32_t cap_y, int32_t cap_h,
                   const char *s, uint16_t fg, uint16_t bg)
{
    aa_text(x + (w - aa_w(f, s)) / 2, y + (h - cap_h) / 2 - cap_y, f, s, fg, bg);
}
#define AA_MID(x, y, w, h, F, s, fg, bg) aa_mid(x, y, w, h, &AF_##F, AF_##F##_CAP_Y, AF_##F##_CAP_H, s, fg, bg)

/* a filled rounded box (radius r; r = h / 2: a pill), its curved edges anti-aliased against bg */
static uint32_t aa_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    for (; b; b >>= 2)
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
    return r;
}
static void aa_box(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c, uint16_t bg)
{
    const uint16_t *rv = aa_ramp(c, bg);
    int32_t j;
    if (r > h / 2)
        r = h / 2;
    for (j = 0; j < h; j++) {
        int32_t cy16 = j < r ? (r * 16 - 8 - j * 16) : j >= h - r ? (j * 16 + 8 - (h - r) * 16) : -1, in16 = 0;
        if (cy16 >= 0) {                                /* a corner's row: the curve's inset, in 1/16 px */
            int32_t r16 = r * 16;
            in16 = r16 - (int32_t)aa_isqrt((uint32_t)(r16 * r16 - cy16 * cy16));
        }
        {
            int32_t ip = in16 >> 4, a = 15 - ((in16 & 15) * 15 + 8) / 16;
            cv_rect(x + ip + 1, y + j, w - 2 * ip - 2, 1, c);
            if (in16 & 15) {                            /* the edge pixels: their coverage */
                cv_pset(x + ip, y + j, swap16(rv[a]));
                cv_pset(x + w - 1 - ip, y + j, swap16(rv[a]));
            } else {
                cv_pset(x + ip, y + j, c), cv_pset(x + w - 1 - ip, y + j, c);
            }
        }
    }
}

/* the themes (GLOBAL > THEME, perf.theme): the screen, the pills (their text is the screen's colour), the selection, the
 * light tone (dim pills, bar tracks, fills) */
static const uint16_t PF_THEMES[PERF_NTHEME][4] = {
    {0x1216u, 0xFFFFu, 0xFE80u, 0x7D5Fu},               /* BLUE: #1240B0, white, #FFD200, #7FA8FF */
    {0x0000u, 0xFFFFu, 0xFA74u, 0x52AAu},               /* MONOCHROME: black, white, #FF4FA0 (pink), #555555 */
    {0x59C4u, 0xF739u, 0xFFFFu, 0xB44Cu},               /* CREAM: #5A3A22 brown, #F3E6C8 cream, white, #B08A60 */
    {0x38EDu, 0xF651u, 0xFE87u, 0x7951u},               /* PLAYA (the splash's): #3A1C6B dusk, #F2C98A sand, #FFD23F sun, #7A2A8C */
    {0x2104u, 0xEF3Au, 0xFB42u, 0x6B6Du},               /* ROLO (after Roland's panels): #222222, #EDE6D6 cream, #FF6A13 orange, #6E6E6E */
};
#define PF_BG PF_THEMES[perf.theme % PERF_NTHEME][0]
#define PF_PILL PF_THEMES[perf.theme % PERF_NTHEME][1]
#define PF_SEL PF_THEMES[perf.theme % PERF_NTHEME][2]
#define PF_TAG PF_THEMES[perf.theme % PERF_NTHEME][3]
