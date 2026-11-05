#include "late_db_gate.h"
#ifndef INK_TEST_CAP_SOURCE
#define INK_TEST_CAP_SOURCE "../../system/policy/cap.c"
#endif
#include INK_TEST_CAP_SOURCE

void ink_test_late_activity_save(void) {
    static activity_note_t note;
    snprintf(note.member, sizeof note.member, "oldadult");
    snprintf(note.object, sizeof note.object, "SYNTH_OLD");
    snprintf(note.cap, sizeof note.cap, "reader.next_page");
    note.generation = ink_member_generation(note.member);
    note.used = true;
    activity_flush(&note);
}
