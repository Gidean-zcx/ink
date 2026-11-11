/* Capture page requests without rebooting, resetting, or using a real peer. */
#include "ink/sys/cap.h"
#include <stdio.h>
static char fx_called[64], fx_args[128];
static int fx_call(const char *id, const ink_args_t *args, const ink_call_opts_t *opts) {
    (void)args;
    snprintf(fx_called, sizeof fx_called, "%s", id);
    snprintf(fx_args, sizeof fx_args, "%s", opts && opts->args_json ? opts->args_json : "");
    return 0;
}
