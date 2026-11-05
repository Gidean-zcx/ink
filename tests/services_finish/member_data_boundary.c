#include <string.h>
#include "ink/osal.h"
#include "ink/sys/data.h"
extern volatile bool ink_test_armed, ink_test_waiting, ink_test_release;
static const ink_db_def_t *ink_test_find(const char *name) {
    const ink_db_def_t *d=ink_db_find_def(name);
    if(ink_test_armed && !strcmp(name,"review6.notes")) {
        ink_test_armed=false; ink_test_waiting=true;
        while(!ink_test_release) osal_task_delay_us(10000);
    }
    return d;
}
#define ink_db_find_def ink_test_find
#ifndef INK_TEST_DATA_CAPS_SOURCE
#define INK_TEST_DATA_CAPS_SOURCE "../../system/services/data/data_caps.c"
#endif
#include INK_TEST_DATA_CAPS_SOURCE
