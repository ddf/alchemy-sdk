/**
 * @file catch_pip.h
 * @brief DrawCatchPip: position indicator rendered when a pot is uncaught.
 *
 * When a pot is uncaught (the physical pot position doesn't yet match the
 * stored value), a single bright pip is drawn at the stored-value position
 * on the ring.  Once caught, nothing is drawn.
 */

#pragma once

#include <cstdint>
#include "alchemy/led/panel.h"
#include "alchemy/control/pot_catch.h"

namespace alchemy {

/**
 * Draw a single pip at the stored-value position when the pot is uncaught.
 *
 * The pip position maps s.stored (0..1) linearly across the LED positions:
 *   hour = start_hour + s.stored * (arc_leds - 1) * step_hours
 *
 * v=0 lands on the first arc LED, v=1 on the last, v=0.5 on the center LED
 * (when arc_leds is odd) — matching DrawPip / DrawFill / DrawSelector.
 *
 * Does nothing when s.caught is true.
 *
 * @param color  Pip color (before global brightness); typically white.
 */
void DrawCatchPip(LedPanel&       dst,
                  uint8_t         pot_idx,
                  const PotState& s,
                  float           start_hour,
                  float           step_hours,
                  uint8_t         arc_leds,
                  LedPanel::Rgb   color);

/** ArcGeometry overload. */
inline void DrawCatchPip(LedPanel&          dst,
                         uint8_t            pot_idx,
                         const PotState&    s,
                         const ArcGeometry& geo,
                         LedPanel::Rgb      color)
{
    DrawCatchPip(dst, pot_idx, s,
                 geo.start_hour, geo.step_hours, geo.arc_leds,
                 color);
}

} // namespace alchemy
