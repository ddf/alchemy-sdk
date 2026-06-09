/**
 * @file surface/led_render.cpp
 * @brief alchemy::LedRender implementation.
 */

#include "alchemy/surface/led_render.h"
#include "alchemy/surface/knob_storage.h"
#include "alchemy/control/pot_catch.h"

namespace alchemy {

LedRender::LedRender(LedPanel&          panel,
                     const ArcGeometry& geo,
                     ValueProvider      value,
                     void*              ctx,
                     const KnobStorage* storage,
                     uint8_t            num_pots)
    : panel_(&panel),
      renderer_(geo),
      value_(value),
      ctx_(ctx),
      storage_(storage),
      num_pots_(num_pots)
{
    if (num_pots_ == 0u)
        num_pots_ = storage_ ? storage_->NumPots() : LedBinder::kMaxPots;
    if (num_pots_ > LedBinder::kMaxPots)
        num_pots_ = LedBinder::kMaxPots;
}

void LedRender::Render(uint32_t t_ms)
{
    /* Build the per-frame value array and catch array, then delegate to
     * the existing PerfRenderer. */
    float    combined[LedBinder::kMaxPots] = {};
    PotState pots    [LedBinder::kMaxPots] = {};

    for (uint8_t p = 0; p < num_pots_; p++)
    {
        combined[p]    = value_ ? value_(p, ctx_) : 0.0f;
        if (storage_)
        {
            pots[p] = storage_->State(storage_->ActivePage(), p);
        }
        else
        {
            pots[p].stored = combined[p];
            pots[p].caught = true;
        }
    }

    renderer_.Render(*panel_, binder_.Slots(), pots, combined, num_pots_, t_ms);
}

} // namespace alchemy
