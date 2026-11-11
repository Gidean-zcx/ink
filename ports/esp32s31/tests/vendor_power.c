/* Reuse the board recorder; all register values and time are synthetic. */
#define main ink_vendor_board_cases_main
#include "vendor_board.c"
#undef main

extern void ink_bq_ResetFilter(void);

static void battery(int mv, int ma, int fake_soc) {
    regs[0x55][0x08] = (uint8_t)mv;
    regs[0x55][0x09] = (uint8_t)((unsigned)mv >> 8);
    regs[0x55][0x0c] = (uint8_t)ma;
    regs[0x55][0x0d] = (uint8_t)((uint16_t)ma >> 8);
    regs[0x55][0x2c] = (uint8_t)fake_soc;
}
static void gauge_curve(void) {
    hal_battery_t b;
    battery(3702, -100, 99);
    advance(3000000);
    hal_power_battery(&b);
    assert(b.percent == 50 && b.millivolts == 3702 && !b.charging && !b.external_power);
    battery(3702, -100, 0);
    hal_power_battery(&b);
    assert(b.percent == 50);
    /* An unreadable SOC command cannot gate a voltage/current sample. */
    gauge_soc_unreadable = true;
    hal_power_battery(&b);
    assert(b.percent == 50);
    battery(3300, -100, 99);
    int previous = b.percent;
    for (unsigned i = 0; i < 61; ++i) {
        advance(1000000);
        hal_power_battery(&b);
        assert(b.percent <= previous && previous - b.percent <= 5);
        previous = b.percent;
    }
    battery(4300, 100, 0);
    ink_bq_ResetFilter();
    advance(3000000);
    hal_power_battery(&b);
    assert(b.percent == 95);
    for (unsigned i = 0; i < 29; ++i) {
        advance(1000000);
        hal_power_battery(&b);
        assert(b.percent == 95);
    }
    advance(1000000);
    hal_power_battery(&b);
    assert(b.percent == 100 && b.external_power);
    puts("C03: voltage curve, signed current, monotonic steps and 30 s full hold passed");
}
static void rtc(void) {
    const uint8_t calendar[] = {0x01, 0x02, 0x03, 0x11, 0x03, 0x11, 0x26};
    memcpy(&regs[0x51][2], calendar, sizeof calendar);
    regs[0x51][0] = 0x20;
    int64_t epoch = -1;
    assert(hal_rtc_get(&epoch) && !(regs[0x51][0] & 0x20));
    int64_t good = epoch;
    regs[0x51][2] |= 0x80;
    epoch = -1;
    assert(!hal_rtc_get(&epoch) && epoch == -1);
    assert(hal_rtc_set(good) && !(regs[0x51][2] & 0x80));
    assert(hal_rtc_get(&epoch) && epoch == good);
    regs[0x51][7] = 0;
    assert(!hal_rtc_get(&epoch));
    regs[0x51][7] = 0x11;
    regs[0x51][4] = 0x24;
    assert(!hal_rtc_get(&epoch));
    regs[0x51][4] = 0x03;
    regs[0x51][1] = 0x1f;
    hal_rtc_clear_alarm();
    assert(regs[0x51][1] == 0x11);
    for (unsigned i = 9; i < 13; ++i) assert(regs[0x51][i] == 0x80);
    assert(hal_rtc_set_alarm(good) && (regs[0x51][1] & 2));
    puts("P2: STOP, calendar, untrusted VL, reliable write and AF/TF clearing passed");
}

static void c02_hal(void) {
    battery(4200, 0, 99);
    advance(3000000);
    regs[0x6b][0x1e] = 3;
    assert(hal_power_charger_poll() == 200 && !(regs[0x6b][0x16] & 0x20));
    present[0x55] = false;
    advance(200000);
    uint32_t next = hal_power_charger_poll();
    if (next != 500 || !(regs[0x6b][0x16] & 0x20)) {
        fprintf(stderr, "C02 BLOCKED: next=%u ms EN_CHG=%d; board gauge early return prevents restore at 200 ms\n",
                (unsigned)next, (regs[0x6b][0x16] & 0x20) != 0);
        exit(1);
    }
}
int main(int argc, char **argv) {
    assert(argc == 2);
    fixture();
    battery(3702, -100, 99);
    ink_s31_board_init();
    if (!strcmp(argv[1], "gauge")) gauge_curve();
    else if (!strcmp(argv[1], "rtc")) rtc();
    else if (!strcmp(argv[1], "c02-hal")) c02_hal();
    else assert(false);
    return 0;
}
