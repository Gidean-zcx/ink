/* SHA-256, HMAC and PBKDF2 against the published test vectors (FIPS 180-4, RFC 4231, RFC 7914 §11). */
#include <stdio.h>
#include <string.h>

#include "ink/sys/sha256.h"

void osal_cost_units(const char *cost_class, uint64_t units);
void osal_preempt_point(void);
void osal_cost_units(const char *cost_class, uint64_t units) {
    (void)cost_class;
    (void)units;
}
void osal_preempt_point(void) {}

static int g_fail;

static void hex(const uint8_t *d, char *out) {
    for (int i = 0; i < 32; i++) snprintf(out + i * 2, 3, "%02x", d[i]);
}

static void expect(const char *what, const uint8_t *got, const char *want) {
    char h[65];
    hex(got, h);
    if (strcmp(h, want)) {
        printf("FAIL %s\n  got  %s\n  want %s\n", what, h, want);
        g_fail++;
    }
}

int main(void) {
    uint8_t d[32];
    ink_sha256("", 0, d);
    expect("sha256 empty", d, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    ink_sha256("abc", 3, d);
    expect("sha256 abc", d, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    ink_sha256(two, strlen(two), d);
    expect("sha256 two blocks", d, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    /* in pieces, a million a's */
    ink_sha256_t s;
    ink_sha256_init(&s);
    char a[1000];
    memset(a, 'a', sizeof a);
    for (int i = 0; i < 1000; i++) ink_sha256_update(&s, a, sizeof a);
    ink_sha256_final(&s, d);
    expect("sha256 million", d, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    /* RFC 4231 test 1 and 2 */
    uint8_t k1[20];
    memset(k1, 0x0b, sizeof k1);
    ink_hmac_sha256(k1, sizeof k1, "Hi There", 8, d);
    expect("hmac 1", d, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    ink_hmac_sha256("Jefe", 4, "what do ya want for nothing?", 28, d);
    expect("hmac 2", d, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    /* a key longer than a block: RFC 4231 test 6 */
    uint8_t k6[131];
    memset(k6, 0xaa, sizeof k6);
    const char *m6 = "Test Using Larger Than Block-Size Key - Hash Key First";
    ink_hmac_sha256(k6, sizeof k6, m6, strlen(m6), d);
    expect("hmac 6", d, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    /* PBKDF2-HMAC-SHA256 */
    ink_pbkdf2_sha256("password", 8, "salt", 4, 1, d);
    expect("pbkdf2 1", d, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    ink_pbkdf2_sha256("password", 8, "salt", 4, 2, d);
    expect("pbkdf2 2", d, "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
    ink_pbkdf2_sha256("password", 8, "salt", 4, 4096, d);
    expect("pbkdf2 4096", d, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
    if (g_fail) return 1;
    printf("sha256: all vectors match\n");
    return 0;
}
