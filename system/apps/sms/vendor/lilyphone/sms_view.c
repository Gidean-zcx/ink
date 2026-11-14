/* Copied from LilyPhone (github.com/dparnell/lilyphone1 @de10fc3, src/ui/ui_phone1.cpp, by Daniel Parnell;
 * the repository has no licence file). Whole functions as they are: SOURCE.md lists every changed line. */
#include "sms_lily.h"

/* Queues a message and logs it as pending in one step. Shared by the composer
 * and by sending a reaction, so both end up watched the same way. */
static bool ui_sms_dispatch(const char *number, const char *text)
{
    if(number == NULL || number[0] == '\0') return false;
    if(text == NULL || text[0] == '\0') return false;
    if(ui_send_watch_id != 0) return false; // one in flight is enough

    uint32_t send_id = ui_sms_send(number, text);
    if(send_id == 0) return false;

    // Logged straight away as pending so the conversation reads correctly
    // however long the network takes.
    ui_send_watch_idx = sms_add(number, text, (uint32_t)time(NULL),
                                SMS_DIR_OUT, SMS_ST_PENDING, false);
    ui_send_watch_id  = send_id;
    ui_sms_revision++;
    return true;
}

/* Message bodies arrive with the sender's line breaks in them, and a list row
 * has exactly one line to spare. Collapse every run of whitespace to a single
 * space and drop control characters, so a row cannot grow past its height.
 * Bytes above 0x7F are passed through untouched to keep UTF-8 intact. */
static void ui_message_snippet(char *buf, int len, const char *text)
{
    int  o     = 0;
    bool space = false;

    if(text == NULL) text = "";
    while(*text && isspace((unsigned char)*text)) text++; // leading whitespace

    for(const char *p = text; *p && o < len - 1; p++) {
        unsigned char c = (unsigned char)*p;

        if(c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            space = true;
            continue;
        }
        if(c < 0x20 || c == 0x7F) continue; // other control characters

        if(space && o > 0 && o < len - 1) buf[o++] = ' ';
        space = false;
        if(o < len - 1) buf[o++] = (char)c;
    }
    buf[o] = '\0';
}

/* "14:05" for today, "04/09" for anything older. The list rows have only a
 * narrow column beside the name, so they use this rather than the full stamp. */
static void ui_format_stamp_compact(char *buf, int len, uint32_t ts)
{
    if(ts == 0) {
        lv_snprintf(buf, len, "--:--");
        return;
    }

    time_t    when = (time_t)ts;
    time_t    now  = time(NULL);
    struct tm when_tm;
    struct tm now_tm;

    localtime_r(&when, &when_tm);
    localtime_r(&now, &now_tm);

    if(when_tm.tm_year == now_tm.tm_year && when_tm.tm_yday == now_tm.tm_yday) {
        strftime(buf, len, "%H:%M", &when_tm);
    } else {
        strftime(buf, len, TR("%m月%d日"), &when_tm);
    }
}

/* "14:05" for today, "04/09 14:05" for anything older. Shows "--:--" until the
 * GPS has given us a clock. */
static void ui_format_stamp(char *buf, int len, uint32_t ts)
{
    if(ts == 0) {
        lv_snprintf(buf, len, "--:--");
        return;
    }

    time_t    when = (time_t)ts;
    time_t    now  = time(NULL);
    struct tm when_tm;
    struct tm now_tm;

    localtime_r(&when, &when_tm);
    localtime_r(&now, &now_tm);

    if(when_tm.tm_year == now_tm.tm_year && when_tm.tm_yday == now_tm.tm_yday) {
        strftime(buf, len, "%H:%M", &when_tm);
    } else {
        strftime(buf, len, TR("%m月%d日 %H:%M"), &when_tm);
    }
}

