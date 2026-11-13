/* Copied from LilyPhone (github.com/dparnell/lilyphone1 @de10fc3, src/ui/ui_phone1.cpp, by Daniel Parnell;
 * the repository has no licence file): screen 8.1, the call card. Whole functions as they are, the timer
 * variable and timer callback left out; SOURCE.md lists every changed line. */
#include "lily_compat.h"

static lv_obj_t   *scr8_1_status = NULL;
static lv_obj_t   *scr8_1_answer = NULL;
static modem_call_state_t scr8_1_shown_state = MODEM_CALL_IDLE;

static void scr8_1_answer_event(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_phone_answer();
}

static void scr8_1_hangup_event(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_phone_hang_up();
    scr_mgr_pop(false);
}

static void scr8_1_render(void)
{
    modem_call_state_t state = ui_phone_get_call_state();
    char buf[40];

    switch(state) {
        case MODEM_CALL_INCOMING:
            lv_label_set_text(scr8_1_status, "Incoming call");
            break;

        case MODEM_CALL_DIALING:
            lv_label_set_text(scr8_1_status, "Calling...");
            break;

        case MODEM_CALL_ACTIVE: {
            uint32_t secs = ui_phone_get_call_duration() / 1000;
            lv_snprintf(buf, sizeof(buf), "Connected  %02u:%02u",
                        (unsigned)(secs / 60), (unsigned)(secs % 60));
            lv_label_set_text(scr8_1_status, buf);
            break;
        }

        default:
            lv_label_set_text(scr8_1_status, "Call ended");
            break;
    }

    // Answering only makes sense while the other end is still ringing.
    if(state == MODEM_CALL_INCOMING) {
        lv_obj_clear_flag(scr8_1_answer, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(scr8_1_answer, LV_OBJ_FLAG_HIDDEN);
    }

    if(state != scr8_1_shown_state) {
        scr8_1_shown_state = state;
        ui_disp_full_refr();
    }
}

static void create8_1(lv_obj_t *parent)
{
    char number[CONTACT_NUMBER_LEN];
    ui_phone_get_call_number(number, sizeof(number));
    if(number[0] == '\0') {
        lv_snprintf(number, sizeof(number), "%s", ui_active_number);
    }

    const char *name = number[0] ? contacts_display_name(number) : "Unknown caller";

    lv_obj_t *who = lv_label_create(parent);
    lv_obj_set_width(who, lv_pct(92));
    lv_obj_set_style_text_font(who, FONT_BOLD_SIZE_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(who, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_align(who, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(who, LV_LABEL_LONG_WRAP);
    lv_label_set_text(who, name);
    lv_obj_align(who, LV_ALIGN_TOP_MID, 0, 62);

    // When the name came from the contact book, still show the raw number.
    lv_obj_t *sub = lv_label_create(parent);
    lv_obj_set_width(sub, lv_pct(92));
    lv_obj_set_style_text_font(sub, FONT_BOLD_SIZE_15, LV_PART_MAIN);
    lv_obj_set_style_text_color(sub, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
    lv_obj_set_height(sub, 18);
    lv_label_set_text(sub, (strcmp(name, number) == 0) ? " " : number);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 126);

    scr8_1_status = lv_label_create(parent);
    lv_obj_set_width(scr8_1_status, lv_pct(92));
    lv_obj_set_style_text_font(scr8_1_status, FONT_BOLD_SIZE_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(scr8_1_status, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_align(scr8_1_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(scr8_1_status, "Calling...");
    lv_obj_align(scr8_1_status, LV_ALIGN_CENTER, 0, 20);

    /* Taller than a list row: these are the most urgent controls in the phone,
     * and since a call raises this screen over a locked one they are often the
     * first thing pressed after picking the phone up. */
    lv_obj_t *bar = scr_action_bar_create(parent, 58);
    scr8_1_answer = scr_bar_btn_create(bar, LV_SYMBOL_CALL "  Answer", 108, scr8_1_answer_event, NULL);
    lv_obj_t *hangup = scr_bar_btn_create(bar, LV_SYMBOL_CLOSE "  Hang up", 108, scr8_1_hangup_event, NULL);

    lv_obj_set_height(scr8_1_answer, 48);
    lv_obj_set_height(hangup, 48);

    scr_back_btn_create(parent, "Call", scr8_btn_event_cb);
}
