/* Cross-check generated server messages with the unmodified production tokenizer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/json.h"

void osal_cost_units(const char *name, uint64_t units) { (void)name; (void)units; }

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 2;
    char *line = NULL;
    size_t cap = 0, count = 0;
    ssize_t n;
    int max = 0;
    while ((n = getline(&line, &cap, file)) >= 0) {
        ink_json_t json;
        count++;
        if (ink_json_parse(&json, line, (size_t)n) || json.ntok > INK_JSON_MAX_TOKENS) {
            fprintf(stderr, "sample %zu: production parser rejected the message\n", count);
            free(line);
            fclose(file);
            return 1;
        }
        if (json.ntok > max) max = json.ntok;
    }
    free(line);
    fclose(file);
    printf("production parser: %zu generated messages, maximum %d tokens\n", count, max);
    return count ? 0 : 1;
}
