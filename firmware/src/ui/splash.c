/* SPDX-License-Identifier: GPL-3.0-only */
/* The Optimist boot screen: the logo, an Optimist dinghy drawn from its geometry (no bitmap), "OPTIMIST" and the
 * build's version.
 *
 * The dinghy is line art: straight strokes (LG_SEGS, their end points in px of the logo's box) and the class
 * emblem on the sail. The emblem is a ring with a bar through its lower half, a small serif at each end of the bar.
 * In units of the ring's outer radius R (measured on the reference artwork, a ring 247 px across): inner radius
 * 0.686 R; the bar 0.288 R wide, centred, from 0.061 R below the centre to 1.36 R below it; the serifs 0.024 R past
 * the bar on each side and 0.028 R tall. Each pixel gets its coverage (Q8), kept in the canvas until the colour
 * pass: a stroke from the pixel's distance to its line and its overlap with the stroke's length, the ring from the
 * distance to the centre, the bar and serifs from their overlap with the pixel.
 */
#define LOGO_R 6                                         /* the emblem's outer radius, px */
#define LG_QX 52                                         /* the emblem's centre in the box, px */
#define LG_QY 34
#define LG_HW 150                                        /* a stroke's half width, Q8 px */
#define LOGO_W 110u
#define LOGO_H 151u
#define LOGO_Y 10u                                       /* screen row of the box's top */
#define LOGO_TEXT_Y (LOGO_Y + LOGO_H + 12u)              /* "OPTIMIST" (FONT_L), the version under it */
#define LG(m) ((int32_t)(LOGO_R * 256 * (m) / 1000))     /* per mille of R -> Q8 px */
#define LG_RO LG(1000)                                   /* the ring's outer and inner radius */
#define LG_RI LG(686)
#define LG_BAR LG(144)                                   /* the bar's half width, the serifs' */
#define LG_SERIF LG(168)
#define LG_SH LG(28)                                     /* the serifs' height */
#define LG_TOP LG(61)                                    /* the bar's ends, below the centre */
#define LG_BOT LG(1360)
#define LOGO_C RGB(232, 232, 236)                        /* the logo: light on the dark screen */

static int32_t lg_clamp(int32_t v) { return v < 0 ? 0 : v > 256 ? 256 : v; }

/* the length of [lo, hi) inside the pixel [p, p + 256), Q8 */
static int32_t lg_span(int32_t lo, int32_t hi, int32_t p)
{
    int32_t a = lo > p ? lo : p, b = hi < p + 256 ? hi : p + 256;
    return b > a ? b - a : 0;
}

/* coverage of the rectangle |x| < hw, y0 <= y < y1 (centred on the ring) for the pixel at corner (px, py) */
static int32_t lg_rect(int32_t hw, int32_t y0, int32_t y1, int32_t px, int32_t py)
{
    return lg_span(-hw, hw, px) * lg_span(y0, y1, py) >> 8;
}

/* coverage of the emblem for the pixel at corner (px, py), Q8 relative to the ring's centre */
static int32_t lg_cover(int32_t px, int32_t py)
{
    int32_t cx = px + 128, cy = py + 128, d2 = cx * cx + cy * cy, a = 0, b;
    if (d2 < (LG_RO + 256) * (LG_RO + 256))             /* the ring: a disc minus a disc, (r^2 - d^2) / 2r ~ r - d */
        a = lg_clamp((LG_RO * LG_RO - d2) / (2 * LG_RO) + 128) - lg_clamp((LG_RI * LG_RI - d2) / (2 * LG_RI) + 128);
    b = lg_rect(LG_BAR, LG_TOP, LG_BOT, px, py);                                     /* the bar */
    a = b > a ? b : a;
    b = lg_rect(LG_SERIF, LG_TOP, LG_TOP + LG_SH, px, py) + lg_rect(LG_SERIF, LG_BOT - LG_SH, LG_BOT, px, py);
    return b > a ? b : a;                                                            /* (the serifs) */
}

/* the dinghy's strokes, x0 y0 x1 y1 in px of the box (an Optimist side on, bow to the left) */
static const uint8_t LG_SEGS[][4] = {
    {15, 49, 15, 113},  {16, 49, 53, 4},    {15, 80, 52, 6},    {53, 4, 89, 102},  /* mast, head, sprit, leech */
    {15, 102, 89, 102}, {70, 50, 58, 54},   {81, 80, 68, 84},                      /* boom, two battens */
    {3, 114, 102, 110}, {3, 114, 7, 122},   {7, 122, 30, 126},  {30, 126, 75, 125}, /* gunwale, the hull */
    {75, 125, 95, 121}, {95, 121, 102, 110},                                         /* (to the transom) */
    {40, 126, 40, 149}, {50, 126, 50, 149}, {40, 149, 50, 149},                      /* daggerboard */
    {96, 111, 95, 131}, {95, 131, 103, 138}, {103, 138, 106, 120}, {106, 120, 102, 110}, /* rudder */
    {75, 113, 100, 111},                                                             /* tiller */
};
#define LG_NSEGS (sizeof LG_SEGS / sizeof LG_SEGS[0])

