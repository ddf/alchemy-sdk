/**
 * @file alchemy/led/perf_renderer.h
 * @brief Generic performance-mode LED ring renderer.
 *
 * PerfRenderer draws all pot-ring LEDs for one page of a performance layout,
 * reading per-pot animation parameters from a row of ParamSlot descriptors.
 * No module-specific knowledge is required; the descriptors are populated
 * through the Binder fluent API in the module's Bind() override.
 *
 * Application code typically reaches the renderer via
 * alchemy::LedRender (in surface/led_render.h); the standalone class is
 * available for Composers driving a hand-rolled control loop.
 *
 * Slots with ArcStyle::None are skipped entirely; add custom per-pot
 * rendering before or after this call using the primitives in
 * alchemy/led/animations.h.
 *
 * PerfRenderer does NOT clear rings or call LedPanel::Show() — the caller owns those.
 */

#pragma once

#include <cstdint>
#include "alchemy/led/anim_desc.h"        // ParamSlot
#include "alchemy/control/pot_catch.h"
#include "alchemy/led/panel.h"

namespace alchemy {

class PerfRenderer
{
  public:
    /**
     * Construct with ring arc geometry.
     *
     * @param arc_start_hour  Clock-hour of the CCW arc endpoint.
     * @param arc_step_hours  Clock-hours between adjacent LED steps.
     * @param arc_leds        Total number of LEDs in the arc (not the full ring).
     */
    constexpr PerfRenderer(float   arc_start_hour = 7.5f,
                           float   arc_step_hours = 0.75f,
                           uint8_t arc_leds       = 13u)
        : arc_start_hour_(arc_start_hour)
        , arc_step_hours_(arc_step_hours)
        , arc_leds_(arc_leds)
    {}

    /** Construct from an ArcGeometry (e.g. kAlchemyLabArcGeometry). */
    constexpr explicit PerfRenderer(const ArcGeometry& geo)
        : arc_start_hour_(geo.start_hour)
        , arc_step_hours_(geo.step_hours)
        , arc_leds_(geo.arc_leds)
    {}

    /**
     * Render all pots on one page.
     *
     * @param slots     Row of @p num_pots ParamSlot descriptors for this page.
     *                  Obtain via Binder::PageSlots(page).
     * @param pots      PotState array for this page, length ≥ @p num_pots.
     * @param combined  CV-summed normalised values [0..1], length ≥ @p num_pots.
     * @param num_pots  Number of pots to render.
     * @param t_ms      Absolute time for shimmer animation.
     */
    void Render(LedPanel&        dst,
                const ParamSlot* slots,
                const PotState*  pots,
                const float*     combined,
                uint8_t          num_pots,
                uint32_t         t_ms) const;

  private:
    float   arc_start_hour_;
    float   arc_step_hours_;
    uint8_t arc_leds_;
};

} // namespace alchemy
