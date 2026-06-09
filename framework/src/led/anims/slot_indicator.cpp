/**
 * @file slot_indicator.cpp
 */

#include "alchemy/led/anims/slot_indicator.h"

namespace alchemy {

namespace {

inline bool SlotStepInArc(uint8_t step, uint8_t num_arcs)
{
    if (num_arcs == 0u)                    return false;
    if (step >= 1u && step <= 2u)          return true;   /* arc 0 */
    if (num_arcs == 1u)                    return false;
    if (step >= 4u && step <= 5u)          return true;   /* arc 1 */
    if (num_arcs == 2u)                    return false;
    if (step >= 7u && step <= 8u)          return true;   /* arc 2 */
    if (num_arcs == 3u)                    return false;
    return (step >= 10u && step <= 11u);                  /* arc 3 */
}

} // namespace

void DrawSlotIndicator(LedPanel&                 dst,
                         uint8_t                   pot_idx,
                         float                     arc_start_hour,
                         float                     arc_step_hours,
                         uint8_t                   arc_leds,
                         uint8_t                   slot,
                         float                     fill_k,
                         const SlotIndicatorStyle& style)
{
    const uint8_t       colour_idx = (slot >> 2) & 0x3u;
    const uint8_t       num_arcs   = (slot & 0x3u) + 1u;
    const LedPanel::Rgb base_col   = style.colors[colour_idx];

    for (uint8_t step = 0; step < arc_leds; step++)
    {
        const float   hour = arc_start_hour + static_cast<float>(step) * arc_step_hours;
        const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);
        const LedPanel::Rgb c = SlotStepInArc(step, num_arcs)
            ? LedPanel::Scale(base_col, fill_k)
            : LedPanel::Rgb{0u, 0u, 0u};
        dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(c));
    }
}

} // namespace alchemy
