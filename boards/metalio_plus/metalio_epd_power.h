/*
 * Panel power on the Metalio E-Ink4 Plus. The TPS65185's WAKEUP, PWRUP and
 * VCOM_CTRL inputs and its PWR_GOOD output sit on TCA9555 port 0.
 *
 * Written from the TPS65185 datasheet; the order, register values and waits
 * follow the upstream board code so the device sees the sequence the factory
 * firmware uses (CloudZao/Metalio-E-INK4-Plus @690a023,
 * main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c:
 * epd_board_poweron_once, epd_board_poweron, epd_board_poweroff,
 * epd_board_metalio_eink4_plus_tps_enter_sleep). No code was copied. One
 * change: after the last retry upstream restarts the chip; here the caller
 * gets the error, so the system can enter safe mode instead.
 */
#ifndef INK_METALIO_EPD_POWER_H
#define INK_METALIO_EPD_POWER_H

#include <stdbool.h>

#include "tca9555/tca9555.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EPD_PWR_VCOM_CTRL 0x01 /* TCA9555 P00 */
#define EPD_PWR_PWRUP 0x02     /* P01 */
#define EPD_PWR_WAKEUP 0x04    /* P02 */
#define EPD_PWR_GOOD 0x80      /* P07, input: TPS65185 PWR_GOOD */
#define EPD_PWR_SCREEN 0x20    /* P05: camera / screen power (upstream EPD_IO_CAM_SCR_EN; MB_P0_CAM_SCR) */
#define EPD_PWR_RETRIES 3      /* full power cycles after a failed first attempt */

/* VCOM of the E0470A14-AF panel on this board: upstream config.h EPD_VCOM_MV; the factory firmware
 * logs "VCOM=1200 mV" at boot (docs/hardware/2026-11-03-phase0.md). */
#define METALIO_EPD_VCOM_MV 1200

typedef enum {
    EPD_POWER_OK = 0,
    EPD_POWER_NO_PMIC,       /* the PMIC did not answer after WAKEUP */
    EPD_POWER_CONFIG_FAILED, /* REVID or UPSEQ access failed */
    EPD_POWER_NO_PWR_GOOD,   /* PWR_GOOD did not rise after PWRUP */
    EPD_POWER_RAILS_FAILED,  /* ENABLE or VCOM write failed, or the PG register never read all good */
} epd_power_result_t;

typedef struct {
    tca9555_t *ioexp;
    /* The expander is shared (motor, amplifier, shared rails on the same ports): each expander
     * transaction runs between io_lock and io_unlock, short, never across the sequence's waits.
     * May be NULL when nothing else uses the expander. */
    void (*io_lock)(void);
    void (*io_unlock)(void);
    hal_i2c_dev_t *pmic;
    unsigned vcom_mv;
    /* Gate driver output enable (EPD XOE, a GPIO). The panel needs it low at
     * least 12 us before VPOS/VNEG go off. May be NULL. */
    void (*gate_output)(void *ctx, bool on);
    void *ctx;
    uint8_t revid;         /* read during the last power-up */
    unsigned attempts;     /* power-up attempts since init */
    unsigned retries;      /* attempts that were retries */
    unsigned ioexp_errors; /* failed expander writes; the sequence goes on, as upstream */
} epd_power_t;

/** Rails and VCOM up, gate output enabled; retries a failed attempt up to EPD_PWR_RETRIES times. */
epd_power_result_t epd_power_on(epd_power_t *p);

/** Gate output off, rails down, PMIC to SLEEP. */
void epd_power_off(epd_power_t *p);

/** Before deep sleep: PMIC to SLEEP with the datasheet waits. */
void epd_power_sleep(epd_power_t *p);

/** Are the rails and VCOM good now? PWR_GOOD (P07) through the expander, then the PMIC's PG
 *  register; false if either says no or does not answer after three tries (a bus timeout is not a
 *  supply loss; a PMIC that lost WAKEUP does not answer at all). */
bool epd_power_good(epd_power_t *p);

/** Has anyone changed the PMIC's pins (WAKEUP, PWRUP, VCOM_CTRL) or the screen's power (P05) since
 *  epd_power_on brought the rails up? Even a blip that came back cut or restarted the rails, VCOM or
 *  the screen's supply. No bus traffic. */
bool epd_power_pins_touched(epd_power_t *p);

/** At the end of a waveform: did the panel keep its supply all the way? The pins untouched since
 *  epd_power_on and the rails and VCOM good now (epd_power_good). False: the panel did not get the
 *  whole waveform. A drop of PWR_GOOD that came back without anyone touching the pins is only seen
 *  by whoever serves the expander's INT meanwhile (the display port counts those). */
bool epd_power_kept(epd_power_t *p);

const char *epd_power_result_name(epd_power_result_t r);

#ifdef __cplusplus
}
#endif
#endif
