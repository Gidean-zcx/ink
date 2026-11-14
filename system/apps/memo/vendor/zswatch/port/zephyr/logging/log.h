/* Shim for the copied ZSWatch voice memo UI only (see ../../../SOURCE.md): Zephyr logging goes to ink_log. */
#pragma once
#include "ink/sys/log.h"

#define LOG_MODULE_REGISTER(...)
#define LOG_DBG(...) ink_log(INK_LOG_DEBUG, "voice_memo_ui", __VA_ARGS__)
#define LOG_INF(...) ink_log(INK_LOG_INFO, "voice_memo_ui", __VA_ARGS__)
#define LOG_WRN(...) ink_log(INK_LOG_WARN, "voice_memo_ui", __VA_ARGS__)
#define LOG_ERR(...) ink_log(INK_LOG_ERROR, "voice_memo_ui", __VA_ARGS__)
