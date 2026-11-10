/* Observe the HAL argument, then run the unchanged simulator motor implementation. */
#define hal_haptic_pulse ink_test_motor_pulse
#include "../ports/sim/hal/hal_power_sim.c"
#undef hal_haptic_pulse

int ink_test_pulse_ms;
unsigned ink_test_pulses;
void hal_haptic_pulse(int ms) {
    ink_test_pulse_ms = ms;
    ink_test_pulses++;
    ink_test_motor_pulse(ms);
}
