/**
 * @file action_gesture.h
 * @brief Preset save/load gesture arc animation.
 *
 * Draws a pot ring through five phases of a hold-to-commit gesture:
 *
 *   Idle         - bipolar proximity cue: shows which end of the arc
 *                  the user is approaching.
 *   Saving       - progress fill grows from the CCW end as the user
 *                  holds the pot at the CCW extreme.
 *   Loading      - progress fill grows from the CW end.
 *   FlashConfirm - the full ring flashes the confirm color twice
 *                  (2 x flash_half_ms) to signal a successful commit.
 *   Disarmed     - ring goes dark; user must return to neutral before
 *                  a new gesture can arm.
 *
 * Phase values match ActionGesturePhase cast to uint8_t.
 *
 * Typical use:
 *
 *   static const alchemy::ActionGestureStyle kMyStyle = {
 *       .progress_color = {0xC0, 0x40, 0x80},
 *       .confirm_color  = {0xFF, 0xFF, 0xFF},
 *   };
 *
 *   alchemy::DrawActionGesture(
 *       dst, pot_idx, kAlchemyLabArcGeometry,
 *       static_cast<uint8_t>(preset_gesture.phase),
 *       preset_gesture.hold_ms,
 *       preset_gesture.flash_ms,
 *       pot_state, fill_k,
 *       kMyStyle);
 */

#pragma once

#include <cstdint>
#include "alchemy/led/panel.h"
#include "alchemy/control/pot_catch.h"

namespace alchemy {

/** Symbolic phase constants for DrawActionGesture. */
enum class ActionGesturePhase : uint8_t
{
    Idle         = 0,
    Saving       = 1,
    Loading      = 2,
    FlashConfirm = 3,
    Disarmed     = 4,
};

/** Visual style configuration for DrawActionGesture. */
struct ActionGestureStyle
{
    LedPanel::Rgb progress_color = {0xC0, 0x40, 0x80};  ///< Hold-progress fill color.
    LedPanel::Rgb confirm_color  = {0xFF, 0xFF, 0xFF};  ///< Flash-confirm ring color.
    float         hold_ms        = 3000.0f;              ///< Full-hold duration in ms.
    float         flash_half_ms  = 250.0f;               ///< Flash on/off half-period in ms.
    uint8_t       ring_leds      = 16u;                  ///< Total LEDs in ring (for full flash).
};

/**
 * Draw the save/load gesture arc.
 *
 * @param dst               Ring to draw on.
 * @param pot_idx           Ring index.
 * @param arc_start_hour    Clock-hour of the CCW arc endpoint.
 * @param arc_step_hours    Clock-hours between adjacent arc steps.
 * @param arc_leds          Number of LEDs in the arc.
 * @param phase             Current phase as uint8_t (cast of ActionGesturePhase).
 * @param hold_ms_elapsed   Milliseconds held so far (0..style.hold_ms).
 * @param flash_ms_elapsed  Milliseconds into the flash window.
 * @param s                 PotState for the action pot (idle proximity cue uses s.stored).
 * @param fill_k            Brightness factor: 1.0 = caught, 0.30 = uncaught.
 * @param style             Visual style (colors, timing, ring size).
 */
void DrawActionGesture(LedPanel&                 dst,
                       uint8_t                   pot_idx,
                       float                     arc_start_hour,
                       float                     arc_step_hours,
                       uint8_t                   arc_leds,
                       uint8_t                   phase,
                       float                     hold_ms_elapsed,
                       float                     flash_ms_elapsed,
                       const PotState&           s,
                       float                     fill_k,
                       const ActionGestureStyle& style = {});

/** ArcGeometry overload. */
inline void DrawActionGesture(LedPanel&                 dst,
                              uint8_t                   pot_idx,
                              const ArcGeometry&        geo,
                              uint8_t                   phase,
                              float                     hold_ms_elapsed,
                              float                     flash_ms_elapsed,
                              const PotState&           s,
                              float                     fill_k,
                              const ActionGestureStyle& style = {})
{
    DrawActionGesture(dst, pot_idx,
                      geo.start_hour, geo.step_hours, geo.arc_leds,
                      phase, hold_ms_elapsed, flash_ms_elapsed,
                      s, fill_k, style);
}

} // namespace alchemy
