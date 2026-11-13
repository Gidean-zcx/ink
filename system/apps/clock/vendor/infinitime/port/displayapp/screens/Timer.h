/* clock-3: our declaration of InfiniTime's Screens::Timer without LVGL (SOURCE.md): only what the copied Refresh,
 * DisplayTime, ToggleRunning and Reset use, plus the hours InfiniTime does not have. SetTimerRunning, SetTimerStopped
 * and SetTimerRinging are the glue's (clock_infinitime.cpp). One per timer. */
#pragma once
#include <chrono>
#include "components/motor/MotorController.h"
#include "components/timer/Timer.h"
#include "displayapp/widgets/Counter.h"
#include "systemtask/WakeLock.h"
#include "utility/DirtyValue.h"
namespace Pinetime::Applications::Screens {
    class Timer {
    public:
        Timer(Controllers::Timer& timerController, Controllers::MotorController& motor) : timer {timerController}, motorController {motor} {}
        void Refresh();
        void Reset();
        void ToggleRunning();
        void SetTimerRinging();
        void SetTimerRunning();
        void SetTimerStopped();
        void DisplayTime();
        Pinetime::Controllers::Timer& timer;
        Pinetime::Controllers::MotorController& motorController;
        Pinetime::System::WakeLock wakeLock;
        Widgets::Counter hourCounter = Widgets::Counter(0, 23);
        Widgets::Counter minuteCounter = Widgets::Counter(0, 59);
        Widgets::Counter secondCounter = Widgets::Counter(0, 59);
        Utility::DirtyValue<std::chrono::seconds> displaySeconds;
    };
}
