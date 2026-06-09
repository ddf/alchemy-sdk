/**
 * @file surface/preset_gesture_ui.cpp
 * @brief alchemy::PresetGestureUi implementation.
 */

#include "alchemy/surface/preset_gesture_ui.h"
#include "alchemy/hw/alchemy_lab_layout.h"   /* kLedsPerRing */

namespace alchemy {

uint8_t PresetGestureUi::SlotFromValue(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    int idx = static_cast<int>(v * static_cast<float>(Presets::kNumSlots));
    if (idx >= static_cast<int>(Presets::kNumSlots))
        idx = static_cast<int>(Presets::kNumSlots) - 1;
    return static_cast<uint8_t>(idx);
}

bool PresetGestureUi::Update(float slot_v, float action_v, float dt_ms)
{
    if (!presets_) return false;

    last_slot_v_   = slot_v;
    last_action_v_ = action_v;

    const bool at_save = (action_v <=        kPresetEdgeThreshold);
    const bool at_load = (action_v >= 1.0f - kPresetEdgeThreshold);
    const bool neutral = (action_v >= kPresetNeutralLo && action_v <= kPresetNeutralHi);

    PresetGestureState& g = state_;
    bool committed = false;

    switch (g.phase)
    {
        case PresetGesturePhase::Idle:
            if (at_save)
            {
                g.phase    = PresetGesturePhase::Saving;
                g.hold_ms  = 0.0f;
                g.was_save = true;
                g.slot     = SelectedSlot();
            }
            else if (at_load)
            {
                g.phase    = PresetGesturePhase::Loading;
                g.hold_ms  = 0.0f;
                g.was_save = false;
                g.slot     = SelectedSlot();
            }
            break;

        case PresetGesturePhase::Saving:
        case PresetGesturePhase::Loading:
        {
            const bool still = (g.phase == PresetGesturePhase::Saving) ? at_save : at_load;
            if (!still)
            {
                g.phase   = PresetGesturePhase::Idle;
                g.hold_ms = 0.0f;
                break;
            }
            g.hold_ms += dt_ms;
            if (g.hold_ms >= kPresetHoldMs)
            {
                g.last_ok = (g.was_save) ? presets_->Save(g.slot) : presets_->Load(g.slot);
                committed = true;
                g.phase   = g.last_ok ? PresetGesturePhase::FlashConfirm
                                      : PresetGesturePhase::Disarmed;
                g.flash_ms = 0.0f;
                g.hold_ms  = 0.0f;
            }
            break;
        }

        case PresetGesturePhase::FlashConfirm:
            g.flash_ms += dt_ms;
            if (g.flash_ms >= kPresetFlashTotalMs)
            {
                g.phase    = PresetGesturePhase::Disarmed;
                g.flash_ms = 0.0f;
            }
            break;

        case PresetGesturePhase::Disarmed:
            if (neutral) g.phase = PresetGesturePhase::Idle;
            break;
    }

    return committed;
}

void PresetGestureUi::Render(LedPanel& panel, const ArcGeometry& geo,
                             uint8_t slot_pot_idx, uint8_t action_pot_idx,
                             uint32_t t_ms,
                             LedPanel::Rgb progress_color,
                             LedPanel::Rgb flash_color) const
{
    DrawSlotIndicator(panel, slot_pot_idx,
                      geo.start_hour, geo.step_hours, geo.arc_leds,
                      SlotFromValue(last_slot_v_),
                      1.0f, slot_indicator_style_);

    ActionGestureStyle style;
    style.progress_color = progress_color;
    style.confirm_color  = flash_color;
    style.hold_ms        = kPresetHoldMs;
    style.flash_half_ms  = kPresetFlashHalfMs;
    style.ring_leds      = kLedsPerRing;

    PotState ps;
    ps.stored = last_action_v_;
    ps.caught = true;

    DrawActionGesture(panel, action_pot_idx, geo,
                      static_cast<uint8_t>(state_.phase),
                      state_.hold_ms,
                      state_.flash_ms,
                      ps,
                      1.0f, style);
    (void)t_ms;
}

} // namespace alchemy
