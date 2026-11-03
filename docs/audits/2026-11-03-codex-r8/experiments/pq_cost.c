/* Instruction-level measurement only. Uses repository ISS config, real kernel, production layout. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "iss_rt.h"
#include "pqbn_v4.h"
enum { W=1216, H=684 };
#define NT ((W/32)*(H/4))
int main(void) {
    pqbn_v4_ctx c; memset(&c,0,sizeof c);
    c.W=W;c.H=H;c.ntx=W/32;c.nty=H/4;c.line_mode=1;
    c.fb4=iss_psram_alloc(W*H/2,64);c.hot=iss_psram_alloc(W*H/2,64);c.cold=iss_psram_alloc(W*H*2,64);
    c.tile_dirty=iss_sram_alloc(NT,64);c.tile_plan=iss_sram_alloc(NT*2,64);c.tile_nmax=iss_sram_alloc(NT,64);
    c.band=iss_sram_alloc((H/4)*4,64);c.row_active=iss_sram_alloc(H,64);
    if(!c.fb4||!c.hot||!c.cold||!c.tile_dirty||!c.tile_plan||!c.tile_nmax||!c.band||!c.row_active) return 2;
    const struct {const char *name;uint32_t from,n;int gray;} cases[]={
        {"skip-white-255",20,255,255},{"skip-gray-255",20,255,0x77},
        {"skip-white-cross4096",4000,255,255},{"skip-white-1",1,1,255}
    };
    for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++) {
        memset((void*)c.fb4,cases[i].gray,W*H/2);pqbn_v4_init(&c,cases[i].from);
        iss_cache_cold();iss_begin(cases[i].name);
        int rows=pqbn_v4_skip(&c,cases[i].from,cases[i].from+cases[i].n);
        iss_end2(rows,cases[i].n);
        printf("%s rows=%d model_cycles=%d\n",cases[i].name,rows,rows*43);
    }
    uint8_t *line=iss_sram_alloc(W/4,64);
    iss_cache_cold();iss_begin("executor-idle-row");
    int r=pqbn_v4_exec_row(&c,100,0,line);
    iss_end(1);printf("exec_row=%d\n",r);
    return 0;
}
