/**
 * @file alchemy/surface/preset_gesture_ui.h
 * @brief alchemy::PresetGestureUi — opt-in two-pot save/load UI gesture.
 *
 * Implements the canonical Alchemy Lab preset gesture: a slot-selector
 * pot (0..1 maps to slot 0..N-1) and an action pot (hard-CCW = save,
 * hard-CW = load, held kPresetHoldMs).  On commit it calls
 * Presets::Save / ::Load on the bound store.
 *
 * Independent of the Settings surface — instantiate directly if you
 * want preset gestures outside settings mode.  When you opt into
 * Settings::UsePresets() the Settings owns a PresetGestureUi internally.
 *
 * If you don't want the canned UI, ignore this class and call
 * presets.Save(slot) / presets.Load(slot) from your own gesture.
 * But you know, this gesture is pretty cool.
 *
 */

#pragma once

#include <cstdint>
#include "alchemy/control/pot_catch.h"
#include "alchemy/hardware_types.h"
#include "alchemy/led/animations.h"
#include "alchemy/led/panel.h"
#include "alchemy/surface/presets.h"

namespace alchemy {

/* ── Preset gesture FSM ──────────────────────────────────────────────── */

enum class PresetGesturePhase : uint8_t
{
    Idle = 0,
    Saving,
    Loading,
    FlashConfirm,
    Disarmed,
};

struct PresetGestureState
{
    PresetGesturePhase phase    = PresetGesturePhase::Idle;
    float              hold_ms  = 0.0f;
    float              flash_ms = 0.0f;
    uint8_t            slot     = 0;
    bool               was_save = false;
    bool               last_ok  = true;
};

/* ── Timing and threshold constants ──────────────────────────────────── */

constexpr float kPresetHoldMs        = 3000.0f;
constexpr float kPresetFlashTotalMs  = 1000.0f;
constexpr float kPresetFlashHalfMs   =  250.0f;
constexpr float kPresetEdgeThreshold =   0.05f;
constexpr float kPresetNeutralLo     =   0.30f;
constexpr float kPresetNeutralHi     =   0.70f;

/* ── Slot-indicator visual style ─────────────────────────────────────── */

/** Default colour palette for the slot indicator — four cool-to-warm
 *  groups covering 16 slots (slot/4 → colour, slot%4 → arc count). */
constexpr SlotIndicatorStyle kPresetSlotIndicatorDefault = {{
    {0x40, 0x80, 0xFF},
    {0x40, 0xE0, 0x60},
    {0xFF, 0xC8, 0x10},
    {0xFF, 0x40, 0x40},
}};

/* ── PresetGestureUi ─────────────────────────────────────────────────── */

class PresetGestureUi
{
  public:
    /** Default-construct unbound; call Bind() before Update/Render. */
    PresetGestureUi() = default;

    /** Bind to a Presets store at construction. */
    explicit PresetGestureUi(Presets& store) : presets_(&store) {}

    /** Bind (or re-bind) the Presets store. */
    void Bind(Presets& store) { presets_ = &store; }

    /**
     * Tick the FSM.  Returns true if a save/load committed this frame.
     *
     * @param slot_v   Slot-selector pot value (0..1).
     * @param action_v Action pot value (0..1; hard-CCW = save, hard-CW = load).
     * @param dt_ms    Elapsed time since the last call.
     */
    bool Update(float slot_v, float action_v, float dt_ms);

    /** Draw the slot indicator + the action-gesture progress arc. */
    void Render(LedPanel& panel, const ArcGeometry& geo,
                uint8_t slot_pot_idx, uint8_t action_pot_idx,
                uint32_t t_ms,
                LedPanel::Rgb progress_color = {0xFF, 0x40, 0x00},
                LedPanel::Rgb flash_color    = {0xFF, 0xFF, 0xFF}) const;

    /** Slot-indicator colour groups. */
    void SetSlotIndicatorColors(const SlotIndicatorStyle& colours)
    {
        slot_indicator_style_ = colours;
    }

    /**
     * Force the action pot into the Disarmed phase.  Call when entering
     * settings so that a pot already parked at the save/load edge does
     * not instantly trigger; the user must first bring it through the
     * neutral (noon) zone, at which point Idle is re-armed.
     *
     * No-op if a gesture is already in flight (Saving/Loading/FlashConfirm)
     * so post-commit re-arming does not cut off the confirmation flash.
     */
    void Disarm()
    {
        if (state_.phase != PresetGesturePhase::Idle) return;
        state_.phase    = PresetGesturePhase::Disarmed;
        state_.hold_ms  = 0.0f;
        state_.flash_ms = 0.0f;
    }

    const PresetGestureState& GestureState() const { return state_; }
    uint8_t SelectedSlot() const { return SlotFromValue(last_slot_v_); }

  private:
    static uint8_t SlotFromValue(float v);

    Presets*             presets_       = nullptr;
    PresetGestureState   state_;
    float                last_slot_v_   = 0.0f;
    float                last_action_v_ = 0.5f;
    SlotIndicatorStyle   slot_indicator_style_ = kPresetSlotIndicatorDefault;
};

} // namespace alchemy
