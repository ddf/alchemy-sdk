/**
 * @file alchemy/led/anims/beat_pip.cpp
 */

#include "alchemy/led/anims/beat_pip.h"

#include <algorithm>

namespace alchemy {

void BeatPip::Reset()
{
    phase_ms_        = 0.0f;
    on_remaining_ms_ = 0.0f;
    was_blinking_    = false;
}

void BeatPip::Tick(float dt_ms, float period_ms)
{
    last_period_ms_ = period_ms;

    if (period_ms < cfg_.beat_threshold_ms)
    {
        /* Hold state at zero so the next regime transition re-inits cleanly. */
        phase_ms_        = 0.0f;
        on_remaining_ms_ = 0.0f;
        was_blinking_    = false;
        return;
    }

    const float on_ms = std::min(cfg_.on_window_ms, period_ms * 0.5f);

    on_remaining_ms_ -= dt_ms;
    if (on_remaining_ms_ < 0.0f) on_remaining_ms_ = 0.0f;

    if (!was_blinking_)
    {
        /* First frame in blink regime — light the pip immediately. */
        phase_ms_        = 0.0f;
        on_remaining_ms_ = on_ms;
        was_blinking_    = true;
    }

    phase_ms_ += dt_ms;
    while (phase_ms_ >= period_ms)
    {
        phase_ms_        -= period_ms;
        on_remaining_ms_  = on_ms;
    }
}

void BeatPip::Draw(LedPanel&     panel,
                   uint8_t       pot,
                   LedPanel::Rgb on_color,
                   LedPanel::Rgb solid_color,
                   float         hour) const
{
    if (last_period_ms_ >= cfg_.beat_threshold_ms)
    {
        if (on_remaining_ms_ > 0.0f)
            panel.SetRingByHour(pot, hour, panel.ScaleGlobal(on_color));
        return;
    }

    /* Short-period regime: blend on_color → solid_color as the period
     * approaches min_period_ms. */
    const float range = cfg_.beat_threshold_ms - cfg_.min_period_ms;
    float t = (range > 0.0f)
                  ? (cfg_.beat_threshold_ms - last_period_ms_) / range
                  : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    panel.SetRingByHour(pot, hour,
                        panel.ScaleGlobal(LedPanel::Mix(on_color, solid_color, t)));
}

} // namespace alchemy
