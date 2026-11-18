#include "host_platform.h"
#include <stdio.h>
static int32_t b1[2u * 8192u], b2[2u * 8192u], quiet[2u * 8192u];
static double bright(const int32_t *b)
{
    int64_t s = 0;
    for (uint32_t i = 1; i < 8192u; i++) {
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
    for (int i = 0; i < 10; i++) host_frame();
    cfg.macro = 0; cfg.fx_drive = 0; cfg.fx_delay = 0; cfg.fx_reverb = 0;
    cfg.lfo_on[0] = 0; cfg.analog[0] = 0; cfg.bass_oct = 1;
    for (int algo = 0; algo <= 4; algo++) {
        cfg.bass_algo = (uint8_t)algo;
        cfg.perf = 1;
        cfg.bass_cut = 5;   play(b1);
        cfg.bass_cut = 120; play(b2);
        double a = bright(b1), z = bright(b2);
        printf("bass algo %d: cut=5 bright=%.0f | cut=120 bright=%.0f  %s\n",
               algo, a, z, (z > a * 1.3 || z < a * 0.7) ? "RESPONDS" : "NO EFFECT");
    }
    return 0;
}
