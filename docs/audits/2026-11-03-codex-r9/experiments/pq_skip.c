/* Audit-only differential test of the public skip contract; no product edits. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pqbn_v4.h"

static void *zalloc(size_t n) { void *p = calloc(1,n); if(!p) abort(); return p; }
static void setup(pqbn_v4_ctx *c, int h, uint32_t now) {
    memset(c,0,sizeof *c);
    c->W=64; c->H=h; c->ntx=2; c->nty=h/4; c->marg=2; c->plan_budget=8000; c->line_mode=1;
    c->dec=zalloc(16384); c->exk=zalloc(1536); c->exk_m=zalloc(1792); c->tau=zalloc(4096); c->tau_nib=zalloc(2048);
    if(pqbn_v4_build((uint8_t*)c->dec,(uint8_t*)c->exk,c->step,7.6f,0.3f,120.f)) abort();
    for(int i=0;i<4096;i++) ((uint8_t*)c->tau)[i]=(uint8_t)((((i&63)*37)+((i>>6)*101))&15);
    pqbn_v4_build_motion((uint8_t*)c->exk_m,(uint8_t*)c->tau_nib,c->tau);
    size_t px=(size_t)c->W*h, nt=(size_t)c->ntx*c->nty;
    c->fb4=zalloc(px/2); memset((void*)c->fb4,255,px/2);
    c->hot=zalloc(px/2); c->cold=zalloc(px*2); c->tile_dirty=zalloc(nt); c->tile_plan=zalloc(nt*2);
    c->tile_nmax=zalloc(nt); c->band=zalloc((size_t)c->nty*4); c->row_active=zalloc(h);
    pqbn_v4_init(c,now);
}
static void destroy(pqbn_v4_ctx *c) {
    free((void*)c->dec); free((void*)c->exk); free((void*)c->exk_m); free((void*)c->tau); free((void*)c->tau_nib);
    free((void*)c->fb4); free(c->hot); free(c->cold); free(c->tile_dirty); free(c->tile_plan);
    free(c->tile_nmax); free(c->band); free(c->row_active);
}
static int differences(const void *a,const void *b,size_t n) {
    const uint8_t *x=a,*y=b; int d=0; for(size_t i=0;i<n;i++) d+=x[i]!=y[i]; return d;
}
static int young(const pqbn_v4_ctx *c,uint32_t now) {
    int n=0; for(int i=0;i<c->W*c->H;i++) {
        uint16_t s=c->cold[i]; uint32_t ts=(s&255)|((s>>5)&0x300);
        n+=((now-ts)&1023)<64;
    } return n;
}
static int one(int h,uint32_t from,uint32_t n) {
    pqbn_v4_ctx a,b; setup(&a,h,from-40);setup(&b,h,from-40);
    for(uint32_t i=0;i<40;i++) {pqbn_v4_phase(&a,from-39+i);pqbn_v4_phase(&b,from-39+i);}
    int expected_rows=0; uint32_t limited=n>512?512:n;
    for(uint32_t i=1;i<=n;i++) pqbn_v4_phase(&a,from+i);
    for(uint32_t i=0;i<limited;i++) {
        unsigned p=(from+n-limited+1+i)&511;
        expected_rows+=(int)((p+1)*h/512-p*h/512);
    }
    int rows=pqbn_v4_skip(&b,from,from+n);
    int cold=differences(a.cold,b.cold,(size_t)a.W*h*2);
    int hot=differences(a.hot,b.hot,(size_t)a.W*h/2);
    int plan=differences(a.tile_plan,b.tile_plan,(size_t)a.ntx*a.nty*2);
    int band=differences(a.band,b.band,(size_t)a.nty*4);
    printf("case H=%d from=%u n=%u cold_bytes=%d hot_bytes=%d plan_bytes=%d band_bytes=%d young_ref=%d young_skip=%d rows=%d actual_rows=%d\n",
           h,from,n,cold,hot,plan,band,young(&a,from+n),young(&b,from+n),rows,expected_rows);
    /* Inspect the first falsely-young tile's next plan, not just the final pixel count. */
    int y0=0;
    for(int y=0;y<h;y++) {
        uint16_t s=b.cold[y*64]; unsigned ts=(s&255)|((s>>5)&0x300);
        if(((from+n-ts)&1023)<64) {y0=(y/4)*4;break;}
    }
    memset((uint8_t*)a.fb4+y0*32,0x77,64*4/2); memset((uint8_t*)b.fb4+y0*32,0x77,64*4/2);
    int ti=(y0/4)*2; a.tile_dirty[ti]=a.tile_dirty[ti+1]=b.tile_dirty[ti]=b.tile_dirty[ti+1]=1;
    pqbn_v4_phase(&a,from+n+1); pqbn_v4_phase(&b,from+n+1);
    printf("next_plan H=%d from=%u n=%u y=%d hot_diff=%d cold_diff=%d\n",h,from,n,y0,
           differences(a.hot+y0*32,b.hot+y0*32,64*4/2),differences(a.cold+y0*64,b.cold+y0*64,64*4*2));
    destroy(&a);destroy(&b);
    return cold||hot||plan||band||rows!=expected_rows;
}
int main(void) {
    const struct {int h;uint32_t from,n;} cases[]={
        {512,20,0},{512,20,1},{512,20,255},{512,4000,255},{512,8170,255},
        {512,20,512},{512,20,513},{512,20,768},{512,20,1024},{512,20,2048},
        {512,20,4096},{512,4000,4500},{512,4000,9000},{512,20,8172},{512,65500,255},
        {512,0xfffffff0u,255},{512,0,65536},{684,0,1},{684,1,1},{684,2,1},
        {684,510,255},{684,4000,255},{4,100,255}
    };
    int fail=0; for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++) fail+=one(cases[i].h,cases[i].from,cases[i].n);
    printf("differential_failed=%d total=%zu\n",fail,sizeof cases/sizeof cases[0]); return fail?1:0;
}
