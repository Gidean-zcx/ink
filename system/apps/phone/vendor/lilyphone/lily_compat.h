/* Glue (ours, not LilyPhone): what the copied phone_store.c and call_card.c expect from their Arduino and LVGL 8
 * project, mapped onto this system. phone.c implements the ui_phone_* and scr_* functions. */
#ifndef LILY_COMPAT_H
#define LILY_COMPAT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ink/sys/i18n.h"
#include "ink/ui_theme.h"

/* include/phone_store.h, with this system's sizes: phone.contacts names and numbers, and its 200-record limit */
#define CONTACT_NAME_LEN 97
#define CONTACT_NUMBER_LEN 25
#define CONTACTS_MAX 200
typedef struct {
    char name[CONTACT_NAME_LEN];
    char number[CONTACT_NUMBER_LEN];
} contact_t;
bool phone_number_match(const char *a, const char *b);
int contacts_find_by_number(const char *number);
const char *contacts_display_name(const char *number);
void lily_contacts_bind(contact_t *table, int count);

/* ui_phone1.cpp's view of the modem, answered from phone.c's call state */
typedef enum { MODEM_CALL_IDLE, MODEM_CALL_DIALING, MODEM_CALL_INCOMING, MODEM_CALL_ACTIVE } modem_call_state_t;
modem_call_state_t ui_phone_get_call_state(void);
void ui_phone_get_call_number(char *buf, size_t cap);
const char *ui_phone_end_text(void); /* the ended state's line: duration, or why it did not connect */
void ui_phone_answer(void);          /* call.answer */
void ui_phone_hang_up(void);         /* call.hangup */
#define ui_active_number ""          /* ui_phone_get_call_number always answers */

/* its theme: our tokens (text grows with the system text size) */
#define FONT_BOLD_SIZE_20 ink_ui_font_px(INK_FONT_TITLE, INK_WEIGHT_BOLD)
#define FONT_BOLD_SIZE_16 ink_ui_font_px(INK_FONT_BODY, INK_WEIGHT_MEDIUM)
#define FONT_BOLD_SIZE_15 ink_ui_font_px(INK_FONT_NOTE, INK_WEIGHT_MEDIUM)
#define DECKPRO_COLOR_FG ink_ui_ink()
/* A changed label invalidates only itself, so the panel refreshes that area: no full refresh. */
#define ui_disp_full_refr() ((void)0)
lv_obj_t *scr_action_bar_create(lv_obj_t *parent, int height);
lv_obj_t *scr_bar_btn_create(lv_obj_t *bar, const char *text, int width, lv_event_cb_t cb, void *ctx);

/* call_card.c's own glue: build the card, follow the state, forget it with the page */
lv_obj_t *lily_call_card(lv_obj_t *parent); /* returns the status line */
void lily_call_card_render(void);
void lily_call_card_gone(void);
#endif
