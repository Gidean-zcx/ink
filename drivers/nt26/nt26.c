/*
 * NT26 SIM selection (see nt26.h). Timeouts and the 500 ms pauses between commands follow the
 * official settings page (settings_network_tab.cc @690a023); unlike it, nothing reboots
 * afterwards: the driver waits for the new card's registration instead.
 */
#include "nt26/nt26.h"

#include <stdio.h>
#include <string.h>

#include "ink/hal_net.h"
#include "ink/osal.h"

#define NT26_CFUN0_TIMEOUT_MS 8000
#define NT26_SLOT_TIMEOUT_MS 5000
#define NT26_CFUN1_TIMEOUT_MS 15000
#define NT26_QUERY_TIMEOUT_MS 5000
#define NT26_STEP_PAUSE_US 500000
#define NT26_REG_POLL_US 500000

static nt26_result_t at(const char *cmd, char *resp, size_t cap, uint32_t timeout_ms) {
    hal_at_result_t r = hal_modem_at(cmd, resp, cap, timeout_ms);
    return r == HAL_AT_OK ? NT26_OK : r == HAL_AT_NO_MODEM ? NT26_NO_MODEM : NT26_AT_FAILED;
}

nt26_result_t nt26_sim_slot(nt26_slot_t *slot) {
    char resp[64];
    nt26_result_t r = at("AT+ECSIMCFG?", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    const char *p = strstr(resp, "\"SimSlot\"");
    int v = -1;
    if (!p || sscanf(p + strlen("\"SimSlot\""), " ,%d", &v) != 1 || (v != 0 && v != 1)) return NT26_AT_FAILED;
    *slot = (nt26_slot_t)v;
    return NT26_OK;
}

nt26_result_t nt26_registered(bool *registered) {
    char resp[64];
    nt26_result_t r = at("AT+CEREG?", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    int n, stat;
    const char *p = strstr(resp, "+CEREG:");
    if (!p || sscanf(p, "+CEREG: %d,%d", &n, &stat) != 2) return NT26_AT_FAILED;
    *registered = stat == 1 || stat == 5;
    return NT26_OK;
}

nt26_result_t nt26_select_sim(nt26_slot_t slot, uint32_t reg_timeout_ms) {
    char resp[64], cmd[40];
    nt26_result_t r = at("AT+CFUN=0", resp, sizeof resp, NT26_CFUN0_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    osal_task_delay_us(NT26_STEP_PAUSE_US);
    snprintf(cmd, sizeof cmd, "AT+ECSIMCFG=SimSlot,%d", (int)slot);
    r = at(cmd, resp, sizeof resp, NT26_SLOT_TIMEOUT_MS);
    if (r != NT26_OK) { /* leave the radio on, on the old card */
        at("AT+CFUN=1", resp, sizeof resp, NT26_CFUN1_TIMEOUT_MS);
        return r;
    }
    osal_task_delay_us(NT26_STEP_PAUSE_US);
    r = at("AT+CFUN=1", resp, sizeof resp, NT26_CFUN1_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    uint64_t deadline = osal_now_us() + (uint64_t)reg_timeout_ms * 1000u;
    for (;;) {
        bool reg = false;
        if (nt26_registered(&reg) == NT26_OK && reg) return NT26_OK;
        if (osal_now_us() >= deadline) return NT26_NOT_REGISTERED;
        osal_task_delay_us(NT26_REG_POLL_US);
    }
}

const char *nt26_result_name(nt26_result_t r) {
    switch (r) {
    case NT26_OK: return "ok";
    case NT26_NOT_REGISTERED: return "not registered";
    case NT26_AT_FAILED: return "AT failed";
    case NT26_NO_MODEM: return "no modem";
    }
    return "?";
}
