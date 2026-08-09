#include "rtc_clock.h"

bool RtcClock::begin() { return rtc_.begin(); }

bool RtcClock::lost_power() { return rtc_.lostPower(); }

void RtcClock::now(int* hour, int* minute) {
  DateTime t = rtc_.now();
  *hour = t.hour();
  *minute = t.minute();
}

uint32_t RtcClock::unix_time() { return rtc_.now().unixtime(); }
