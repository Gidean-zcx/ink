/* clock-3 shim for InfiniTime's WakeLock (SOURCE.md): one reference on INK_NEED_AWAKE, taken and given back once. */
#pragma once
#include "ink/sys/power.h"
namespace Pinetime::System {
    class WakeLock {
    public:
        void Lock() {
            if (!held) ink_power_hold(INK_NEED_AWAKE);
            held = true;
        }
        void Release() {
            if (held) ink_power_release(INK_NEED_AWAKE);
            held = false;
        }
        bool held = false;
    };
}
