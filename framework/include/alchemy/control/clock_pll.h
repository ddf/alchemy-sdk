/**
 * @file clock_pll.h
 * @brief Per-line tempo-meter PLL for external clock pulses (BPM only).
 *
 * **For new code that needs a position in the bar, a phase ramp, or
 * click-free synced audio, use `alchemy::MusicalClock` +
 * `alchemy::ClockFollower` (see musical_clock.h / clock_follower.h)
 * instead.**  `ClockPll` is retained for modules that only want a tempo
 * number and no musical position.
 *
 * Implements a PI-controller PLL tuned for 24 PPQN MIDI clocks at
 * musical tempos (20–300 BPM).  Designed to be polled at ~1 ms cadence
 * (no ISR required).
 *
 *   OnPulse(stamp_us)  — call on every rising edge detected from the
 *                        CV input poll loop.
 *   Update(now_us)     — call once per poll tick to detect clock loss.
 *
 * Both functions accept a raw microsecond timestamp from the platform
 * monotonic counter (32-bit wrap-safe arithmetic is used internally).
 */

#pragma once

#include <cstdint>

namespace alchemy {

/* ── Tuning constants ─────────────────────────────────────────────────── */

/** Pulses per quarter note assumed on the input (standard MIDI clock). */
constexpr uint32_t kClockPllExtPpqn = 24u;

/** EMA smoothing factor for period estimate. Smaller = smoother, slower. */
constexpr float    kClockPllAlpha = 0.08f;

/** Consecutive valid pulses required to declare lock. */
constexpr uint32_t kClockPllLockPulses = 8u;

/** Silence duration (× period_est) after which lock is dropped. */
constexpr float    kClockPllLossFactor = 32.0f;

/** Acceptance window (±fraction of period_est) for missed-pulse detection. */
constexpr float    kClockPllMissedPulseTol = 0.34f;

/** Sub-period glitch floor: pulses faster than this fraction are dropped. */
constexpr float    kClockPllGlitchToleranceLo = 1.0f - kClockPllMissedPulseTol;

/** Maximum number of consecutive missed pulses to compensate for. */
constexpr uint32_t kClockPllMaxMissedPulses = 4u;

/** BPM limits used to derive hardware-safe period bounds. */
constexpr float kClockPllBpmMin = 20.0f;
constexpr float kClockPllBpmMax = 300.0f;

/* ── ClockPll ─────────────────────────────────────────────────────────── */

class ClockPll
{
  public:
    void Init();

    /** Handle a clock-pulse rising edge. stamp_us from platform µs counter. */
    void OnPulse(uint32_t stamp_us);

    /** Per-tick loss-detection (no pulse). now_us same source as OnPulse. */
    void Update(uint32_t now_us);

    /** Reset to unlocked state (preserves period estimate for warm re-lock). */
    void Reset();

    /** True once kClockPllLockPulses valid pulses have been seen. */
    bool Locked() const { return locked_; }

    /**
     * Current quarter-note period in seconds.
     * Valid only when Locked(); returns 0 otherwise.
     */
    float QuarterSeconds() const;

    /** Current BPM estimate. Valid only when Locked(). */
    float Bpm() const;

  private:
    void RecomputeLimits();

    bool     enabled_           = true;
    bool     locked_            = false;
    bool     warm_loss_         = false;
    uint32_t valid_pulse_count_ = 0u;
    uint32_t last_pulse_stamp_  = 0u;
    bool     have_stamp_        = false;
    float    period_est_us_     = 0.0f;
    uint32_t min_period_us_     = 500u;
    uint32_t max_period_us_     = 600000u;
};

} // namespace alchemy