static void scr13_populate(void)
{
    lv_obj_clean(scr13_list);

    int threads = sms_thread_count();
    if(threads == 0) {
        scr_empty_note_create(scr13_list, "No messages yet.\n\nUse " LV_SYMBOL_PLUS " to write one.");
    }

    for(int i = 0; i < threads; i++) {
        const char      *number = sms_thread_number(i);
        const sms_msg_t *last   = sms_thread_last(i);
        char title[CONTACT_NAME_LEN + 8];
        char snippet[96];
        char badge[16];

        // An unread marker in the title is easier to spot on e-ink than a
        // separate dot would be.
        lv_snprintf(title, sizeof(title), "%s%s",
                    sms_thread_unread(number) > 0 ? "● " : "",
                    contacts_display_name(number));

        // A row previews the conversation; the whole message is one tap away.
        ui_reaction_t reaction;
        if(last && ui_reaction_parse(last->text, &reaction)) {
            lv_snprintf(snippet, sizeof(snippet), "%s %s \"%s\"",
                        reaction.emoji, reaction.word, reaction.quoted);
        } else {
            ui_message_snippet(snippet, sizeof(snippet), last ? last->text : "");
        }
        if(last && last->dir == SMS_DIR_OUT) {
            // Without a prefix a thread you last replied to reads as if they
            // said it. The delivery state goes here too: sending returns to
            // this screen, so this is where a failure has to be visible.
            const char *prefix = (last->status == SMS_ST_FAILED)  ? TR("未发出：")
                               : (last->status == SMS_ST_UNKNOWN) ? TR("结果未知：")
                               : (last->status == SMS_ST_DRAFT)   ? TR("草稿：")
                               : (last->status == SMS_ST_PENDING) ? TR("发送中：")
                                                                  : TR("我：");
            char line[sizeof(snippet)];
            lv_snprintf(line, sizeof(line), "%s%s", prefix, snippet);
            lv_snprintf(snippet, sizeof(snippet), "%s", line);
        }

        ui_format_stamp_compact(badge, sizeof(badge), last ? last->ts : 0);

        scr_row_create(scr13_list, title, snippet, badge,
                       scr13_row_event, (void *)(intptr_t)i);
    }

    scr13_built_revision = ui_sms_revision;
}

// The message a tapped bubble refers to, as an index into the whole log.
static int scr13_1_delete_target = -1;

/* Removing an entry shifts every later index down, including the one the
 * pending-send watcher is holding on to. */
static void ui_sms_delete_at(int idx)
{
    if(!sms_delete(idx)) return;

    if(ui_send_watch_id != 0) {
        if(ui_send_watch_idx == idx)     ui_send_watch_id = 0; // the watched message is gone
        else if(ui_send_watch_idx > idx) ui_send_watch_idx--;
    }
    ui_sms_revision++;
}

static void scr13_1_do_delete_thread(void)
{
    sms_thread_delete(ui_active_number);

    // Absolute indexes are meaningless now that a whole conversation left.
    ui_send_watch_id = 0;
    ui_sms_revision++;
    scr_mgr_pop(false);
}

static void scr13_1_do_delete_message(void)
{
    ui_sms_delete_at(scr13_1_delete_target);
    scr13_1_delete_target = -1;

    // Nothing left to look at once the last message in the thread is gone.
    if(sms_thread_msg_count(ui_active_number) == 0) {
        scr_mgr_pop(false);
        return;
    }
    scr13_1_populate();
}

/* One message, placed at `y` in the conversation; returns the y the next
 * bubble should start at. Received messages sit against the left edge, sent
 * ones are pushed right, which is the only cue this display needs to tell them
 * apart. The bubbles are positioned by hand rather than by a flex layout
 * because this build of LVGL has no margin properties to offset one child
 * against the rest. */
