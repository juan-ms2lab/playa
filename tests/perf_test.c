/* SPDX-License-Identifier: GPL-3.0-only */
/* JTS22 PLAYA, a performance build for the M-VAVE FM-1: created by JFS with help from Claude (Anthropic), 2026.
 * Built on Optimist by w0ts and contributors (github.com/w0ts/optimist), GPL-3.0-only; this file is part of that work
 * and under the same licence (LICENSE). */
/* PERF (JTS22, io/midi/perf.c, midi_control.c): layers A / B on one channel, the split's gains (LAYER, HARD, BLEND,
 * REV, width 0), which layer a note plays, the velocity kept, and note ownership: a note off ends exactly the voices its
 * note on started, whatever the split did since; repeated notes, the pedal, All Notes Off. Live drums on channel 10;
 * a controller's knobs (CC 70..76) as the panel's encoders.
 * Built with -DFELUCCA_PERF=1 (tests/run_tests.sh). Exit status: the number of failed checks. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i)
{
    static const uint8_t E[NPART] = {0, 1, 3};
    return i < NPART ? E[i] : 0u;
}
#include "../firmware/src/storage/project.c"

static int fails;
static void check(int ok, const char *what)
{
    printf("perf: %-86s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static uint32_t blk;
static void blocks(uint32_t n)
{
    int32_t out[CTL * 2];
    while (n--) {
        mix_block(out, CTL);
        mo_r = mo_w;
        blk++;
    }
}
static void send(uint32_t status, uint32_t note, uint32_t vel)
{
    midi_in_q[mi_w++ % MQ] = (status >> 4) | status << 8 | note << 16 | vel << 24;
    blocks(1);
}
static int gated(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return 1;
    return 0;
}
static uint32_t ngated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate;
    return n;
}
static void reset(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(midi_ch, 0, sizeof midi_ch);
    memset(midi_held, 0, sizeof midi_held);
    memset(kb_kind, 0, sizeof kb_kind);
    memset(&um, 0, sizeof um);
    memset(&drums, 0, sizeof drums);
    drums_set = -2;
    midi_nheld = 0;
    host_tracks_init();
    bps_defaults();
    fm1_in.notes = fm1_in.buttons = kb_prev = 0;
    mi_w = mi_r = 0;
    mo_w = mo_r = 0;
    usb.config = 1;
    panic_req = transport_req = 0;
    song.playing = 0;
    song.sel = 0;
    for (i = 0; i < NPART; i++) {
        host_preset(&trk[i], 0, 1);
        trk[i].p[P_VOICE] = V_POLY;
        trk[i].p[P_AMODE] = trk[i].p[P_AHOLD] = 0;
        trk[i].p[P_SUS] = 127;
        trk[i].p[P_TRANS] = 0;
    }
    song.g[G_BPM] = 120;
    ly_bit[LY_ROLL] = 1u << 8;
    mo_r = mo_w;
}

static uint32_t xg_of(const track_t *t, uint32_t note)   /* the gated voice's gain, 0 = none */
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return t->v[i].xg;
    return 0;
}
static uint32_t vel_of(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return t->v[i].vel;
    return 0;
}
static void set(uint32_t mode, uint32_t split, uint32_t width, uint32_t rev)
{
    perf.mode = (uint8_t)mode, perf.split = (uint8_t)split, perf.width = (uint8_t)width, perf.rev = (uint8_t)rev;
}
static void t_gains(void)
{
    uint32_t n, ok = 1;
    set(PERF_LAYER, 60, 12, 0);
    check(perf_gain(0, 20) == 32768u && perf_gain(1, 100) == 32768u, "LAYER: both layers, full gain");
    set(PERF_HARD, 60, 12, 0);
    check(perf_gain(0, 59) && !perf_gain(1, 59) && !perf_gain(0, 60) && perf_gain(1, 60), "HARD: A below the split, B from it");
    set(PERF_HARD, 60, 12, 1);
    check(!perf_gain(0, 59) && perf_gain(1, 59) && perf_gain(0, 60) && !perf_gain(1, 60), "HARD REV: the other way round");
    set(PERF_BLEND, 60, 0, 0);
    check(perf_gain(0, 59) == 32768u && !perf_gain(1, 59) && !perf_gain(0, 60), "BLEND width 0 is HARD");
    set(PERF_BLEND, 60, 12, 0);
    check(perf_gain(0, 54) == 32768u && !perf_gain(1, 54) && !perf_gain(0, 66) && perf_gain(1, 66) == 32768u,
          "BLEND: A alone at split - width/2, B alone at split + width/2");
    check(perf_gain(0, 60) == 23170u && perf_gain(1, 60) == 23170u, "BLEND: -3 dB each at the split");
    for (n = 0; n < 128u; n++) {
        double a = perf_gain(0, n) / 32768.0, b = perf_gain(1, n) / 32768.0;
        ok &= fabs(a * a + b * b - 1.0) < 0.002;
    }
    check(ok, "BLEND: equal power on every note");
    set(PERF_BLEND, 60, 12, 1);
    check(perf_gain(1, 54) == 32768u && !perf_gain(0, 54), "BLEND REV: B below");
    set(PERF_SINGLE, 60, 12, 0);
    check(perf_gain(0, 20) == 32768u && perf_gain(0, 100) == 32768u && !perf_gain(1, 20) && !perf_gain(1, 100),
          "SINGLE: A alone, the whole keyboard");
}
static void t_route(void)
{
    reset();
    set(PERF_BLEND, 60, 12, 0);
    check(trk_midi_ch(0) == 0u && trk_midi_ch(1) == 0u && trk_midi_ch(TRK_DRUM) == 9u,
          "defaults: layers A and B on channel 1, the drums on 10 (GM)");
    send(0x90, 60, 100);
    check(xg_of(&trk[0], 60) == 23170u && xg_of(&trk[1], 60) == 23170u && !gated(&trk[2], 60), "a note in the window plays both layers");
    check(vel_of(&trk[0], 60) == 100u && vel_of(&trk[1], 60) == 100u, "the velocity is the note's on both");
    send(0x90, 40, 90);
    check(gated(&trk[0], 40) && !gated(&trk[1], 40) && xg_of(&trk[0], 40) == 32768u, "below the window: A alone, full gain");
    send(0x90, 80, 90);
    check(!gated(&trk[0], 80) && gated(&trk[1], 80), "above the window: B alone");
    send(0x80, 60, 0);
    check(!gated(&trk[0], 60) && !gated(&trk[1], 60), "its note off ends both");
    send(0x80, 40, 0);
    send(0x80, 80, 0);
    check(ngated(&trk[0]) + ngated(&trk[1]) == 0u && midi_nheld == 0u, "every note off: nothing held");
    check(midi_track_idx(9) == TRK_DRUM, "channel 10 plays the drum track");
    set(PERF_SINGLE, 60, 12, 0);
    send(0x90, 80, 100);
    check(gated(&trk[0], 80) && !gated(&trk[1], 80), "SINGLE: a note plays layer A only");
    send(0x80, 80, 0);
    check(!ngated(&trk[0]) && midi_nheld == 0u, "... and its note off ends it");
}
static void t_own(void)
{
    reset();
    set(PERF_HARD, 60, 0, 0);
    send(0x90, 50, 100);                                 /* A's */
    set(PERF_HARD, 30, 0, 0);                            /* 50 is B's now */
    send(0x80, 50, 0);
    check(!gated(&trk[0], 50) && !gated(&trk[1], 50) && midi_nheld == 0u, "split moved while held: the note off ends A's voice");
    set(PERF_LAYER, 60, 12, 0);
    send(0x90, 62, 100);
    trk[0].p[P_TRANS] = 7;                                /* (transposed while held) */
    send(0x80, 62, 0);
    trk[0].p[P_TRANS] = 0;
    check(ngated(&trk[0]) + ngated(&trk[1]) == 0u, "transposed while held: the note off still ends it");
    send(0x90, 64, 100);
    send(0x90, 64, 110);
    check(ngated(&trk[0]) == 1u && ngated(&trk[1]) == 1u && midi_nheld == 2u, "a repeated note: one voice a layer, one press a layer");
    send(0x80, 64, 0);
    check(ngated(&trk[0]) + ngated(&trk[1]) == 0u && midi_nheld == 0u, "its one note off ends both");
    send(0xB0, 64, 127);                                  /* pedal down */
    send(0x90, 65, 100);
    send(0x80, 65, 0);
    check(gated(&trk[0], 65) && gated(&trk[1], 65), "the pedal holds both layers");
    send(0x90, 65, 100);
    send(0x80, 65, 0);
    check(ngated(&trk[0]) == 1u && ngated(&trk[1]) == 1u, "replayed under the pedal: still one voice a layer");
    send(0xB0, 64, 0);
    check(ngated(&trk[0]) + ngated(&trk[1]) == 0u && midi_nheld == 0u, "pedal up: both end");
    send(0x90, 67, 100);
    send(0x90, 69, 100);
    send(0xB0, 123, 0);
    check(ngated(&trk[0]) + ngated(&trk[1]) == 0u && midi_nheld == 0u, "All Notes Off ends every layer's notes");
}
static void t_knobs(void)
{
    uint32_t r;
    reset();
    for (r = 0; r < 7u; r++)
        (void)perf_enc_take(r);
    send(0xB0, 70, 64);
    check(perf_enc_take(3) == 0, "CC 70's first value: no step (it only sets the knob)");
    send(0xB0, 70, 70);
    send(0xB0, 70, 67);
    check(perf_enc_take(3) == 3, "CC 70 64 -> 70 -> 67: KNOB 1 +3");
    send(0xB0, 74, 40);
    send(0xB0, 74, 46);
    check(perf_enc_take(0) == 1, "CC 74 +6: SELECT +1 (a step every 4 values)");
    send(0xB0, 74, 48);
    check(perf_enc_take(0) == 1, "... +2 more: the rest kept, SELECT +1");
    send(0xB1, 75, 100);
    send(0xB1, 75, 92);
    check(perf_enc_take(2) == -2, "CC 75 on channel 2, -8: PRESETS -2");
    send(0xB0, 76, 10);
    send(0xB0, 76, 14);
    check(perf_enc_take(1) == 1, "CC 76: ALGORITHM");
    send(0xB0, 73, 0);
    send(0xB0, 73, 127);
    check(perf_enc_take(6) == 127 && !ngated(&trk[0]), "CC 73: KNOB 4, the whole turn; no note, no other effect");
    send(0xB0, 77, 40);
    send(0xB0, 77, 45);
    check(perf_enc_take(PERF_LEVEL) == 5, "CC 77 (K8): the selected track's LEVEL, a step a value");
    perf.cc[0] = 20;                                     /* (GLOBAL > CC 1-4: KNOB 1 on CC 20) */
    send(0xB0, 20, 10);
    send(0xB0, 20, 13);
    send(0xB0, 70, 0);
    send(0xB0, 70, 50);
    check(perf_enc_take(3) == 3, "KNOB 1 moved to CC 20: CC 20 turns it, CC 70 no longer");
    perf.cc[1] = 0;
    send(0xB0, 71, 0);
    send(0xB0, 71, 60);
    check(perf_enc_take(4) == 0, "a knob's CC OFF (0): nothing");
    perf.cc[0] = 70, perf.cc[1] = 71;
}
static void t_budget(void)
{
    uint32_t n;
    reset();
    set(PERF_LAYER, 60, 12, 0);
    for (n = 0; n < 6u; n++)
        send(0x90, 60 + 2 * n, 100);
    check(ngated(&trk[0]) == 6u && ngated(&trk[1]) == 6u, "LAYER, a 6-note chord: all 12 voices (the shared budget is 12, not 8)");
    for (n = 0; n < 6u; n++)
        send(0x80, 60 + 2 * n, 0);
    blocks(400);                                         /* (the release tails out) */
    for (n = 0; n < 4u; n++)
        trk_note_on(&trk[2], 40 + n, 90);                /* the song's part (track 3): 4 held voices */
    blocks(1);
    for (n = 0; n < 4u; n++)
        send(0x90, 60 + 2 * n, 100);                     /* the layers ask for 8: 12 in all, the budget */
    send(0x90, 70, 100);                                 /* 2 more: past it */
    check(ngated(&trk[0]) == 5u && ngated(&trk[1]) == 5u && ngated(&trk[2]) == 2u,
          "the budget full: the live layers take the song's voices, never each other's");
}
static void t_store(void)
{
    uint8_t rec[FXR_MAX + 8u];
    uint32_t n, i;
    reset();
    set(PERF_HARD, 48, 7, 1);
    n = fxr_encode(rec);
    set(PERF_LAYER, 0, 0, 0);
    set(PERF_SINGLE, 48, 7, 1);
    n = fxr_encode(rec);
    set(PERF_LAYER, 0, 0, 0);
    check(fxr_decode(rec, n, 1) && perf.mode == PERF_SINGLE, "SINGLE round-trips through the project");
    perf.cc[3] = 99;
    n = fxr_encode(rec);
    perf.cc[3] = 73;
    check(fxr_decode(rec, n, 1) && perf.cc[3] == 99, "the knobs' CCs round-trip through the project");
    perf.cc[3] = 73;
    set(PERF_HARD, 48, 7, 1);
    n = fxr_encode(rec);
    set(PERF_LAYER, 0, 0, 0);
    check(fxr_decode(rec, n, 1) && perf.mode == PERF_HARD && perf.split == 48 && perf.width == 7 && perf.rev == 1,
          "the split round-trips through the project's FX record");
    for (i = FXR_HEAD; i + 1u < n && rec[i] != PERF_TLV; i += 2u + rec[i + 1u])
        ;
    rec[i + 2u] = 9;                                     /* (a damaged MODE) */
    fxr_decode(rec, n, 1);
    check(perf.mode == PERF_BLEND && perf.split == 60 && perf.width == 12 && !perf.rev, "a damaged value: the default split");
    set(PERF_HARD, 48, 7, 1);
    fxr_decode(rec, FXR_HEAD, 1);                        /* (a record of before: no PERF TLV) */
    check(perf.mode == PERF_BLEND && perf.split == 60, "a project from before: the default split");
}
static void t_keys(void)
{
    uint32_t k;
    reset();
    set(PERF_LAYER, 60, 12, 0);
    song.sel = 0;
    key_down(7);
    blocks(1);
    check(ngated(&trk[0]) == 1u && ngated(&trk[1]) == 1u, "a panel key on layer A plays both layers (LAYER)");
    key_up(7);
    blocks(1);
    check(ngated(&trk[0]) + ngated(&trk[1]) == 0u && !kb_lay, "its release ends both");
    set(PERF_HARD, 127, 0, 0);
    song.sel = 1;
    for (k = 0; k < 27u; k += 6u)
        key_down(k);
    blocks(1);
    check(ngated(&trk[0]) == 5u && !ngated(&trk[1]), "on layer B's row, HARD with the split at the top: the keys play A (the split decides, not the row)");
    for (k = 0; k < 27u; k += 6u)
        key_up(k);
    blocks(1);
    check(ngated(&trk[0]) + ngated(&trk[1]) == 0u && midi_nheld == 0u, "every key up: nothing held");
    song.sel = 2;
    key_down(7);
    blocks(1);
    check(ngated(&trk[2]) == 1u && !ngated(&trk[0]) && !ngated(&trk[1]), "track 3 selected: its own key path, no layers");
    key_up(7);
}
int main(void)
{
    t_keys();
    t_store();
    t_budget();
    t_knobs();
    t_gains();
    t_route();
    t_own();
    printf("perf: %d failed\n", fails);
    return fails;
}
