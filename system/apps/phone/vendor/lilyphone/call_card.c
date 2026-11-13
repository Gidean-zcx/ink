/* Copied from LilyPhone (github.com/dparnell/lilyphone1 @de10fc3, src/ui/ui_phone1.cpp, by Daniel Parnell;
 * the repository has no licence file): screen 8.1, the call card. Whole functions as they are, the timer
 * variable and timer callback left out; SOURCE.md lists every changed line. */
#include "lily_compat.h"

static lv_obj_t   *scr8_1_status INK_PSRAM_BSS;
static lv_obj_t   *scr8_1_answer INK_PSRAM_BSS;
static modem_call_state_t scr8_1_shown_state INK_PSRAM_BSS; /* MODEM_CALL_IDLE is 0 */

static void scr8_1_answer_event(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_phone_answer();
}

static void scr8_1_hangup_event(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_phone_hang_up();
}

static void scr8_1_render(void)
{
    modem_call_state_t state = ui_phone_get_call_state();

    switch(state) {
        case MODEM_CALL_INCOMING:
            lv_label_set_text(scr8_1_status, TR("来电"));
            break;

        case MODEM_CALL_DIALING:
            lv_label_set_text(scr8_1_status, TR("正在拨号"));
            break;

        case MODEM_CALL_ACTIVE: {
            lv_label_set_text(scr8_1_status, TR("通话中"));
            break;
        }

        default:
            lv_label_set_text(scr8_1_status, ui_phone_end_text());
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

    const char *name = number[0] ? contacts_display_name(number) : TR("未知号码");

    lv_obj_t *who = lv_label_create(parent);
    lv_obj_set_width(who, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
    lv_obj_set_style_text_font(who, FONT_BOLD_SIZE_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(who, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_align(who, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(who, LV_LABEL_LONG_WRAP);
    lv_label_set_text(who, name);
    lv_obj_align(who, LV_ALIGN_TOP_MID, 0, ink_ui_px(INK_STATUS_BAR + INK_BLOCK));

    // When the name came from the contact book, still show the raw number.
    lv_obj_t *sub = lv_label_create(parent);
    lv_obj_set_width(sub, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
    lv_obj_set_style_text_font(sub, FONT_BOLD_SIZE_15, LV_PART_MAIN);
    lv_obj_set_style_text_color(sub, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
    lv_obj_set_height(sub, ink_ui_px(ink_ui_line_height(INK_FONT_NOTE)));
    lv_label_set_text(sub, (strcmp(name, number) == 0) ? " " : number);
    lv_obj_align_to(sub, who, LV_ALIGN_OUT_BOTTOM_MID, 0, ink_ui_px(INK_GAP_XS));

    scr8_1_status = lv_label_create(parent);
    lv_obj_set_width(scr8_1_status, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
    lv_obj_set_style_text_font(scr8_1_status, FONT_BOLD_SIZE_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(scr8_1_status, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_align(scr8_1_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(scr8_1_status, TR("正在拨号"));
    lv_obj_align_to(scr8_1_status, sub, LV_ALIGN_OUT_BOTTOM_MID, 0, ink_ui_px(INK_GAP));

    /* Taller than a list row: these are the most urgent controls in the phone,
     * and since a call raises this screen over a locked one they are often the
     * first thing pressed after picking the phone up. */
    lv_obj_t *bar = scr_action_bar_create(parent, INK_PRIMARY);
    scr8_1_answer = scr_bar_btn_create(bar, TR("接听"), INK_CONTROL, scr8_1_answer_event, NULL);
    lv_obj_t *hangup = scr_bar_btn_create(bar, TR("挂断"), INK_CONTROL, scr8_1_hangup_event, NULL);

    lv_obj_set_height(scr8_1_answer, ink_ui_px(INK_PRIMARY));
    lv_obj_set_height(hangup, ink_ui_px(INK_PRIMARY));

}
/* Glue (ours, not LilyPhone): what phone.c calls instead of the screen manager's entry and timer. */
lv_obj_t *lily_call_card(lv_obj_t *parent)
{
    scr8_1_shown_state = MODEM_CALL_IDLE;
    create8_1(parent);
    scr8_1_render();
    return scr8_1_status;
}
void lily_call_card_render(void) { if(scr8_1_status) scr8_1_render(); }
void lily_call_card_gone(void) { scr8_1_status = scr8_1_answer = NULL; }
