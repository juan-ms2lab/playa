/* SPDX-License-Identifier: GPL-3.0-only */
/* JTS22 PLAYA, a performance build for the M-VAVE FM-1: created by JFS with help from Claude (Anthropic), 2026.
 * Built on Optimist by w0ts and contributors (github.com/w0ts/optimist), GPL-3.0-only; this file is part of that work
 * and under the same licence (LICENSE). */
/* PERF (JTS22 performance, builder item PERF): parts 1 and 2 are layers A and B. When both have the same MIDI channel,
 * a note on it plays on both, through the split (midi_control.c midi_note_on):
 *   LAYER  both, at full gain
 *   HARD   A below the split note, B at or above it (REV: the other way round)
 *   BLEND  both inside the window split - width/2 .. split + width/2, an equal-power crossfade:
 *          t = (note - (split - width/2)) / width, A = cos(t pi/2), B = sin(t pi/2); width 0 is HARD
 *   SINGLE A alone, over the whole keyboard (the user, 2026-11-10: "a single mode where there is only one layer")
 * The gain is the voice's (voice_t.xg, applied to its amp1 in track_render: ramped, no click); the velocity is the
 * note's own. Integer only: a quarter sine of 33 points, interpolated (no trigonometry in the audio interrupt). */
enum { PERF_LAYER, PERF_HARD, PERF_BLEND, PERF_SINGLE, PERF_NMODE };
typedef struct {
    uint8_t mode;                       /* PERF_* */
    uint8_t split;                      /* 0..127, the first note of B's side (normal direction) */
    uint8_t width;                      /* 0..24 semitones, the whole blend window */
    uint8_t rev;                        /* 1: B below, A above */
    uint8_t theme;                      /* the pages' colours (ui/optimist/op_perfdraw.c PF_THEMES): 0 blue, 1 monochrome, 2 cream, 3 playa, 4 rolo */
    uint8_t cc[8];                      /* the controller knobs' CCs, K1..K8 (perf_cc), 0 = off; GLOBAL > CC 1-4, CC 5-8 */
} perf_t;
#define PERF_DEFAULT {PERF_BLEND, 60, 12, 0, 3, {70, 71, 72, 73, 74, 75, 76, 77}}   /* (theme 3: PLAYA, the default: the user, 2026-11-10) */   /* (the CCs: an MPK mini 3's K1..K8) */
#define PERF_NTHEME 5u  /* (spec: C4, a 12-semitone blend, A low / B high) */
static perf_t perf = PERF_DEFAULT;
static uint16_t perf_xg = 32768u;       /* the gain the next voice_start takes (unity outside a layered press) */
/* the keys just play (the user, 2026-11-10: on PERFORM "keyboard lights off and just play"): 1 off SEQ (the UI sets it each
 * frame, op_perfdraw.c); the drum track's white keys then play lanes too, no step entry, no lane picked, no key lit */
static volatile uint8_t perf_play_keys = 1;

static const uint16_t PERF_QSIN[33] = {
    0, 1608, 3212, 4808, 6393, 7962, 9512, 11039, 12540, 14010, 15447, 16846, 18205, 19520, 20788, 22006, 23170,
    24279, 25330, 26320, 27246, 28106, 28899, 29622, 30274, 30853, 31357, 31786, 32138, 32413, 32610, 32729, 32768};

/* sin(x pi/2), x in Q13 (0..8192) -> Q15 */
static uint32_t perf_qsin(uint32_t x)
{
    uint32_t i = x >> 8, f = x & 255u;
    if (i >= 32u)
        return 32768u;
    return PERF_QSIN[i] + (((PERF_QSIN[i + 1u] - PERF_QSIN[i]) * f) >> 8);
}

/* layer 0 (A) or 1 (B): its gain for note, Q15; 0 = it does not play the note */
static uint32_t perf_gain(uint32_t layer, uint32_t note)
{
    int32_t num, den = 2 * (int32_t)perf.width, x;
    uint32_t b_side;
    if (perf.mode == PERF_LAYER)
        return 32768u;
    if (perf.mode == PERF_SINGLE)
        return layer ? 0u : 32768u;
    if (perf.mode == PERF_HARD || !perf.width) {
        b_side = note >= perf.split;
        return (b_side ^ perf.rev) == layer ? 32768u : 0u;
    }
    num = 2 * (int32_t)note - 2 * (int32_t)perf.split + (int32_t)perf.width;   /* t = num / den */
    x = num <= 0 ? 0 : num >= den ? 8192 : num * 8192 / den;
    if (perf.rev)
        x = 8192 - x;
    return layer ? perf_qsin((uint32_t)x) : perf_qsin(8192u - (uint32_t)x);   /* B = sin, A = cos */
}

