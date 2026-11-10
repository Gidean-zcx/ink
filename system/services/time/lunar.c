/*
 * The lunar calendar (see ink/sys/time.h): one number per year, in flash.
 *
 *   bits 0..3   the leap month (0 none)
 *   bit  4      the leap month has 30 days
 *   bits 5..16  month n (1..12) has 30 days: bit 4 + n
 *   bits 17..22 new year's day, counted from January 1 = 1 (Feb 1 = 32)
 *
 * The numbers are generated from Apple's Chinese calendar (Foundation, Asia/Shanghai, 2026-11-04;
 * tests/fixtures/lunar/chinese_years_apple.txt; two of its months read length 0 at their last day, in 2057 and
 * 2097: they have 30 days, as the sums and the second table show). A second, independently kept table agrees on
 * 195 of the 200 years; the five others (1914-16, 1920, 2060) differ by which of two months is long, as almanacs
 * of different methods do. No year from 1921 to 2059 is in doubt.
 */
#include <stdio.h>
#include <string.h>

#include "ink/sys/time.h"
#include "ink/sys/i18n.h"

#define FIRST_YEAR 1901
#define LAST_YEAR 2100

static const uint32_t YEARS[LAST_YEAR - FIRST_YEAR + 1] = {
    0x64EA40, 0x4FD4A0, 0x3B6545, 0x5EC960, 0x475360, 0x3354D4, 0x58AD40, 0x436B20,
    0x2D7542, 0x52EA40, 0x3DB4A6, 0x6364A0, 0x4B4960, 0x352975, 0x5B55A0, 0x46AD40,
    0x2EB6A2, 0x55B520, 0x41D257, 0x67B240, 0x4FA4A0, 0x39A5A5, 0x5F4AC0, 0x4856C0,
    0x315AB4, 0x58DA80, 0x43D520, 0x2FE942, 0x53D240, 0x3CD4C6, 0x60A560, 0x4B4AE0,
    0x352AD5, 0x5B6B40, 0x46DA80, 0x30EC33, 0x54E920, 0x3F6277, 0x655260, 0x4EA560,
    0x36A376, 0x5D55A0, 0x48AD40, 0x33B4B4, 0x597480, 0x436920, 0x2DA962, 0x5352A0,
    0x3B55A7, 0x60A6C0, 0x4B55A0, 0x375955, 0x5AB640, 0x45B4A0, 0x31D453, 0x57A940,
    0x3EB2A8, 0x6352E0, 0x4EAAC0, 0x38AEA6, 0x5D5AA0, 0x48DA40, 0x32EAA4, 0x59D4A0,
    0x42C940, 0x2AC9E3, 0x515360, 0x3D5B47, 0x60AD40, 0x4B6D20, 0x377645, 0x5D6A40,
    0x4564A0, 0x2F6564, 0x554960, 0x3F5568, 0x6255A0, 0x4CADA0, 0x38B536, 0x5FB520,
    0x49B240, 0x33D2A4, 0x59A4A0, 0x43C9AA, 0x674AC0, 0x5056C0, 0x3A5EA6, 0x60DAA0,
    0x4BD520, 0x37EA45, 0x5DD240, 0x47A4C0, 0x2EA5C3, 0x534AE0, 0x3F5AC8, 0x646B40,
    0x4CDAA0, 0x396D25, 0x5EE920, 0x48D260, 0x315364, 0x56A560, 0x414B60, 0x2D55C2,
    0x50AD40, 0x3BBAA7, 0x637480, 0x4D6920, 0x35AA65, 0x5B52A0, 0x44A5A0, 0x2EABA4,
    0x5356A0, 0x3F7549, 0x64BA40, 0x4FB4A0, 0x39D156, 0x5FA940, 0x4992A0, 0x3353C4,
    0x56AAC0, 0x4156A0, 0x2D5B42, 0x52DA40, 0x3AECA6, 0x60E4A0, 0x4AC960, 0x34CAE5,
    0x599560, 0x44AB40, 0x2EADC3, 0x556D20, 0x3FEA4B, 0x656A40, 0x4F64A0, 0x39A176,
    0x5D4960, 0x469560, 0x305765, 0x56B5A0, 0x416D40, 0x2DB542, 0x53B240, 0x3DD4A7,
    0x61A4A0, 0x4B4AA0, 0x3549B5, 0x5A96C0, 0x42B6A0, 0x2EDA53, 0x55D920, 0x41F248,
    0x65D240, 0x4FA4C0, 0x38A2D6, 0x5D4AE0, 0x469AC0, 0x306CB4, 0x56EAA0, 0x42E920,
    0x2AE963, 0x50D260, 0x3B5567, 0x60A560, 0x494B60, 0x355745, 0x5AAD40, 0x456CA0,
    0x2F7544, 0x556940, 0x3FB2A8, 0x6552A0, 0x4CA5A0, 0x36ADA6, 0x5D56A0, 0x48B540,
    0x30BAA4, 0x57B4A0, 0x43A940, 0x2DC9A3, 0x5192C0, 0x3B99C7, 0x60AAC0, 0x4B56A0,
    0x355A55, 0x5ADA40, 0x45D4A0, 0x30E544, 0x52D160, 0x3CD2E8, 0x629560, 0x4CAB60,
    0x36AAD6, 0x5D6D40, 0x48EA40, 0x3372A4, 0x5764A0, 0x415160, 0x2B49E2, 0x509560,
};

