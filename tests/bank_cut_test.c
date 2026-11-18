/* Bank-voice CUTOFF knob: the EDIT page exposes CUTOFF for every engine, so
 * turning it must change each bank algo's filter. Classic algo 0 used the
 * knob already; the bank recipes (1-4) had hard-coded SVF cutoffs. Check the
 * actual filter coefficient set at note-on, plus a brightness proxy for the
 * classic voice. */
#include "host_platform.h"
#include <stdio.h>

static int32_t b1[2u * 8192u], b2[2u * 8192u], quiet[2u * 8192u];
static double bright(const int32_t *b)
{
    int64_t s = 0;
    uint32_t i;
    for (i = 1; i < 8192u; i++) {
        int32_t d = b[2u * i] - b[2u * (i - 1u)];
        s += d < 0 ? -d : d;
    }
    return (double)s / 8192.0;
}
static void play(int32_t *out)
{
    fm1_in.notes = 1u << 8;
    host_render(out, 8192);
    fm1_in.notes = 0;
    host_render(quiet, 8192);
}

int main(void)
{
    host_boot();
    { int i; for (i = 0; i < 10; i++) host_frame(); }
    cfg.macro = 0; cfg.fx_drive = 0; cfg.fx_delay = 0; cfg.fx_reverb = 0;
    cfg.lfo_on[0] = 0; cfg.analog[0] = 0; cfg.bass_oct = 1;
    int failed = 0;

    /* classic voice: brightness proxy */
    cfg.bass_algo = 0; cfg.perf = 1;
    cfg.bass_cut = 5;   play(b1);
    cfg.bass_cut = 120; play(b2);
    {
        int ok = bright(b2) > bright(b1) * 1.3;
        printf("bass algo 0 cutoff (brightness)         %s\n", ok ? "ok" : "FAIL");
        failed += !ok;
    }

    /* bank voices: check the cutoff state each algo actually renders with.
     * algos 1-2 use the SVF coefficient; algo 3 uses bb.cut (decays per sample),
     * algo 4 uses bb.fcur (its filter envelope start). */
    for (int algo = 1; algo <= 4; algo++) {
        cfg.bass_algo = (uint8_t)algo;
        cfg.bass_cut = 5;   bank_bass_note(38, 105);
        int dark = algo == 3 ? bb.cut : algo == 4 ? bb.fcur : bb.svf.f;
        cfg.bass_cut = 120; bank_bass_note(38, 105);
        int open = algo == 3 ? bb.cut : algo == 4 ? bb.fcur : bb.svf.f;
        int ok = open > dark * 1.5 && dark > 0;
        printf("bass algo %d cutoff (%d -> %d)          %s\n",
               algo, dark, open, ok ? "ok" : "FAIL");
        failed += !ok;
    }
    printf(failed ? "BANK CUTOFF FAILED (%d)\n" : "bank cutoff passed\n", failed);
    return failed ? 1 : 0;
}
