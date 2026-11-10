/* Port-private input/sleep handover. The input task owns all event delivery and motor writes. */
#ifndef INK_S31_IO_H
#define INK_S31_IO_H

#include "ink/hal_i2c.h"
#include "ink/hal_power.h"

void s31_input_init(hal_i2c_dev_t *touch);
void s31_input_pause(void); /* holds the sampling lock until resume */
void s31_input_resume(uint32_t wake, uint64_t t, bool keys_back);
bool s31_haptic_busy(void);
bool s31_accel_take_motion(void);
bool s31_accel_sleep(bool watch);
void s31_accel_resume(void);
void s31_power_init(void);
void ink_s31_audio_init(void);
int ink_s31_sleep_init(void);
int ink_s31_light_sleep(void);

#endif