/* A controller's knobs as the panel's encoders (the user, 2026-11-10: an MPK mini's K1..K8; "can you edit the cc for
 * knobs 1-8"): the CC of knob k is perf.cc[k] (any channel; 0 off; GLOBAL > CC 1-4, CC 5-8), absolute 0..127, made
 * encoder steps by the change from the knob's last value (its first move only sets it): K1..K4 KNOB 1..4 and K8 the
 * selected track's LEVEL, a step a value; K5 SELECT, K6 PRESETS, K7 ALGORITHM, a step every 4 values (lists).
 * The audio interrupt only adds (perf_enc_in), the main loop only reads (panel_enc, op_perf.c pf_enc): no lock.
 * Roles: ui/panel.c EN_*, PERF_LEVEL past them. */
#define PERF_NCC 8u
#define PERF_LEVEL 7u
static const uint8_t PERF_CC_ROLE[PERF_NCC] = {3, 4, 5, 6, 0, 2, 1, PERF_LEVEL};   /* EN_K1..EN_K4, EN_SELECT, EN_PRESET, EN_ALGO */
static uint8_t perf_cc_last[PERF_NCC] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static int8_t perf_cc_rest[PERF_NCC];
static uint8_t perf_cc_was[PERF_NCC];                   /* the CC each knob's last value came on (a CC changed: forgotten) */
static volatile int16_t perf_enc_in[8];
static int16_t perf_enc_seen[8];

/* cc's value v: 1 = taken (a knob), 0 = not one of them */
static int perf_cc(uint32_t cc, uint32_t v)
{
    uint32_t k;
    int32_t d, q;
    for (k = 0; k < PERF_NCC && (!perf.cc[k] || perf.cc[k] != cc); k++)
        ;
    if (k >= PERF_NCC)
        return 0;
    if (perf_cc_was[k] != cc) {                         /* (another CC than its last: a fresh start, no jump) */
        perf_cc_was[k] = (uint8_t)cc;
        perf_cc_last[k] = 0xFFu;
        perf_cc_rest[k] = 0;
    }
    if (perf_cc_last[k] != 0xFFu) {
        d = (int32_t)v - perf_cc_last[k] + perf_cc_rest[k];
        q = k < 4u || k == PERF_LEVEL ? d : d / 4;   /* (the lists: a step every 4 values, the rest kept) */
        perf_cc_rest[k] = (int8_t)(d - (k < 4u || k == PERF_LEVEL ? d : q * 4));
        perf_enc_in[PERF_CC_ROLE[k]] = (int16_t)(perf_enc_in[PERF_CC_ROLE[k]] + q);
    }
    perf_cc_last[k] = (uint8_t)v;
    return 1;
}

/* the steps of role since the last call (main loop) */
static int32_t perf_enc_take(uint32_t role)
{
    int16_t now = perf_enc_in[role], d = (int16_t)(now - perf_enc_seen[role]);
    perf_enc_seen[role] = now;
    return d;
}

/* ---- the project's FX record (fx/fx_rec.c): PERF_TLV, 4 bytes MODE SPLIT WIDTH REV; none (or a damaged value) reads as
 * the defaults, so a project from before, or from a build without PERF, plays as the spec's default performance; a 5th
 * byte THEME (none: PLAYA), then the eight knobs' CCs (none: 70..77) */
#define PERF_TLV 0x4Fu
#define PERF_TLV_N 13u
static uint32_t perf_tlv(uint8_t *o)
{
    uint32_t k;
    o[0] = perf.mode, o[1] = perf.split, o[2] = perf.width, o[3] = perf.rev, o[4] = perf.theme;
    for (k = 0; k < 8u; k++)
        o[5u + k] = perf.cc[k];
    return PERF_TLV_N;
}
static void perf_untlv(const uint8_t *a, uint32_t n)
{
    static const perf_t D = PERF_DEFAULT;
    perf = D;
    if (n >= 4u && a[0] < PERF_NMODE && a[1] <= 127u && a[2] <= 24u && a[3] <= 1u)
        perf.mode = a[0], perf.split = a[1], perf.width = a[2], perf.rev = a[3];
    if (n >= 5u && a[4] < PERF_NTHEME)
        perf.theme = a[4];
    if (n >= 13u) {
        uint32_t k;
        for (k = 0; k < 8u; k++)
            perf.cc[k] = a[5u + k] <= 127u ? a[5u + k] : perf.cc[k];
    }
}
