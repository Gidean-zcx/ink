/* clock-3: our declaration of InfiniTime's Widgets::Counter without LVGL (SOURCE.md). The copied UpBtnPressed,
 * DownBtnPressed and SetValue use it; UpdateLabel is the glue's: it redraws that one number. */
#pragma once
namespace Pinetime::Applications::Widgets {
    class Counter {
    public:
        Counter(int low, int high) : min {low}, max {high}, value {low} {}
        void UpBtnPressed();
        void DownBtnPressed();
        void SetValue(int newValue);
        int GetValue() const { return value; }
        void UpdateLabel();
        void (*ValueChangedHandler)(void* userData) = nullptr;
        int min, max, value;
        void* userData = nullptr;
    };
}
