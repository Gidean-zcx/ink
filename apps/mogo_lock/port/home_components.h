/* The mini library's mode is the lock style's own setting (display.lock_library), not a home component. */
#pragma once
#include <stdbool.h>
bool app_home_library_recent(void);
bool app_home_library_set_recent(bool recent);
