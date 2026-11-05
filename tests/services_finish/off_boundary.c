#include "ink/sys/power.h"
#ifdef INK_TEST_OLD_OFF
static void legacy_set_picture(void (*fn)(const char *));
#define ink_power_set_off_picture legacy_set_picture
#endif
#ifndef INK_TEST_OFF_SOURCE
#define INK_TEST_OFF_SOURCE "../../system/ui/shell/off_picture.c"
#endif
#include INK_TEST_OFF_SOURCE

#ifdef INK_TEST_OLD_OFF
#undef ink_power_set_off_picture
static void (*legacy_picture)(const char *);
static void bridge(const char *why) { legacy_picture(why); ink_power_off_picture_result(true); }
static void legacy_set_picture(void (*fn)(const char *)) { legacy_picture=fn; ink_power_set_off_picture(bridge); }
#endif
