/* Audit-only: quiescent mixed rail/gray/completed-plan/motion states, plus wrap boundaries. */
#define main prior_main
#include "pq_skip.c"
#undef main

static uint32_t rng(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }
static void mixed(pqbn_v4_ctx *c, int h, uint32_t from, int mode) {
    setup(c, h, from - 700);
    uint32_t seed = 91;
    for (int i=0;i<c->W*h/2;i++) ((uint8_t*)c->fb4)[i]=(uint8_t)rng(&seed);
    pqbn_v4_init(c, from - 700);
    c->auto_motion=mode==2;
    c->plan_budget=512;
    for (uint32_t i=1;i<=700;i++) {
        if (i==340 || i==480 || i==530) {
            for (int t=0;t<c->ntx*c->nty;t++) if ((rng(&seed)&7)==0) {
                int y=t/c->ntx*4,x=t%c->ntx*16;
                for(int r=0;r<4;r++) memset((uint8_t*)c->fb4+(y+r)*32+x,(rng(&seed)&1)?0:255,16);
                c->tile_dirty[t]=1;
            }
        }
        if(mode && i==540) {
            uint8_t *sel=zalloc((size_t)c->ntx*c->nty);
            for(int t=0;t<c->ntx*c->nty;t++) sel[t]=(t%3)==0;
            pqbn_v4_motion_enter(c,from-700+i,sel); free(sel);
        }
        pqbn_v4_phase(c,from-700+i);
    }
    for(int t=0;t<c->ntx*c->nty;t++)
        if(c->tile_dirty[t] || (c->tile_nmax[t]&(T_PEND|T_CAP)) ||
           (uint16_t)(from-c->tile_plan[t]) < (c->tile_nmax[t]&T_NMAX)) abort();
}
static int state_diff(const pqbn_v4_ctx *a,const pqbn_v4_ctx *b) {
    size_t px=(size_t)a->W*a->H,nt=(size_t)a->ntx*a->nty;
    return differences(a->hot,b->hot,px/2)+differences(a->cold,b->cold,px*2)+
        differences(a->tile_plan,b->tile_plan,nt*2)+differences(a->tile_nmax,b->tile_nmax,nt)+
        differences(a->tile_dirty,b->tile_dirty,nt)+differences(a->band,b->band,a->nty*4)+
        differences(a->row_active,b->row_active,a->H)+
        (a->plan_cursor!=b->plan_cursor)+(a->backlog!=b->backlog)+(a->n_motion!=b->n_motion)+
        (a->stat_planned!=b->stat_planned);
}
static int extended_one(int h,uint32_t from,uint32_t n,int mode,int periodic) {
    pqbn_v4_ctx a,b; mixed(&a,h,from,mode); mixed(&b,h,from,mode);
    /* For enormous rests, run >=65536 real phases to reach saturation. Then the full idle state
     * repeats every 65536 phases (10/16-bit clocks; no pending work or automatic motion exits).
     * Verify that cycle explicitly before using it as the oracle. */
    uint32_t refn=periodic?65536+(n&65535):n;
    for(uint32_t i=0;i<refn;i++) pqbn_v4_phase(&a,from+i+1);
    if(periodic) {
        pqbn_v4_ctx cycle; mixed(&cycle,h,from,mode);
        for(uint32_t i=0;i<refn+65536;i++) pqbn_v4_phase(&cycle,from+i+1);
        if(state_diff(&a,&cycle)) abort();
        destroy(&cycle);
    }
    int rows=pqbn_v4_skip(&b,from,from+n),expected=0;
    if(n>512) expected=h;
    else for(uint32_t i=0;i<n;i++) {uint32_t p=(from+i+1)&511;expected+=(int)((p+1)*h/512-p*h/512);}
    int before=state_diff(&a,&b);
    /* Exercise motion exit and next planning/execution, not merely timestamp equality. */
    pqbn_v4_motion_exit(&a,from+n+1,NULL); pqbn_v4_motion_exit(&b,from+n+1,NULL);
    memset((void*)a.fb4,0x77,(size_t)a.W*h/2);memset((void*)b.fb4,0x77,(size_t)b.W*h/2);
    memset(a.tile_dirty,1,(size_t)a.ntx*a.nty);memset(b.tile_dirty,1,(size_t)b.ntx*b.nty);
    int after=0,drive=0;
    for(int p=1;p<=20;p++) {
        pqbn_v4_phase(&a,from+n+p);pqbn_v4_phase(&b,from+n+p);
        after+=state_diff(&a,&b);
        for(int y=0;y<h;y++) {uint8_t x[16]={0},z[16]={0};
            int ra=pqbn_v4_exec_row(&a,from+n+p,y,x),rb=pqbn_v4_exec_row(&b,from+n+p,y,z);
            drive+=(ra!=rb)+differences(x,z,sizeof x);
        }
    }
    int fail=before||after||drive||rows!=expected;
    if(fail) printf("FAIL h=%d from=%u n=%u mode=%d periodic=%d state=%d next=%d drive=%d rows=%d expected=%d\n",
        h,from,n,mode,periodic,before,after,drive,rows,expected);
    destroy(&a);destroy(&b);return fail;
}
int main(void) {
    const uint32_t starts[]={0,1,255,511,3999,4095,4096,65530,0xffffff00u,0xffffffffu};
    const uint32_t ns[]={0,1,2,255,256,511,512,513,768,1024,4095,4096,9000,65535,65536,65537};
    int total=0,failed=0;
    for(int mode=0;mode<3;mode++) for(unsigned s=0;s<sizeof starts/sizeof starts[0];s++)
        for(unsigned n=0;n<sizeof ns/sizeof ns[0];n++) {
            failed+=extended_one(8,starts[s],ns[n],mode,0);total++;
        }
    for(int mode=0;mode<3;mode++) for(unsigned s=0;s<sizeof starts/sizeof starts[0];s++) {
        failed+=extended_one(684,starts[s],513,mode,0);total++;
        failed+=extended_one(684,starts[s],9000,mode,0);total++;
    }
    const uint32_t large[]={0x7fffffffu,0x80000000u,0xfffffffeu,0xffffffffu};
    for(int mode=0;mode<3;mode++) for(unsigned n=0;n<sizeof large/sizeof large[0];n++) {
        failed+=extended_one(8,0xfffffff0u,large[n],mode,1);total++;
    }
    printf("extended_failed=%d total=%d direct=540 periodic=12\n",failed,total);
    return failed?1:0;
}