static int64_t days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static void civil_from_days(int64_t z, int *y, int *m, int *d) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int yy = (int)yoe + (int)(era * 400);
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = yy + (*m <= 2);
}

static int64_t new_year(int y) { /* days since 1970 */
    return days_from_civil(y, 1, 1) + (int64_t)((YEARS[y - FIRST_YEAR] >> 17) & 63) - 1;
}

int ink_lunar_leap_month(int y) {
    if (y < FIRST_YEAR || y > LAST_YEAR) return 0;
    return (int)(YEARS[y - FIRST_YEAR] & 15);
}

int ink_lunar_month_days(int y, int m, bool leap) {
    if (y < FIRST_YEAR || y > LAST_YEAR || m < 1 || m > 12) return 0;
    uint32_t v = YEARS[y - FIRST_YEAR];
    if (leap) return (int)(v & 15) == m ? 29 + (int)((v >> 4) & 1) : 0;
    return 29 + (int)((v >> (4 + m)) & 1);
}

bool ink_lunar_from_solar(int y, int m, int d, ink_lunar_t *out) {
    if (y < FIRST_YEAR || y > LAST_YEAR + 1 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    int64_t day = days_from_civil(y, m, d);
    int ly = y;
    if (ly > LAST_YEAR || day < new_year(ly)) ly--;
    if (ly < FIRST_YEAR) return false;
    int64_t left = day - new_year(ly);
    int leap = ink_lunar_leap_month(ly);
    for (int mo = 1; mo <= 12; mo++) {
        int n = ink_lunar_month_days(ly, mo, false);
        if (left < n) {
            *out = (ink_lunar_t){ly, mo, (int)left + 1, false};
            return true;
        }
        left -= n;
        if (mo == leap) {
            n = ink_lunar_month_days(ly, mo, true);
            if (left < n) {
                *out = (ink_lunar_t){ly, mo, (int)left + 1, true};
                return true;
            }
            left -= n;
        }
    }
    return false; /* past the last year of the table */
}

bool ink_lunar_to_solar(const ink_lunar_t *l, int *y, int *m, int *d) {
    if (l->year < FIRST_YEAR || l->year > LAST_YEAR || l->day < 1 || l->day > ink_lunar_month_days(l->year, l->month, l->leap)) return false;
    int64_t day = new_year(l->year);
    int leap = ink_lunar_leap_month(l->year);
    for (int mo = 1; mo < l->month; mo++) {
        day += ink_lunar_month_days(l->year, mo, false);
        if (mo == leap) day += ink_lunar_month_days(l->year, mo, true);
    }
    if (l->leap) day += ink_lunar_month_days(l->year, l->month, false);
    civil_from_days(day + l->day - 1, y, m, d);
    return true;
}

int ink_lunar_name(const ink_lunar_t *l, char *out, size_t cap) {
    static const char *const MONTH[] = {"正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊"};
    static const char *const DIGIT[] = {"一", "二", "三", "四", "五", "六", "七", "八", "九", "十"};
    static const char *const TENS[] = {"初", "十", "廿", "三"};
    if (l->month < 1 || l->month > 12 || l->day < 1 || l->day > 30) return -1;
    if (strcmp(ink_lang_code(), "zh")) {
        int n = snprintf(out, cap, TR("农历 %s%d月%d日"), l->leap ? TR("闰") : "", l->month, l->day);
        return n > 0 && (size_t)n < cap ? n : -1;
    }
    char day[16];
    if (l->day == 10) snprintf(day, sizeof day, "初十");
    else if (l->day == 20) snprintf(day, sizeof day, "二十");
    else if (l->day == 30) snprintf(day, sizeof day, "三十");
    else snprintf(day, sizeof day, "%s%s", TENS[l->day / 10], DIGIT[l->day % 10 - 1]);
    int n = snprintf(out, cap, "%s%s月%s", l->leap ? "闰" : "", MONTH[l->month - 1], day);
    return n > 0 && (size_t)n < cap ? n : -1;
}

const char *ink_lunar_zodiac(int year) {
    static const char *const Z[] = {TR_NOOP("鼠"), TR_NOOP("牛"), TR_NOOP("虎"), TR_NOOP("兔"), TR_NOOP("龙"), TR_NOOP("蛇"), TR_NOOP("马"), TR_NOOP("羊"), TR_NOOP("猴"), TR_NOOP("鸡"), TR_NOOP("狗"), TR_NOOP("猪")};
    return TR(Z[(((year - 4) % 12) + 12) % 12]);
}

int ink_lunar_ganzhi(int year, char *out, size_t cap) {
    static const char *const GAN[] = {TR_NOOP("甲"), TR_NOOP("乙"), TR_NOOP("丙"), TR_NOOP("丁"), TR_NOOP("戊"), TR_NOOP("己"), TR_NOOP("庚"), TR_NOOP("辛"), TR_NOOP("壬"), TR_NOOP("癸")};
    static const char *const ZHI[] = {TR_NOOP("子"), TR_NOOP("丑"), TR_NOOP("寅"), TR_NOOP("卯"), TR_NOOP("辰"), TR_NOOP("巳"), TR_NOOP("午"), TR_NOOP("未"), TR_NOOP("申"), TR_NOOP("酉"), TR_NOOP("戌"), TR_NOOP("亥")};
    int n = snprintf(out, cap, "%s%s", TR(GAN[(((year - 4) % 10) + 10) % 10]), TR(ZHI[(((year - 4) % 12) + 12) % 12]));
    return n > 0 && (size_t)n < cap ? n : -1;
}
