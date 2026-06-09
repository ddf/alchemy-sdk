/**
 * @file catch_pip.cpp
 */

#include "alchemy/led/anims/catch_pip.h"

namespace alchemy {

void DrawCatchPip(LedPanel&       dst,
                    uint8_t         pot_idx,
                    const PotState& s,
                    float           start_hour,
                    float           step_hours,
                    uint8_t         arc_leds,
                    LedPanel::Rgb   color)
{
    if (s.caught) return;
    if (arc_leds == 0u) return;

    float v = s.stored;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    const float hour = start_hour + v * static_cast<float>(arc_leds - 1u) * step_hours;
    dst.SetRingByHour(pot_idx, hour, dst.ScaleGlobal(color));
}

} // namespace alchemy
