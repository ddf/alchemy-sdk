/**
 * @file perf_renderer.cpp
 * @brief alchemy::PerfRenderer implementation.
 */

#include "alchemy/led/perf_renderer.h"
#include "alchemy/led/animations.h"
#include "alchemy/led/panel.h"

namespace alchemy {

void PerfRenderer::Render(LedPanel&        dst,
                          const ParamSlot* slots,
                          const PotState*  pots,
                          const float*     combined,
                          uint8_t          num_pots,
                          uint32_t         t_ms) const
{
    for (uint8_t p = 0; p < num_pots; ++p)
    {
        const ParamSlot& slot = slots[p];

        if (slot.arc_style == ArcStyle::None)
            continue;

        const PotState& ps     = pots[p];
        const float     cv     = combined[p];
        const float     fill_k = ps.caught ? 1.0f : 0.30f;

        /* Captured by the Gradient arc case, read by the GradientSnap pip. */
        ColorMorphResult morph_result = {{0u, 0u, 0u}, false};

        /* ── Arc ─────────────────────────────────────────────────── */

        switch (slot.arc_style)
        {
            case ArcStyle::Level:
            {
                FillDesc desc;
                desc.mode         = FillMode::Edge;
                desc.color        = LedPanel::Scale(slot.arc_color,     fill_k);
                desc.passive_color= LedPanel::Scale(slot.arc_alt_color, fill_k);
                desc.anim         = slot.arc_anim;
                desc.anim_depth   = slot.arc_anim_depth;
                DrawFill(dst, p,
                         arc_start_hour_, arc_step_hours_, arc_leds_,
                         cv, desc, t_ms);
                break;
            }

            case ArcStyle::Bipolar:
            {
                const float signed_v = (cv - 0.5f) * 2.0f;
                FillDesc desc;
                desc.mode         = FillMode::Center;
                desc.color        = LedPanel::Scale(slot.arc_color,        fill_k);
                desc.neg_color    = LedPanel::Scale(slot.arc_alt_color,    fill_k);
                desc.center_color = LedPanel::Scale(slot.arc_center_color, fill_k);
                desc.anim         = slot.arc_anim;
                desc.anim_depth   = slot.arc_anim_depth;
                desc.pivot        = arc_leds_ / 2u;
                DrawFill(dst, p,
                         arc_start_hour_, arc_step_hours_, arc_leds_,
                         signed_v, desc, t_ms);
                break;
            }

            case ArcStyle::Selector:
            {
                SelectorDesc sel;
                sel.num_zones     = slot.arc_num_zones;
                sel.zone_geo      = slot.arc_zone_geo;
                sel.active_color  = LedPanel::Scale(slot.arc_color,     fill_k);
                sel.inactive_color= LedPanel::Scale(slot.arc_alt_color, fill_k);
                sel.inactive_dim  = slot.arc_inactive_dim;
                sel.avail_mask    = slot.arc_avail_mask;
                DrawSelector(dst, p, arc_start_hour_, arc_step_hours_, arc_leds_, cv, sel);
                break;
            }

            case ArcStyle::Gradient:
            {
                if (slot.arc_snaps && slot.arc_num_snaps > 0u)
                {
                    morph_result = DrawColorMorphArc(
                        dst, p,
                        arc_start_hour_, arc_step_hours_, arc_leds_,
                        cv, ps.caught,
                        slot.arc_snaps, slot.arc_num_snaps);
                }
                break;
            }

            case ArcStyle::GradientFill:
            {
                /* Plain Edge fill whose color is sampled from a sibling pot's
                 * value through the snap gradient; falls back to arc_color. */
                LedPanel::Rgb  col = slot.arc_color;
                const uint8_t  src = slot.arc_color_src_pot;
                if (src < num_pots && slot.arc_snaps && slot.arc_num_snaps > 0u)
                    col = ColorMorphColor(combined[src],
                                          slot.arc_snaps, slot.arc_num_snaps);

                FillDesc desc;
                desc.color = LedPanel::Scale(col, fill_k);
                DrawFill(dst, p,
                         arc_start_hour_, arc_step_hours_, arc_leds_,
                         cv, desc, t_ms);
                break;
            }

            default:
                break;
        }

        /* ── Bottom pip ──────────────────────────────────────────── */

        const uint8_t bottom = dst.RingOffsetForHour(p, 6.0f);

        switch (slot.pip_style)
        {
            case PipStyle::Solid:
                dst.SetRingByOffset(p, bottom,
                    dst.ScaleGlobal(LedPanel::Scale(slot.pip_color, fill_k)));
                break;

            case PipStyle::ThresholdSnap:
                if (cv >= slot.snap_lo && cv <= slot.snap_hi)
                    dst.SetRingByOffset(p, bottom,
                        dst.ScaleGlobal(LedPanel::Scale(slot.pip_color, fill_k)));
                else if (cv > slot.snap_hi)
                    dst.SetRingByOffset(p, bottom,
                        dst.ScaleGlobal(LedPanel::Scale(slot.snap_hi_color, fill_k)));
                break;

            case PipStyle::GradientSnap:
                if (morph_result.at_snap && ps.caught)
                    dst.SetRingByOffset(p, bottom,
                        dst.ScaleGlobal(morph_result.fill_color));
                break;

            case PipStyle::None:
            default:
                break;
        }

        /* ── Catch pip ───────────────────────────────────────────── */

        DrawCatchPip(dst, p, ps, arc_start_hour_, arc_step_hours_, arc_leds_, {0xFF, 0xFF, 0xFF});
    }
}

} // namespace alchemy
