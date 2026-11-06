#include "ink/sys/data.h"
#include <string.h>
void ink_test_late_gate(const char *kind);
static void test_db_kind(const ink_db_def_t *def, const char *member) {
    if (!member || strcmp(member, "oldadult")) return;
    if (!strcmp(def->name, "media.positions")) ink_test_late_gate("media_flash");
    if (!strcmp(def->name, "notify.inbox")) ink_test_late_gate("notify");
    if (!strcmp(def->name, "notify.prefs")) ink_test_late_gate("notify_prefs");
    if (!strcmp(def->name, "activity.log")) ink_test_late_gate("activity");
}
static ink_db_t *test_open(const ink_db_def_t *def, const char *member) {
    test_db_kind(def, member);
    return ink_db_open(def, member);
}
static ink_db_t *test_open_member(const ink_db_def_t *def, const char *member) {
    test_db_kind(def, member);
    return ink_db_open_member(def, member);
}
#define ink_db_open test_open
#define ink_db_open_member test_open_member
