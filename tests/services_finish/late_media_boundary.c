#include "late_db_gate.h"
#include "ink/sys/file.h"
#include "ink/sys/members.h"
static int test_card_write(const char *path, const void *data, size_t len) {
    ink_test_late_gate("media_card");
    return ink_fs_write_atomic(path, data, len);
}
static bool test_begin(const char *member, uint32_t generation) {
    ink_test_late_gate("media_card");
    return ink_member_io_begin(member, generation);
}
#define ink_fs_write_atomic test_card_write
#define ink_member_io_begin test_begin
#ifndef INK_TEST_MEDIA_SOURCE
#define INK_TEST_MEDIA_SOURCE "../../system/services/audio/media_service.c"
#endif
#include INK_TEST_MEDIA_SOURCE

void ink_test_late_media_save(bool card) {
    snprintf(M.member, sizeof M.member, "oldadult");
    M.generation = ink_member_generation(M.member);
    if (card) save_to_card("/family/synthetic.ogg", 12345);
    else save_to_flash("/family/synthetic.ogg", 12345);
}
