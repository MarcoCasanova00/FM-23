#include "host_platform.h"
#include <stdio.h>
static int32_t q[2u * 256u];
int main(void)
{
    host_boot();
    for (int i = 0; i < 10; i++) host_frame();
    cfg.macro = 0; cfg.fx_drive = 0; cfg.fx_delay = 0; cfg.fx_reverb = 0;
    cfg.lfo_on[0] = 0; cfg.analog[0] = 0; cfg.bass_oct = 1; cfg.perf = 1;
    for (int algo = 1; algo <= 4; algo++) {
        cfg.bass_algo = (uint8_t)algo;
        cfg.bass_cut = 5;   fm1_in.notes = 1u << 8; host_render(q, 256); fm1_in.notes = 0;
        printf("algo %d cut=5   svf.f=%d fcur=%d cut=%d\n", algo, bb.svf.f, bb.fcur, bb.cut);
        host_render(q, 16000);
        cfg.bass_cut = 120; fm1_in.notes = 1u << 8; host_render(q, 256); fm1_in.notes = 0;
        printf("algo %d cut=120 svf.f=%d fcur=%d cut=%d\n", algo, bb.svf.f, bb.fcur, bb.cut);
        host_render(q, 16000);
    }
    return 0;
}
