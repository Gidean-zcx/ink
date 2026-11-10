/* Exercise the real board, TCA9555 and PMIC sequence against register writes. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "metalio_plus/metalio_board.h"
#include "metalio_plus/metalio_epd_power.h"

struct osal_mutex { bool held; };
struct hal_i2c_dev { int unused; };
static struct osal_mutex lock;
static struct hal_i2c_dev dev;
static uint8_t regs[8];
static uint8_t rtc_pending; /* the clock chip's interrupt is pending: P16 at its active level */
static uint64_t now;
typedef struct { bool read; uint8_t reg, value; uint64_t at; } transfer_t;
enum { TRANSFER_CAP = 64 };
static transfer_t transfers[TRANSFER_CAP];
static size_t count;
static void record(bool read, uint8_t reg, uint8_t value) {
    assert(count < TRANSFER_CAP);
    transfers[count++] = (transfer_t){read, reg, value, now};
}
static void expect(size_t i, bool read, int reg, int value) {
    assert(i < count);
    assert(transfers[i].read == read && transfers[i].reg == reg && transfers[i].value == value);
}
static void reset_trace(void) { count = 0; now = 0; }
osal_mutex_t *osal_mutex_create(void) { return &lock; }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t us) {
    assert(m == &lock && !m->held && us == OSAL_WAIT_FOREVER);
    m->held = true;
    return OSAL_OK;
}
void osal_mutex_unlock(osal_mutex_t *m) { assert(m->held); m->held = false; }
uint64_t osal_now_us(void) { return now; }
void osal_task_delay_us(uint64_t us) { assert(!lock.held); now += us; }
void osal_task_yield(void) { assert(!lock.held); }
hal_i2c_dev_t *hal_i2c_device(int bus, uint8_t addr, const char *name) {
    assert(bus == 0 && addr == TCA9555_ADDR_BASE && name);
    return &dev;
}
hal_i2c_status_t hal_i2c_write(hal_i2c_dev_t *d, const uint8_t *data, size_t len) {
    assert(d == &dev && len == 2 && data[0] < sizeof regs);
    regs[data[0]] = data[1];
    record(false, data[0], data[1]);
    return HAL_I2C_OK;
}
hal_i2c_status_t hal_i2c_write_read(hal_i2c_dev_t *d, const uint8_t *w, size_t wn, uint8_t *r, size_t rn) {
    assert(d == &dev && wn == 1 && rn == 1 && *w < sizeof regs);
    /* No external input is active: input pins high, output pins follow their latch. */
    *r = *w < 2 ? (uint8_t)(regs[2 + *w] | regs[6 + *w]) : regs[*w];
    if (*w == 1) *r = (rtc_pending != 0) == (MB_RTC_INT_ACTIVE_HIGH != 0) ? (uint8_t)(*r | MB_P1_RTC_INT) : (uint8_t)(*r & ~MB_P1_RTC_INT);
    record(true, *w, *r);
    return HAL_I2C_OK;
}

int main(void) {
    assert(metalio_board_init() == HAL_I2C_OK);
    assert(regs[6] == 0xC0 && regs[7] == 0xE3);
    epd_power_t panel = {.ioexp = metalio_ioexp(), .io_lock = metalio_board_lock, .io_unlock = metalio_board_unlock};
    assert(metalio_board_set(0, 7, 7) == HAL_I2C_OK);
    reset_trace();
    epd_power_sleep(&panel);
    assert(count == 2 && now == 170000);
    expect(0, false, 2, MB_P0_CAM_SCR | EPD_PWR_WAKEUP);
    expect(1, false, 2, MB_P0_CAM_SCR);
    assert(transfers[0].at == 0 && transfers[1].at == 50000);
    reset_trace();
    epd_power_sleep(&panel);
    assert(count == 0 && now == 120000);

    reset_trace();
    assert(metalio_board_mask_for_sleep(true) == HAL_I2C_OK);
    assert(count == 6);
    expect(0, false, 2, 0x20);
    expect(1, false, 6, 0x00);
    expect(2, false, 3, 0x97);
    expect(3, false, 7, 0x00);
    expect(4, true, 0, 0x20);
    expect(5, true, 1, 0x97);
    reset_trace();
    assert(metalio_board_arm_wake(true, true) == HAL_I2C_OK);
    expect(0, false, 7, 0x40);
    expect(1, true, 1, MB_RTC_INT_ACTIVE_HIGH ? 0x97 : 0xD7); /* P16 idle */
    expect(2, false, 7, 0xC0);
    assert(count == 3 && regs[6] == 0 && !(regs[3] & MB_P1_RTC_INT));
    reset_trace();
    assert(metalio_board_cut_rails(false) == HAL_I2C_OK);
    expect(0, false, 2, 0);
    expect(1, false, 3, 0x87);
    assert(count == 2);
    /* The HAL latches P17 before this function; normal directions precede rail restore. */
    reset_trace();
    assert(metalio_board_restore_keys(NULL, NULL) == HAL_I2C_OK);
    expect(0, false, 3, MB_P1_SHARED_RST);
    expect(1, false, 6, 0xC0);
    expect(2, false, 7, 0xE3);
    expect(3, true, 0, 0xC0);
    expect(4, true, 1, MB_RTC_INT_ACTIVE_HIGH ? 0xA7 : 0xE7); /* P16 idle */
    assert(count == 5);
    reset_trace();
    assert(metalio_board_restore_rails() == HAL_I2C_OK);
    expect(0, false, 3, MB_P1_SHARED_RST | MB_P1_SHARED_PWR);
    expect(1, false, 2, MB_P0_CAM_SCR);
    assert(count == 2);
    reset_trace();
    assert(metalio_board_mask_for_sleep(false) == HAL_I2C_OK);
    assert(metalio_board_arm_wake(true, false) == HAL_I2C_OK);
    assert(regs[6] == 0 && regs[7] == 0x80 && regs[3] == 0x97);
    reset_trace();
    assert(metalio_board_cut_rails(true) == HAL_I2C_OK);
    assert(count == 1 && (regs[3] & MB_P1_SHARED_PWR));
    assert(metalio_board_mask_for_sleep(true) == HAL_I2C_OK);
    rtc_pending = true; /* An alarm became active during masking, before the wake input is enabled. */
    reset_trace();
    assert(metalio_board_arm_wake(true, true) == HAL_I2C_ERR);
    assert(count == 2 && regs[7] == 0x40); /* POWER remains masked; the HAL restores normal inputs. */
    puts("s31 board: PMIC delays, safe latches, wake directions and rail order passed");
    return 0;
}
