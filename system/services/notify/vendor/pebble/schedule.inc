/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

static uint8_t prv_next_days(uint8_t days) {
  return ((days << 1) | (days >> (PBL_DAY_PER_WEEK - 1))) & PBL_CRON_WDAY_ANY;
}

static int prv_minutes(uint8_t hour, uint8_t minute) {
  return hour * PBL_MIN_PER_HOUR + minute;
}

static bool prv_is_in_slot_period(const DoNotDisturbScheduleSlot *slot, const struct pbl_tm *now) {
  const DoNotDisturbSchedule *schedule = &slot->schedule;
  const int from = prv_minutes(schedule->from_hour, schedule->from_minute);
  const int to = prv_minutes(schedule->to_hour, schedule->to_minute);
  const int minute = prv_minutes(now->tm_hour, now->tm_min);
  const uint8_t days = do_not_disturb_schedule_slot_get_days(slot);
  const uint8_t today = 1 << now->tm_wday;

  if (from < to) {
    return (days & today) && minute >= from && minute < to;
  }
  if (from == to) {
    return false;
  }
  if (slot->legacy) {
    return (days & today) && (minute >= from || minute < to);
  }
  // After midnight, the period started the day before
  return ((days & today) && minute >= from) || ((prv_next_days(days) & today) && minute < to);
}

