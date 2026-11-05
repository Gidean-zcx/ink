/* Original upgrade service, with a controllable resource-HAL error and a read-only observer. */
#include <stdio.h>
#include <stdlib.h>
#include "ink/hal_ota.h"
static int resource_calls, write_failure;
static unsigned first_byte;
static int audit_resource(int kind, size_t offset, const void *data, size_t n);
#define hal_ota_write_resource audit_resource
#include "../../../../system/services/update/update_service.c"
#undef hal_ota_write_resource

static int audit_resource(int kind, size_t offset, const void *data, size_t n) {
    if (!resource_calls && n) first_byte = *(const unsigned char *)data;
    resource_calls++;
    if (write_failure) return -1;
    return hal_ota_write_resource(kind, offset, data, n);
}

int audit_resource_case(int corrupt) { /* Called on exec.bg, the same executor as rest_job. */
    const char *path = "/content/update/audit.inkfw";
    pkg_t p;
    if (ink_fs_card_hold() != INK_FS_OK) return -1;
    int original = check_package(path, &p);
    if (original != PKG_OK) { ink_fs_card_release(); return -2; }
    if (corrupt) {
        /* A card edit between successful installation and the next boot. Keep the signed header unchanged. */
        hal_file_t *f = hal_fs_open(path, HAL_O_WRITE);
        unsigned char changed = 0xa5;
        if (!f || hal_fs_seek(f, (int64_t)p.sec[1].offset, 0) || hal_fs_write(f, &changed, 1) != 1) return -3;
        hal_fs_close(f);
    }
    int current = check_sections(path, &p);
    ink_fs_card_release();
    snprintf(U.st.state, sizeof U.st.state, "resources");
    snprintf(U.st.path, sizeof U.st.path, "%s", path);
    U.st.res_done_section = 0;
    write_failure = !corrupt;
    rest_job(NULL);
    printf("AUDIT resources corrupt=%d validated_before=%d section_check_now=%d writes=%d first_byte=%u state_cleared=%d path_cleared=%d\n",
           corrupt, original, current, resource_calls, first_byte, !U.st.state[0], !U.st.path[0]);
    return resource_calls > 0 && !U.st.state[0] && !U.st.path[0] && (!corrupt || (current == PKG_BAD_SECTION && first_byte == 0xa5)) ? 0 : -4;
}
