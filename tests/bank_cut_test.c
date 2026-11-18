/* Bank-voice CUTOFF knob: the EDIT page exposes CUTOFF for the bank engines,
 * so turning it must change their brightness. Classic algo 0 already responds;
 * this test covers the bank algos (1..4) that were hard-coded. */
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
    for (int algo = 0; algo <= 4; algo++) {
        cfg.bass_algo = (uint8_t)algo;
        cfg.perf = 1;
        cfg.bass_cut = 5;   play(b1);
        cfg.bass_cut = 120; play(b2);
        double dark = bright(b1), open = bright(b2);
        int ok = open > dark * 1.3;               /* at least 30% brighter */
        printf("bass algo %d cutoff responds       %s (dark %.0f -> open %.0f)\n",
               algo, ok ? "ok" : "FAIL", dark, open);
        failed += !ok;
    }
    printf(failed ? "BANK CUTOFF FAILED (%d)\n" : "bank cutoff passed\n", failed);
    return failed ? 1 : 0;
}