/* floor(sqrt(v)) */
static uint32_t lg_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    for (; b; b >>= 2)
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
    return r;
}

/* the box's rows in the canvas now (a pass of optimist_logo: the whole box when the canvas holds it, two passes with
 * CANVAS_STRIP's 240 x 62, three of 240 x 31): lg_y0 .. lg_y0 + lg_n - 1 */
static uint32_t lg_y0, lg_n;
/* keep the larger coverage at (x, y) of the box (nothing outside this pass's rows: the canvas ends there) */
static void lg_max(uint32_t x, uint32_t y, int32_t k)
{
    uint16_t *p;
    if (y < lg_y0 || y - lg_y0 >= lg_n || x >= LOGO_W)
        return;
    p = &cv_px[(y - lg_y0) * LOGO_W + x];
    if (k > (int32_t)*p)
        *p = (uint16_t)k;
}

/* one stroke into the canvas's coverage: from its line (the distance, Q8) and its length, its ends a half width on */
static void lg_seg(const uint8_t *s)
{
    int32_t dx = s[2] - s[0], dy = s[3] - s[1], len = (int32_t)lg_isqrt((uint32_t)(dx * dx + dy * dy) << 8); /* Q4 */
    int32_t x0 = (s[0] < s[2] ? s[0] : s[2]) - 2, x1 = (s[0] > s[2] ? s[0] : s[2]) + 2, x, y;
    int32_t y0 = (s[1] < s[3] ? s[1] : s[3]) - 2, y1 = (s[1] > s[3] ? s[1] : s[3]) + 2;
    for (y = y0 < 0 ? 0 : y0; y <= y1 && y < (int32_t)LOGO_H; y++)
        for (x = x0 < 0 ? 0 : x0; x <= x1 && x < (int32_t)LOGO_W; x++) {
            int32_t rx = (x - s[0]) * 256 + 128, ry = (y - s[1]) * 256 + 128;   /* the pixel's centre from the start */
            int32_t d = (rx * dy - ry * dx) * 16 / len, t = (rx * dx + ry * dy) * 16 / len, a, b;
            a = lg_clamp(LG_HW + 128 - (d < 0 ? -d : d));
            b = lg_clamp(t + LG_HW + 128);
            t = lg_clamp(len * 16 - t + LG_HW + 128);
            lg_max((uint32_t)x, (uint32_t)y, a * (b < t ? b : t) >> 8);
        }
}

/* the logo into the canvas (LOGO_W x LOGO_H): the coverage of each part, then its colour; then onto the screen */
static void optimist_logo(uint32_t x0, uint32_t y0)
{
    uint32_t x, y, i, rows = CV_MAX / LOGO_W;           /* (the box's rows a pass: all 151 on the whole canvas) */
    for (lg_y0 = 0; lg_y0 < LOGO_H; lg_y0 += lg_n) {
        lg_n = LOGO_H - lg_y0 < rows ? LOGO_H - lg_y0 : rows;
        cv_begin(LOGO_W, lg_n, C_BLACK);
        for (i = 0; i < LG_NSEGS; i++)
            lg_seg(LG_SEGS[i]);
        for (y = LG_QY - LOGO_R - 1; y <= LG_QY + LOGO_R * 1360 / 1000 + 1; y++)
            for (x = LG_QX - LOGO_R - 1; x <= LG_QX + LOGO_R + 1; x++)
                lg_max(x, y, lg_cover(((int32_t)x - LG_QX) * 256, ((int32_t)y - LG_QY) * 256));
        for (i = 0; i < LOGO_W * lg_n; i++) {
            uint32_t k = cv_px[i], c = LOGO_C;
            if (k)
                cv_px[i] = swap16((((c >> 11) * k >> 8) << 11) | ((((c >> 5) & 63u) * k >> 8) << 5) |
                                  ((c & 31u) * k >> 8));
        }
        cv_blit(x0, y0 + lg_y0);
    }
}

#if FELUCCA_PERF
/* JTS22 PLAYA's boot screen (the user, 2026-11-10: "PLAYA, under it JTS22 powered by Optimist, a retro 80 pixel graphic
 * of a beach umbrella in the beach"): an 80 x 80 picture, each pixel 3 x 3 (a sunset sky in bands, a striped sun, the
 * sea, the sand, a striped umbrella, its shadow, a towel), the names over the sky in the performance face (perf_aa.c) */
