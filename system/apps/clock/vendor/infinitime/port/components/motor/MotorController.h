/* clock-3 shim for InfiniTime's MotorController (SOURCE.md): the timer's ringing is its interruption card. StopRinging
 * turns the sound off and leaves the card up; it takes the shared lock, so never call it holding ink_svc_lock. */
#pragma once
#include "ink/sys/interrupt.h"
namespace Pinetime::Controllers {
    class MotorController {
    public:
        void StartRinging() {}
        void StopRinging() {
            if (ringing) ink_interrupt_update(card, nullptr, nullptr, 0, false);
            ringing = false;
        }
        bool IsRinging() { return ringing; }
        int card = 0;
        bool ringing = false;
    };
}
