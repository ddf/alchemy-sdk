/**
 * @file color_morph_arc.cpp
 */

#include "alchemy/led/anims/color_morph_arc.h"
#include <cmath>

namespace alchemy {

LedPanel::Rgb ColorMorphColor(float                 value,
                              const MorphSnapPoint* snaps,
                              uint8_t               num_snaps)
{
    if (num_snaps == 0u) return {0u, 0u, 0u};
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    if (num_snaps == 1u) return snaps[0].color;
    if (value <= snaps[0].position) return snaps[0].color;
    if (value >= snaps[num_snaps - 1u].position) return snaps[num_snaps - 1u].color;

    for (uint8_t i = 0; i < num_snaps - 1u; i++)
    {
        if (value <= snaps[i + 1u].position)
        {
            const float span = snaps[i + 1u].position - snaps[i].position;
            const float t    = (span > 1e-6f)
                ? (value - snaps[i].position) / span
                : 0.0f;
            return LedPanel::Mix(snaps[i].color, snaps[i + 1u].color, t);
        }
    }
    return snaps[num_snaps - 1u].color;
}

void DrawSnapMarkers(LedPanel&             dst,
                     uint8_t               pot_idx,
                     float                 arc_start_hour,
                     float                 arc_step_hours,
                     uint8_t               arc_leds,
                     const MorphSnapPoint* snaps,
                     uint8_t               num_snaps,
                     float                 marker_bright)
{
    const uint8_t effective_snaps = (num_snaps < 16u) ? num_snaps : 16u;

    uint8_t snap_steps[16];
    for (uint8_t si = 0u; si < effective_snaps; si++)
    {
        snap_steps[si] = static_cast<uint8_t>(
            snaps[si].position * static_cast<float>(arc_leds - 1u) + 0.5f);
    }

    for (uint8_t si = 0u; si < effective_snaps; si++)
    {
        const uint8_t step = snap_steps[si];
        const float   hour = arc_start_hour + static_cast<float>(step) * arc_step_hours;
        const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);
        dst.SetRingByOffset(pot_idx, off,
            dst.ScaleGlobal(LedPanel::Scale(snaps[si].color, marker_bright)));
    }
}

ColorMorphResult DrawColorMorphArc(LedPanel&             dst,
                                   uint8_t               pot_idx,
                                   float                 arc_start_hour,
                                   float                 arc_step_hours,
                                   uint8_t               arc_leds,
                                   float                 value,
                                   bool                  caught,
                                   const MorphSnapPoint* snaps,
                                   uint8_t               num_snaps,
                                   float                 snap_tol,
                                   float                 approach_dist,
                                   float                 marker_bright)
{
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    LedPanel::Rgb fill_col = ColorMorphColor(value, snaps, num_snaps);

    float   min_dist  = 1.0f;
    uint8_t near_snap = 0u;
    for (uint8_t si = 0u; si < num_snaps; si++)
    {
        const float dist = std::fabs(value - snaps[si].position);
        if (dist < min_dist) { min_dist = dist; near_snap = si; }
    }

    const bool at_snap = (min_dist <= snap_tol);

    if (min_dist < approach_dist && min_dist > snap_tol)
    {
        const float t_sat = 1.0f
            - (min_dist - snap_tol) / (approach_dist - snap_tol);
        fill_col = LedPanel::Mix(fill_col, snaps[near_snap].color, t_sat);
    }

    const float fill_f    = value * static_cast<float>(arc_leds);
    uint8_t     fill_hard = static_cast<uint8_t>(fill_f);
    float       fill_frac = fill_f - static_cast<float>(fill_hard);
    ArcSnapFrac(fill_hard, fill_frac, arc_leds);

    const float fill_k = caught ? 1.0f : 0.30f;

    const uint8_t effective_snaps = (num_snaps < 16u) ? num_snaps : 16u;
    uint8_t snap_steps[16];
    for (uint8_t si = 0u; si < effective_snaps; si++)
    {
        snap_steps[si] = static_cast<uint8_t>(
            snaps[si].position * static_cast<float>(arc_leds - 1u) + 0.5f);
    }

    for (uint8_t step = 0; step < arc_leds; step++)
    {
        const float   hour = arc_start_hour + static_cast<float>(step) * arc_step_hours;
        const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);

        LedPanel::Rgb c{0u, 0u, 0u};
        for (uint8_t si = 0u; si < effective_snaps; si++)
        {
            if (step == snap_steps[si])
            {
                c = LedPanel::Scale(snaps[si].color, marker_bright);
                break;
            }
        }

        if      (step < fill_hard)  c = LedPanel::Scale(fill_col, fill_k);
        else if (step == fill_hard) c = LedPanel::Scale(fill_col, fill_frac * fill_k);

        dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(c));
    }

    return { fill_col, at_snap };
}

} // namespace alchemy