#define PL_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
static uint16_t playa_px(int32_t x, int32_t y)
{
    static const uint16_t SKY[5] = {PL_RGB(27, 16, 53), PL_RGB(58, 28, 107), PL_RGB(122, 42, 140), PL_RGB(217, 70, 122),
                                    PL_RGB(255, 122, 79)};
    int32_t dx = x - 57, dy = y - 40, sx = x - 32, sy = y - 44;
    if (y >= 24 && y <= 40 && dx * dx * 10 + dy * dy * 26 <= 18 * 18 * 10 && !(y == 40 && (x & 3) == 1)) {
        int32_t w = (dx + 18) * 6 / 37;                    /* the canopy: six wedges, red and white */
        return (w & 1) ? PL_RGB(255, 243, 224) : PL_RGB(230, 57, 70);
    }
    if ((x == 56 || x == 57) && y >= 25 && y <= 72)
        return PL_RGB(107, 66, 38);                        /* the pole (no tip: the names above it) */
    if (y < 45) {
        if (sx * sx + sy * sy <= 16 * 16 && y >= 28 && !(y >= 34 && (y - 34) % 3 == (y > 40 ? 0 : 2)))
            return y < 36 ? PL_RGB(255, 210, 63) : PL_RGB(255, 140, 66);   /* the sun, cut by the 80s' stripes */
        return SKY[y < 12 ? 0 : y < 22 ? 1 : y < 31 ? 2 : y < 38 ? 3 : 4];
    }
    if (y < 55)                                            /* the sea, its glints */
        return ((y == 47 && (x % 9) < 3) || (y == 50 && ((x + 4) % 11) < 4) || (y == 53 && ((x + 7) % 13) < 5))
               ? PL_RGB(79, 163, 199) : PL_RGB(27, 58, 107);
    if ((x - 50) * (x - 50) + (y - 73) * (y - 73) * 9 <= 14 * 14)
        return PL_RGB(217, 167, 102);                      /* the shadow */
    if (x >= 14 && x <= 32 && y >= 63 && y <= 70)
        return ((x - 14) / 3) & 1 ? PL_RGB(255, 243, 224) : PL_RGB(18, 64, 176);   /* the towel */
    return (x * 7 + y * 3) % 11 == 0 ? PL_RGB(217, 167, 102) : PL_RGB(242, 201, 138);   /* the sand */
}
static void playa_paint(void)
{
    int32_t y, x;
    for (y = 0; y < 240; y++) {
        if (y + cv_oy < 0 || y + cv_oy >= (int32_t)cv_h)
            continue;
        for (x = 0; x < 80; x++)
            cv_rect(x * 3, y, 3, 1, playa_px(x, y / 3));
    }
    aa_text(120 - aa_w(&AF_PL, "PLAYA") / 2 + 2, 6, &AF_PL, "PLAYA", PL_RGB(27, 16, 53), PL_RGB(27, 16, 53));
    aa_text(120 - aa_w(&AF_PL, "PLAYA") / 2, 4, &AF_PL, "PLAYA", C_WHITE, PL_RGB(27, 16, 53));
    aa_text(120 - aa_w(&AF_PS, "JTS22 powered by Optimist") / 2, 58, &AF_PS, "JTS22 powered by Optimist",
            PL_RGB(255, 210, 63), PL_RGB(58, 28, 107));
}
#endif
/* the boot screen: the logo, the name in the theme's colour, the version from the build (VERSION) */
static void boot_splash(void)
{
#if FELUCCA_PERF
    cv_tall(0, 240, C_BLACK, playa_paint);
    return;
#endif
    const char *v = FELUCCA_VERSION, *p = "OPTIMIST ";
    uint32_t i = 0, r0, n;
    while (p[i] && v[i] == p[i])
        i++;
    if (!p[i])
        v += i;                                          /* "OPTIMIST 0.3" -> "0.3" (a release says "0.3 BETA") */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    optimist_logo((240u - LOGO_W) / 2u, LOGO_Y);
    for (r0 = 0; r0 < 56u; r0 += n) {                  /* the name, the version: 56 rows, in passes on a smaller canvas */
        n = 56u - r0 < CV_MAX / 240u ? 56u - r0 : CV_MAX / 240u;
        cv_begin(240, n, C_BLACK);
        cv_oy = -(int32_t)r0;
        cv_text(120 - text_w(&FONT_L, "OPTIMIST") / 2, 0, &FONT_L, "OPTIMIST", C_HI);
        cv_text(120 - text_w(&FONT_S, v) / 2, 40, &FONT_S, v, C_AMB);
        cv_oy = 0;
        cv_blit(0, LOGO_TEXT_Y + r0);
    }
}
