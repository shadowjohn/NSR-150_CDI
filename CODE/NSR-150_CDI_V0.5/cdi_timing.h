#ifndef NSR150_CDI_TIMING_H
#define NSR150_CDI_TIMING_H

#include <stdint.h>

namespace NSR150CDI {

static const uint32_t kMinimumRpm = 100UL;
static const uint32_t kMaximumRpm = 17000UL;
static const uint32_t kRcEnableRpm = 5000UL;
static const uint32_t kSparkPulseWidthUs = 200UL;
static const uint32_t kFullAdvanceMilliDegrees = 65000UL;
static const uint32_t kAdvanceTableLastRpm = 15000UL;
static const uint32_t kMinimumPulseIntervalUs = 60000000UL / kMaximumRpm;
static const uint32_t kMaximumPulseIntervalUs = 60000000UL / kMinimumRpm;

// 保留 V0.4 實車使用的點火表，單位改為千分之一度以避開 ISR 浮點運算。
static const uint16_t kAdvanceTableMilliDegrees[] = {
  10000, 12000, 12000, 25000,
  26000, 23000, 20000, 16000,
  13000, 9000,  9000,  9000,
  8000,  8000,  8000,  8000
};

struct Timing {
  uint32_t rpm;
  uint32_t advanceMilliDegrees;
  uint32_t fireDelayUs;
  bool rcEnabled;
};

inline bool isValidRpm(uint32_t rpm) {
  return rpm >= kMinimumRpm && rpm <= kMaximumRpm;
}

inline bool isValidPulseIntervalUs(uint32_t pulseIntervalUs) {
  return pulseIntervalUs >= kMinimumPulseIntervalUs &&
    pulseIntervalUs <= kMaximumPulseIntervalUs;
}

inline uint32_t calculateRpmFromPulseIntervalUs(uint32_t pulseIntervalUs) {
  if (!isValidPulseIntervalUs(pulseIntervalUs)) {
    return 0;
  }

  const uint32_t calculatedRpm = 60000000UL / pulseIntervalUs;

  // micros() 的一微秒量化會讓實際 17,000 rpm 落在 3,529 us，
  // 因此只在合法脈衝區間內將計算值飽和至既定限轉值。
  return calculatedRpm > kMaximumRpm ? kMaximumRpm : calculatedRpm;
}

inline uint32_t calculateAdvanceMilliDegrees(uint32_t rpm) {
  if (rpm >= kAdvanceTableLastRpm) {
    return kAdvanceTableMilliDegrees[15];
  }

  const uint32_t index = rpm / 1000UL;
  const uint32_t remainder = rpm % 1000UL;
  const int32_t lower = kAdvanceTableMilliDegrees[index];
  const int32_t upper = kAdvanceTableMilliDegrees[index + 1UL];

  return static_cast<uint32_t>(
    lower + ((static_cast<int32_t>(remainder) * (upper - lower)) / 1000L)
  );
}

inline Timing calculateTiming(uint32_t rpm) {
  Timing timing;
  timing.rpm = rpm;
  timing.advanceMilliDegrees = calculateAdvanceMilliDegrees(rpm);
  timing.fireDelayUs = static_cast<uint32_t>(
    (60000000ULL * (kFullAdvanceMilliDegrees - timing.advanceMilliDegrees)) /
    (static_cast<uint64_t>(rpm) * 360000ULL)
  );
  timing.rcEnabled = rpm >= kRcEnableRpm;
  return timing;
}

}  // namespace NSR150CDI

#endif
