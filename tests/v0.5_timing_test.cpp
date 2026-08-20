#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "../CODE/NSR-150_CDI_V0.5/cdi_timing.h"

namespace {

void expectEqual(const char* name, std::uint32_t actual, std::uint32_t expected) {
  if (actual != expected) {
    std::cerr << name << ": expected " << expected << ", actual " << actual << "\n";
    std::exit(1);
  }
}

void expectTiming(std::uint32_t rpm, std::uint32_t expectedAdvanceMilliDegrees,
                  std::uint32_t expectedDelayUs, bool expectedRcEnabled) {
  const NSR150CDI::Timing timing = NSR150CDI::calculateTiming(rpm);

  expectEqual("advance", timing.advanceMilliDegrees, expectedAdvanceMilliDegrees);
  expectEqual("delay", timing.fireDelayUs, expectedDelayUs);
  expectEqual("RC enabled", timing.rcEnabled ? 1U : 0U, expectedRcEnabled ? 1U : 0U);
}

}  // namespace

int main() {
  expectEqual("99 rpm is rejected", NSR150CDI::isValidRpm(99) ? 1U : 0U, 0U);
  expectEqual("100 rpm is accepted", NSR150CDI::isValidRpm(100) ? 1U : 0U, 1U);
  expectEqual("17000 rpm is accepted", NSR150CDI::isValidRpm(17000) ? 1U : 0U, 1U);
  expectEqual("17001 rpm is rejected", NSR150CDI::isValidRpm(17001) ? 1U : 0U, 0U);
  expectEqual("3528 us pulse is rejected", NSR150CDI::isValidPulseIntervalUs(3528) ? 1U : 0U, 0U);
  expectEqual("3529 us pulse is accepted", NSR150CDI::isValidPulseIntervalUs(3529) ? 1U : 0U, 1U);
  expectEqual("3529 us pulse is clamped to 17000 rpm", NSR150CDI::calculateRpmFromPulseIntervalUs(3529), 17000U);
  expectEqual("3530 us pulse produces 16997 rpm", NSR150CDI::calculateRpmFromPulseIntervalUs(3530), 16997U);

  expectTiming(2500, 18500, 3100, false);
  expectTiming(4500, 24500, 1500, false);
  expectTiming(14999, 8000, 633, true);
  expectTiming(15000, 8000, 633, true);
  expectTiming(17000, 8000, 558, true);

  expectEqual("RC is off below 5000 rpm", NSR150CDI::calculateTiming(4999).rcEnabled ? 1U : 0U, 0U);
  expectEqual("RC is on from 5000 rpm", NSR150CDI::calculateTiming(5000).rcEnabled ? 1U : 0U, 1U);

  expectEqual("SCR trigger width", NSR150CDI::kSparkPulseWidthUs, 200U);
  std::cout << "V0.5 timing tests passed\n";
  return 0;
}
