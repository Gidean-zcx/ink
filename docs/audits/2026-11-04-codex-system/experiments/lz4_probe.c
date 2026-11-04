/* Bounded format/size correctness checks, not a security test. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/sys/lz4.h"
int main(void) {
    const size_t sizes[]={0,1,4,5,12,14,15,16,254,255,256,269,270,4096,65535,65536,70000};
    unsigned checks=0;
    void *work=malloc(INK_LZ4_WORK);
    for (unsigned s=0;s<sizeof sizes/sizeof sizes[0];s++) {
        size_t n=sizes[s],bound=ink_lz4_bound(n);
        uint8_t *src=malloc(n+1),*enc=malloc(bound),*dst=malloc(n+1);
        for (int pattern=0;pattern<3;pattern++) {
            for (size_t i=0;i<n;i++) src[i]=(uint8_t)(pattern==0 ? 0 : pattern==1 ? i%17 : (i*37+(i>>8)*19)%256);
            int z=ink_lz4_encode(src,n,enc,bound,work);
            assert(z>0 && (size_t)z<=bound);
            assert(ink_lz4_decode(enc,(size_t)z,dst,n)==(int)n && !memcmp(src,dst,n));
            checks++;
            if(n) { assert(ink_lz4_decode(enc,(size_t)z,dst,n-1)<0); checks++; }
            /* Truncations need not all be invalid (they can be valid shorter blocks). Check bounds. */
            for(int k=0;k<z;k++) {
                uint8_t *short_src=malloc((size_t)k+1);
                memcpy(short_src,enc,(size_t)k);
                int r=ink_lz4_decode(short_src,(size_t)k,dst,n);
                assert(r<0 || (size_t)r<=n); free(short_src); checks++;
            }
        }
        free(src); free(enc); free(dst);
    }
    const uint8_t bad[][4]={{0xF0,255,255,255},{0x10,65,0,0},{0x10,65,2,0},{0x1F,65,1,0}};
    uint8_t dst[32];
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++) {assert(ink_lz4_decode(bad[i],4,dst,sizeof dst)<0);checks++;}
    free(work); printf("AUDIT lz4 checks=%u failures=0\n",checks); return 0;
}
