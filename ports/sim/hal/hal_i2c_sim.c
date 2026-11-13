/*
 * hal_i2c on the simulated shared bus.
 *
 * Each transfer holds the bus for its bit time (sim_i2c_transfer, so it
 * contends with touch and fuel-gauge reads) and is answered by the
 * register-level chip model attached at its address; no model means NACK.
 * --i2c-faults RATE turns that share of transfers into timeouts that hold
 * the bus for the driver timeout, as seen on the device (phase 0, factory
 * firmware: "pca9555_set_value: ESP_ERR_TIMEOUT" followed by a PMIC retry).
 */
#include <stdio.h>

#include "ink/hal_i2c.h"
#include <string.h>

#include "sim.h"

#define MAX_MODELS 8
#define MAX_DEVS 16
#define TIMEOUT_US 50000 /* upstream passes 50 ms to its expander transfers */

struct hal_i2c_dev {
    int bus;
    uint8_t addr;
    char name[24];
    uint32_t fault_rng; /* own fault stream: adding a device does not shift another's faults */
};

static struct {
    int bus;
    uint8_t addr;
    const sim_i2c_model_t *model;
    void *state;
} MODELS[MAX_MODELS];
static int n_models;
static struct hal_i2c_dev DEVS[MAX_DEVS];
static int n_devs;
static double fault_rate;

void sim_i2c_attach(int bus, uint8_t addr7, const sim_i2c_model_t *m, void *state) {
    if (n_models == MAX_MODELS) sim_fatal("i2c: too many chip models");
    MODELS[n_models].bus = bus;
    MODELS[n_models].addr = addr7;
    MODELS[n_models].model = m;
    MODELS[n_models].state = state;
    n_models++;
}

static const char *fault_dev; /* NULL: every device */
void sim_i2c_set_faults(double timeout_rate, const char *device) {
    fault_rate = timeout_rate;
    fault_dev = device;
}

hal_i2c_dev_t *hal_i2c_device(int bus, uint8_t addr7, const char *name) {
    for (int i = 0; i < n_devs; i++)
        if (DEVS[i].bus == bus && DEVS[i].addr == addr7) return &DEVS[i];
    if (n_devs == MAX_DEVS) sim_fatal("i2c: too many devices");
    struct hal_i2c_dev *d = &DEVS[n_devs++];
    d->bus = bus;
    d->addr = addr7;
    d->fault_rng = (g_sim.seed ? g_sim.seed : 1u) * 2654435761u ^ (0x9E3779B9u * (uint32_t)(addr7 + 1)) ^ (uint32_t)bus;
    if (!d->fault_rng) d->fault_rng = 0x2545F491u;
    sim_copy_name(d->name, sizeof d->name, name);
    return d;
}

static double fault_draw(hal_i2c_dev_t *d) { /* xorshift32 per device */
    uint32_t x = d->fault_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    d->fault_rng = x;
    return (double)(x >> 8) / (double)(1u << 24);
}

static hal_i2c_status_t xfer(hal_i2c_dev_t *d, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    /* no random draw unless faults are on: other random streams stay as they were */
    if (fault_rate > 0 && (!fault_dev || !strcmp(fault_dev, d->name)) && fault_draw(d) < fault_rate) {
        sim_i2c_hold(d->name, SIM_US(TIMEOUT_US));
        sim_metric_add("i2c.timeouts", 1);
        return HAL_I2C_TIMEOUT;
    }
    uint64_t t0 = sim_i2c_transfer_at(d->name, 1 + (int)wlen + (rlen ? 1 + (int)rlen : 0));
    for (int i = 0; i < n_models; i++)
        if (MODELS[i].bus == d->bus && MODELS[i].addr == d->addr) {
            sim_i2c_xfer_t0 = t0; /* nothing blocks between here and the model: it is this transfer's */
            hal_i2c_status_t s = (hal_i2c_status_t)MODELS[i].model->xfer(MODELS[i].state, w, wlen, r, rlen);
            if (s == HAL_I2C_NACK) sim_metric_add("i2c.nacks", 1);
            return s;
        }
    sim_metric_add("i2c.nacks", 1);
    return HAL_I2C_NACK;
}

hal_i2c_status_t hal_i2c_write(hal_i2c_dev_t *dev, const uint8_t *data, size_t len) {
    return xfer(dev, data, len, NULL, 0);
}

hal_i2c_status_t hal_i2c_write_read(hal_i2c_dev_t *dev, const uint8_t *wdata, size_t wlen, uint8_t *rdata,
                                    size_t rlen) {
    return xfer(dev, wdata, wlen, rdata, rlen);
}

const char *hal_i2c_status_name(hal_i2c_status_t s) {
    switch (s) {
    case HAL_I2C_OK: return "ok";
    case HAL_I2C_NACK: return "nack";
    case HAL_I2C_TIMEOUT: return "timeout";
    case HAL_I2C_ERR: return "error";
    }
    return "?";
}
