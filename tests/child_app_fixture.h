/* Existing file tests opt in through a real parent, preserving their boundary assertions. */
static volatile bool child_app_pin_done;
static int child_app_pin_result;
static void child_app_pin_reply(void *ctx, int result, uint32_t wait) {
    (void)ctx; (void)wait; child_app_pin_result = result; child_app_pin_done = true;
}
static void permit_child_files(void) {
    child_app_pin_done = false;
    CHECK(ink_member_set_pin("owner", "", "24681357", child_app_pin_reply, NULL) == 0 &&
        WAIT_FOR(child_app_pin_done, 6000) && child_app_pin_result == INK_PIN_OK, "prepare the synthetic parent's verifier");
    CHECK(AI("kid.app_set", "{\"id\":\"kid\",\"app\":\"files\",\"mode\":1}") && g_rc.status == INK_RC_PENDING_CONFIRMATION,
        "the parent explicitly enables files for the child");
    unsigned token = 0;
    CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "file permission has a real confirmation");
    child_app_pin_done = false;
    CHECK(ink_confirm_accept_with_pin(token, "24681357", child_app_pin_reply, NULL) == 0 &&
        WAIT_FOR(child_app_pin_done, 6000) && child_app_pin_result == INK_PIN_OK, "the parent confirms the file permission locally");
    CHECK(WAIT_FOR(ink_member_app_mode("kid", "files") == 1, 6000), "file boundary tests run with parent permission");
    child_app_pin_done = false;
    CHECK(ink_member_set_pin("owner", "24681357", "", child_app_pin_reply, NULL) == 0 &&
        WAIT_FOR(child_app_pin_done, 6000) && child_app_pin_result == INK_PIN_OK, "restore the fixture's password-free adult");
    quiet();
}
