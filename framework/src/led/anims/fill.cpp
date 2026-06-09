/**
 * @file fill.cpp
 */

#include "alchemy/led/anims/fill.h"
#include <cmath>

namespace alchemy {

static constexpr float kTwoPi         = 6.28318530f;
static constexpr float kPulsePeriodMs = 2000.0f;
static constexpr float kRippleHz      = 0.5f;
static constexpr float kRippleSpread  = 0.8f;

void DrawFill(LedPanel&       dst,
              uint8_t         pot_idx,
              float           start_hour,
              float           step_hours,
              uint8_t         num_steps,
              float           value,
              const FillDesc& desc,
              uint32_t        t_ms)
{
    /* Pre-compute Pulse breath multiplier (constant across all steps). */
    float breath_k = 1.0f;
    if (desc.anim == FillAnim::Pulse)
    {
        const float lo    = 1.0f - desc.anim_depth;
        const float phase = kTwoPi
            * static_cast<float>(t_ms % static_cast<uint32_t>(kPulsePeriodMs))
            / kPulsePeriodMs;
        breath_k = lo + desc.anim_depth * (0.5f + 0.5f * std::sin(phase));
    }

    /* ── Edge: unipolar fill from CCW end ─────────────────────────────── */
    if (desc.mode == FillMode::Edge)
    {
        float v = value;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;

        const float fill_f    = v * static_cast<float>(num_steps);
        uint8_t     fill_hard = static_cast<uint8_t>(fill_f);
        float       fill_frac = fill_f - static_cast<float>(fill_hard);
        ArcSnapFrac(fill_hard, fill_frac, num_steps);

        for (uint8_t step = 0u; step < num_steps; ++step)
        {
            const float   hour = start_hour + static_cast<float>(step) * step_hours;
            const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);

            float step_k = 1.0f;
            if (desc.anim == FillAnim::Ripple)
            {
                step_k = 1.0f + desc.anim_depth * std::sin(
                    kTwoPi * kRippleHz * (static_cast<float>(t_ms) * 0.001f)
                    + static_cast<float>(step) * kRippleSpread);
                if (step_k < 0.0f) step_k = 0.0f;
                if (step_k > 1.0f) step_k = 1.0f;
            }
            else if (desc.anim == FillAnim::Pulse)
            {
                step_k = breath_k;
            }

            if (step < fill_hard)
            {
                dst.SetRingByOffset(pot_idx, off,
                    dst.ScaleGlobal(LedPanel::Scale(desc.color, step_k)));
            }
            else if (step == fill_hard)
            {
                dst.SetRingByOffset(pot_idx, off,
                    dst.ScaleGlobal(LedPanel::Scale(desc.color, fill_frac * step_k)));
            }
            else
            {
                /* Passive step */
                if (desc.compose == FillCompose::Overlay)
                    continue;  /* leave untouched */
                dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(desc.passive_color));
            }
        }
        return;
    }

    /* ── Center: bipolar fan from pivot step ──────────────────────────── */
    if (desc.mode == FillMode::Center)
    {
        float sv = value;
        if (sv < -1.0f) sv = -1.0f;
        if (sv >  1.0f) sv =  1.0f;

        const float   amount    = (sv < 0.0f) ? -sv : sv;
        const float   side_f    = amount * static_cast<float>(desc.pivot);
        uint8_t       side_hard = static_cast<uint8_t>(side_f);
        float         side_frac = side_f - static_cast<float>(side_hard);
        ArcSnapFrac(side_hard, side_frac, desc.pivot);

        /* CCW arm color: sentinel {0,0,0} means "use the same color as the
         * CW arm" — preserves the single-color behaviour for callers that
         * don't care about per-arm tinting (attenuverters, damping, etc.
         * opt in by passing a non-black neg_color). */
        const bool neg_distinct = (desc.neg_color.r | desc.neg_color.g | desc.neg_color.b) != 0u;
        const LedPanel::Rgb ccw_color = neg_distinct ? desc.neg_color : desc.color;

        for (uint8_t step = 0u; step < num_steps; ++step)
        {
            const float   hour = start_hour + static_cast<float>(step) * step_hours;
            const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);

            float step_k = 1.0f;
            if (desc.anim == FillAnim::Ripple)
            {
                step_k = 1.0f + desc.anim_depth * std::sin(
                    kTwoPi * kRippleHz * (static_cast<float>(t_ms) * 0.001f)
                    + static_cast<float>(step) * kRippleSpread);
                if (step_k < 0.0f) step_k = 0.0f;
                if (step_k > 1.0f) step_k = 1.0f;
            }
            else if (desc.anim == FillAnim::Pulse)
            {
                step_k = breath_k;
            }

            LedPanel::Rgb c{0u, 0u, 0u};

            if (step == desc.pivot)
            {
                c = LedPanel::Scale(desc.center_color, step_k);
            }
            else if (step < desc.pivot)
            {
                const uint8_t dist = static_cast<uint8_t>(desc.pivot - step);
                if (sv < 0.0f)
                {
                    if      (dist <= side_hard)        c = LedPanel::Scale(ccw_color, step_k);
                    else if (dist == side_hard + 1u)   c = LedPanel::Scale(ccw_color, side_frac * step_k);
                }
            }
            else
            {
                const uint8_t dist = static_cast<uint8_t>(step - desc.pivot);
                if (sv > 0.0f)
                {
                    if      (dist <= side_hard)        c = LedPanel::Scale(desc.color, step_k);
                    else if (dist == side_hard + 1u)   c = LedPanel::Scale(desc.color, side_frac * step_k);
                }
            }

            /* In Overlay mode, skip writing steps where c would be {0,0,0}
               (i.e. steps outside the active arm). */
            if (desc.compose == FillCompose::Overlay
                && c.r == 0u && c.g == 0u && c.b == 0u)
            {
                /* Only skip if this is truly an inactive step (not the pivot at zero). */
                if (step != desc.pivot || (sv >= -kArcEdgeSnap && sv <= kArcEdgeSnap))
                    continue;
            }

            dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(c));
        }
        return;
    }
}

} // namespace alchemy
