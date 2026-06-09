/**
 * @file clock_pll.cpp
 * @brief Per-line PLL follower for external clock pulses.  See clock_pll.h.
 */

#include "alchemy/control/clock_pll.h"

namespace alchemy {

void ClockPll::Init()
{
    enabled_           = true;
    locked_            = false;
    warm_loss_         = false;
    valid_pulse_count_ = 0u;
    last_pulse_stamp_  = 0u;
    have_stamp_        = false;
    period_est_us_     = 0.0f;
    RecomputeLimits();
}

void ClockPll::Reset()
{
    locked_            = false;
    warm_loss_         = false;
    valid_pulse_count_ = 0u;
    have_stamp_        = false;
    period_est_us_     = 0.0f;
}

void ClockPll::RecomputeLimits()
{
    const float fastest_q_us     = (60.0f / kClockPllBpmMax) * 1000000.0f;
    const float fastest_pulse_us = fastest_q_us / static_cast<float>(kClockPllExtPpqn);
    uint32_t    calc_min         = static_cast<uint32_t>(fastest_pulse_us * 0.25f);
    if (calc_min < 500u) calc_min = 500u;
    min_period_us_ = calc_min;

    const float slowest_q_us = (60.0f / kClockPllBpmMin) * 1000000.0f;
    uint32_t    calc_max     = static_cast<uint32_t>(slowest_q_us * 2.0f);
    if (calc_max < 100000u) calc_max = 100000u;
    max_period_us_ = calc_max;
}

void ClockPll::OnPulse(uint32_t stamp_us)
{
    if (!enabled_) return;

    if (!have_stamp_)
    {
        last_pulse_stamp_  = stamp_us;
        have_stamp_        = true;
        valid_pulse_count_ = 1u;
        if (warm_loss_ && period_est_us_ > 0.0f)
        {
            locked_    = true;
            warm_loss_ = false;
        }
        return;
    }

    const uint32_t dt = stamp_us - last_pulse_stamp_;

    if (dt < min_period_us_) return;

    if (dt > max_period_us_)
    {
        locked_            = false;
        valid_pulse_count_ = 1u;
        period_est_us_     = 0.0f;
        last_pulse_stamp_  = stamp_us;
        return;
    }

    if (locked_ && period_est_us_ > 0.0f)
    {
        const float dt_f = static_cast<float>(dt);

        if (dt_f < period_est_us_ * kClockPllGlitchToleranceLo)
            return;

        const float lo_tol = 1.0f - kClockPllMissedPulseTol;
        const float hi_tol = 1.0f + kClockPllMissedPulseTol;
        for (uint32_t k = 1u; k <= kClockPllMaxMissedPulses; ++k)
        {
            const float divided = dt_f / static_cast<float>(k);
            const float ratio   = divided / period_est_us_;
            if (ratio >= lo_tol && ratio <= hi_tol)
            {
                last_pulse_stamp_ = stamp_us;
                period_est_us_ += kClockPllAlpha * (divided - period_est_us_);
                valid_pulse_count_++;
                return;
            }
        }
        return;
    }

    last_pulse_stamp_ = stamp_us;
    valid_pulse_count_++;

    if (period_est_us_ <= 0.0f)
        period_est_us_ = static_cast<float>(dt);
    else
        period_est_us_ += kClockPllAlpha * (static_cast<float>(dt) - period_est_us_);

    if (!locked_ && valid_pulse_count_ >= kClockPllLockPulses)
        locked_ = true;
}

void ClockPll::Update(uint32_t now_us)
{
    if (!enabled_ || !have_stamp_ || period_est_us_ <= 0.0f)
        return;

    const uint32_t dt_since  = now_us - last_pulse_stamp_;
    const float    threshold = period_est_us_ * kClockPllLossFactor;
    if (static_cast<float>(dt_since) > threshold)
    {
        /* period_est_us_ > 0.0f is guaranteed by the early-return above. */
        warm_loss_         = true;
        locked_            = false;
        valid_pulse_count_ = 0u;
        have_stamp_        = false;
        /* period_est_us_ intentionally preserved for warm re-lock */
    }
}

float ClockPll::QuarterSeconds() const
{
    if (!locked_ || period_est_us_ <= 0.0f) return 0.0f;
    return (period_est_us_ * static_cast<float>(kClockPllExtPpqn)) * 1e-6f;
}

float ClockPll::Bpm() const
{
    const float q = QuarterSeconds();
    if (q <= 0.0f) return 0.0f;
    return 60.0f / q;
}

} // namespace alchemy
