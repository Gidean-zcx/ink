/* Links the unchanged production JSON parser. The only stub is cost accounting. */
#include <stdio.h>
#include <stdint.h>
#include "ink/sys/json.h"
void osal_cost_units(const char *cost_class, uint64_t units) {
    (void)cost_class;
    (void)units;
}
int main(void) {
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, stdin);
    buf[n] = '\0';
    ink_json_t j;
    int rc = ink_json_parse(&j, buf, n);
    printf("%d %d\n", rc, rc == 0 ? j.ntok : -1);
    return 0;
}
