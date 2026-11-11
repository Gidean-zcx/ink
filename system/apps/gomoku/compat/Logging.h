#pragma once
extern "C" {
#include "ink/sys/log.h"
}
#define LOG_INF(tag, ...) ink_log(INK_LOG_INFO, tag, __VA_ARGS__)
#define LOG_ERR(tag, ...) ink_log(INK_LOG_ERROR, tag, __VA_ARGS__)