static lv_coord_t scr13_1_bubble_create(lv_obj_t *parent, const sms_msg_t *m, int log_idx,
                                        lv_coord_t y, const char *badge)
{
    char body[SMS_TEXT_LEN];
    LV_UNUSED(badge);

    /* A reaction only lands here when its target is not on screen - normally it
     * is drawn as a badge on the message it refers to instead. Shown as a short
     * annotation, since the text a phone sends quotes that message in full. */
    ui_reaction_t reaction;
    bool is_reaction = ui_reaction_parse(m->text, &reaction);

    if(is_reaction) {
        // Enough of the excerpt to tell which message it was about.
        if(strlen(reaction.quoted) > 28) lv_snprintf(reaction.quoted + 25, 4, "...");
        lv_snprintf(body, sizeof(body), "%s \"%s\"", reaction.word, reaction.quoted);
    } else {
        lv_snprintf(body, sizeof(body), "%s", m->text);
    }

    lv_obj_t *bubble = lv_obj_create(parent);
    lv_obj_set_width(bubble, ink_ui_px(SMS_BUBBLE_W));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(bubble, m->dir == SMS_DIR_OUT ? DECKPRO_COLOR_BG : DECKPRO_COLOR_SHADE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bubble, m->dir == SMS_DIR_OUT ? LV_OPA_TRANSP : LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_border_color(bubble, DECKPRO_COLOR_FG, LV_PART_MAIN);
    // No outline on a reaction, so it reads as a note against the conversation
    // rather than as another message in it.
    lv_obj_set_style_border_width(bubble, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bubble, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bubble, ink_ui_px(INK_GAP_S), LV_PART_MAIN);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(bubble, m->dir == SMS_DIR_OUT ? ink_ui_px(SMS_CONTENT_W - SMS_BUBBLE_W) : 0, y);

    // Tapping a bubble is how a single message gets deleted. A plain object is
    // not clickable by default.
    lv_obj_add_flag(bubble, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bubble, scr13_1_bubble_event, LV_EVENT_CLICKED, (void *)(intptr_t)log_idx);

    lv_obj_t *text = lv_label_create(bubble);
    lv_obj_set_width(text, lv_pct(100));
    lv_obj_set_style_text_font(text, FONT_BOLD_SIZE_15, LV_PART_MAIN);
    lv_obj_set_style_text_color(text, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
    lv_label_set_text(text, body);
    if(m->dir == SMS_DIR_OUT) {
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    }

    // The height only exists once the wrapped text has been laid out.
    lv_obj_update_layout(bubble);
    lv_coord_t bubble_h = lv_obj_get_height(bubble);
    lv_coord_t extra    = 0;

    return y + bubble_h + extra + ink_ui_px(INK_GAP_M);
}

static void scr13_2_send_event(lv_event_t *e)
{
    LV_UNUSED(e);

    const char *body = lv_textarea_get_text(scr13_2_body_ta);

    if(ui_active_number[0] == '\0') {
        lv_label_set_text(scr13_2_status, TR("请先写收件人"));
        return;
    }
    if(body == NULL || body[0] == '\0') {
        lv_label_set_text(scr13_2_status, TR("还没写内容"));
        return;
    }
    if(ui_send_watch_id != 0) {
        lv_label_set_text(scr13_2_status, TR("上一条还在发"));
        return;
    }

    if(!ui_sms_dispatch(ui_active_number, body)) {
        lv_label_set_text(scr13_2_status, TR("现在发不出去，请重试"));
        return;
    }

    // Clear before leaving, or exit13_2 keeps the sent text as a draft.
    lv_textarea_set_text(scr13_2_body_ta, "");

    /* Straight back to whatever opened the composer, rather than waiting on the
     * network: the message is already in the log as pending, and the screen
     * behind shows it that way. The outcome is watched centrally, so it lands
     * wherever the user happens to be by then. */
    lily_sent();
}

/* Glue (ours, not LilyPhone): the send watch the dispatcher keeps, and the entry points sms.c calls. */
uint32_t ui_send_watch_id INK_PSRAM_BSS, ui_sms_revision INK_PSRAM_BSS, scr13_built_revision INK_PSRAM_BSS;
int ui_send_watch_idx INK_PSRAM_BSS;
lv_obj_t *scr13_2_body_ta INK_PSRAM_BSS, *scr13_2_status INK_PSRAM_BSS;
bool lily_dispatch(const char *number, const char *text) { return ui_sms_dispatch(number, text); }
void lily_snippet(char *buf, int len, const char *text) { ui_message_snippet(buf, len, text); }
void lily_stamp(char *buf, int len, uint32_t ts, bool compact) { if (compact) ui_format_stamp_compact(buf, len, ts); else ui_format_stamp(buf, len, ts); }
void lily_rows(void) { scr13_populate(); }
void lily_delete(int idx) { scr13_1_delete_target = idx; scr13_1_do_delete_message(); }
void lily_delete_thread(void) { scr13_1_do_delete_thread(); }
lv_coord_t lily_bubble(lv_obj_t *parent, const sms_msg_t *m, int id, lv_coord_t y) { return scr13_1_bubble_create(parent, m, id, y, NULL); }
void lily_send(void) { scr13_2_send_event(NULL); }
