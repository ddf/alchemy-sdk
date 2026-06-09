/**
 * @file slot_indicator.h
 * @brief DrawSlotIndicator: preset slot visual indicator animation.
 *
 * Encodes a slot number (0..15) as a combination of color group and arc
 * count using a 4x4 grid:
 *
 *   slot / 4  -> color group  (0..3, defined by SlotIndicatorStyle::colors)
 *   slot % 4  -> arc count    (1, 2, 3, or 4 arcs lit)
 *
 * Arc positions within the ring (13-LED arc, gap-separated pairs):
 *   gap(0) - arc0(1-2) - gap(3) - arc1(4-5) - gap(6) -
 *   arc2(7-8) - gap(9) - arc3(10-11) - gap(12)
 *
 * This gives 16 visually distinct states that are easy to count and
 * read at a glance: 4 colors x 4 "fill levels" of arc pairs.
 */

#pragma once

#include <cstdint>
#include "alchemy/led/panel.h"
#include "alchemy/hardware_types.h"

namespace alchemy {

/** Four color groups for the 16-slot indicator. */
struct SlotIndicatorStyle
{
    LedPanel::Rgb colors[4];  ///< One color per group of four slots.
};

/**
 * Draw the preset slot indicator on a pot ring.
 *
 * @param pot_idx        Ring to draw on.
 * @param arc_start_hour Clock-hour of the CCW arc endpoint.
 * @param arc_step_hours Clock-hours between adjacent LED steps.
 * @param arc_leds       Number of LEDs in the arc.
 * @param slot           Slot number 0..15.
 * @param fill_k         Brightness factor: 1.0 = caught, 0.30 = uncaught.
 * @param style          Four slot-group colors.
 */
void DrawSlotIndicator(LedPanel&                  dst,
                       uint8_t                    pot_idx,
                       float                      arc_start_hour,
                       float                      arc_step_hours,
                       uint8_t                    arc_leds,
                       uint8_t                    slot,
                       float                      fill_k,
                       const SlotIndicatorStyle&  style);

/** ArcGeometry overload — forwards to the raw-geometry signature. */
inline void DrawSlotIndicator(LedPanel&                 dst,
                              uint8_t                   pot_idx,
                              const ArcGeometry&        geo,
                              uint8_t                   slot,
                              float                     fill_k,
                              const SlotIndicatorStyle& style)
{
    DrawSlotIndicator(dst, pot_idx, geo.start_hour, geo.step_hours, geo.arc_leds,
                      slot, fill_k, style);
}

} // namespace alchemy
