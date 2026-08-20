/*
 * NSR-150 可程式 CDI
 * Version: V0.5
 *
 * V0.5 保留 V0.4 的點火角表、RC 門檻與 200 us SCR 觸發脈寬，
 * 改由 ESP8266 Timer1 one-shot 排程點火，避免主迴圈延遲改變點火時間。
 *
 * 接腳如下：
 *   D1 凸台脈衝輸入
 *   D4 RC 訊號輸出
 *   D6 點火輸出
 *
 * Release Date: 2026-08-21
 * Author: 羽山秋人 (https://3wa.tw)
 * Author: @FB 田峻墉
 */

#include <Arduino.h>

#include "cdi_timing.h"

const uint8_t ToPin = D1;
const uint8_t RCPin = D4;
const uint8_t FirePin = D6;

// ESP8266 Timer1 在 TIM_DIV16 下為 5 MHz，每一微秒為 5 個 timer ticks。
const uint32_t TimerTicksPerUs = 5UL;
const uint32_t FirePinMask = 1UL << FirePin;
const uint32_t RCPinMask = 1UL << RCPin;

enum TimerPhase : uint8_t {
  TimerIdle = 0,
  TimerWaitingForFire,
  TimerWaitingForRelease
};

volatile TimerPhase timerPhase = TimerIdle;
volatile uint32_t lastPulseAtUs = 0;
volatile uint32_t rpm = 0;
volatile uint32_t rpmDelayUs = 0;
volatile bool hasPreviousPulse = false;

void IRAM_ATTR setFireOutput(bool isHigh) {
  if (isHigh) {
    GPOS = FirePinMask;
  } else {
    GPOC = FirePinMask;
  }
}

void IRAM_ATTR setRCOutput(bool isHigh) {
  if (isHigh) {
    GPOS = RCPinMask;
  } else {
    GPOC = RCPinMask;
  }
}

void IRAM_ATTR stopScheduledOutputs() {
  timer1_disable();
  timerPhase = TimerIdle;
  setFireOutput(false);
  setRCOutput(false);
}

void IRAM_ATTR armTimerUs(uint32_t delayUs) {
  uint32_t ticks = delayUs * TimerTicksPerUs;

  if (ticks == 0) {
    ticks = 1;
  }

  // 單次 Timer1 只服務這次的點火事件，沒有周期性 PWM 或主迴圈等待。
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(ticks);
}

void IRAM_ATTR onFireTimer() {
  if (timerPhase == TimerWaitingForFire) {
    setFireOutput(true);
    timerPhase = TimerWaitingForRelease;
    timer1_write(NSR150CDI::kSparkPulseWidthUs * TimerTicksPerUs);
    return;
  }

  if (timerPhase == TimerWaitingForRelease) {
    stopScheduledOutputs();
    return;
  }

  // 非預期的 Timer1 中斷一律回到不點火狀態。
  stopScheduledOutputs();
}

void IRAM_ATTR countup() {
  const uint32_t pulseAtUs = micros();

  if (!hasPreviousPulse) {
    lastPulseAtUs = pulseAtUs;
    hasPreviousPulse = true;
    return;
  }

  const uint32_t elapsedUs = pulseAtUs - lastPulseAtUs;

  if (elapsedUs < NSR150CDI::kMinimumPulseIntervalUs) {
    // 保留最後一個合理脈衝作為基準，忽略過快的雜訊或不可能轉速。
    return;
  }

  lastPulseAtUs = pulseAtUs;
  rpmDelayUs = elapsedUs;

  if (!NSR150CDI::isValidPulseIntervalUs(elapsedUs)) {
    rpm = 0;
    stopScheduledOutputs();
    return;
  }

  const uint32_t currentRpm = NSR150CDI::calculateRpmFromPulseIntervalUs(elapsedUs);

  if (currentRpm == 0) {
    rpm = 0;
    stopScheduledOutputs();
    return;
  }

  rpm = currentRpm;

  // 正常轉速下前一個事件應已結束；若未結束，先停火，不累積或重複點火。
  if (timerPhase != TimerIdle) {
    stopScheduledOutputs();
  }

  const NSR150CDI::Timing timing = NSR150CDI::calculateTiming(currentRpm);
  setRCOutput(timing.rcEnabled);

  // 點火延遲必須從凸台脈衝起算，而不是從 ISR 或主迴圈稍後開始。
  const uint32_t calculationUs = micros() - pulseAtUs;
  const uint32_t remainingDelayUs = timing.fireDelayUs > calculationUs
    ? timing.fireDelayUs - calculationUs
    : 1UL;

  timerPhase = TimerWaitingForFire;
  armTimerUs(remainingDelayUs);
}

void setup() {
  pinMode(ToPin, INPUT_PULLUP);
  pinMode(RCPin, OUTPUT);
  pinMode(FirePin, OUTPUT);
  setRCOutput(false);
  setFireOutput(false);

  // 保留 V0.4 開機後先讓 RC 全開一次、再關閉的行為。
  for (uint16_t i = 0; i < 250; ++i) {
    setRCOutput(true);
    delayMicroseconds(3000);
    setRCOutput(false);
    delayMicroseconds(3000);
  }

  timer1_isr_init();
  timer1_attachInterrupt(onFireTimer);
  timer1_disable();
  attachInterrupt(digitalPinToInterrupt(ToPin), countup, RISING);
}

void loop() {
  // 點火全由外部中斷與 Timer1 處理；主迴圈不執行 Serial 或等待工作。
  yield();
}
