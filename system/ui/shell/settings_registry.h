/* The settings registry (settings-1): every setting is declared once, the way PebbleOS declares its settings modules
 * (pebble/apps/system/settings/menu.c:23-50); each row's summary, availability and search entry come from that one
 * declaration, as AOSP derives them from one controller (core/BasePreferenceController.java:75-121). The home's rows,
 * the list pages' rows, the search and settings.open read this table. UI task. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {           /* the six states of BasePreferenceController.java:75-121 */
    INK_SR_AVAILABLE,    /* AVAILABLE */
    INK_SR_UNSEARCHABLE, /* AVAILABLE_UNSEARCHABLE: on its page, not found by search */
    INK_SR_NOT_NOW,      /* CONDITIONALLY_UNAVAILABLE: left out while its condition does not hold */
    INK_SR_UNSUPPORTED,  /* UNSUPPORTED_ON_DEVICE: left out of the home and search, grayed on its own page */
    INK_SR_PARENT,       /* DISABLED_FOR_USER: a child sees 家长管理, never the value */
    INK_SR_DEPENDENT,    /* DISABLED_DEPENDENT_SETTING: grayed, the reason in place of the value */
} ink_sr_state_t;
/* its row is on the settings home / in its page's list / on 系统 (the home's rarely used rows, D-137) / on a child's home */
enum { INK_SR_HOME = 1, INK_SR_LIST = 2, INK_SR_SYSTEM = 4, INK_SR_KID = 8 };

typedef struct ink_sr_item ink_sr_item_t;
struct ink_sr_item {
    const char *id, *title, *aliases, *icon; /* aliases: more words the search finds it by, space separated */
    const char *page, *open;     /* the page it is on (activity), and the capability opening it when not by name */
    const char *setting;         /* what settings.get and settings.set read and change for it */
    const char *cap, *args;      /* what a tap on its row does; NULL on the home: settings.open */
    const char *section, *note, *text; /* list rows: a heading above, a note under, a fixed value */
    void (*summary)(const ink_sr_item_t *it, char *out, size_t cap); /* what it is now; NULL: text, setting or state */
    ink_sr_state_t (*state)(char *why, size_t cap);                  /* NULL: available; why: what the row says */
    uint8_t group, where;
    bool adult; /* for adults: a child gets INK_SR_PARENT */
};

const ink_sr_item_t *ink_sr_at(int index); /* NULL past the last */
const ink_sr_item_t *ink_sr_find(const char *id);
const char *ink_sr_group(int group);
/* The item's state for whoever uses the device now; `out` is what its row says: the value, or why not. */
ink_sr_state_t ink_sr_value(const ink_sr_item_t *it, char *out, size_t cap);
/* settings.search: what the query names by title, alias or group, title matches first. Returns how many. */
int ink_sr_search(const char *query, const ink_sr_item_t **out, int max);
extern const ink_sr_item_t *ink_sr_target; /* the row settings.open asked for, while its page is built */
int ink_settings_home_init(void);          /* settings.open (every id in its description) and settings.search */
void ink_settings_home_sync(void);         /* the home writes what changed into its rows (list pages' watchers) */
