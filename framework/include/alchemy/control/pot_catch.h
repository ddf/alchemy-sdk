/**
 * @file pot_catch.h
 * @brief Generic pot-catch state and algorithm.
 *
 * Pot-catch prevents a parameter jump when the user switches to a page
 * where the stored value differs from the physical pot position.  The
 * pot is "uncaught" until it physically passes through the stored value
 * (crossover catch) or lands within kCatchTolerance of it (proximity
 * catch).
 *
 * PotState and ParamLock are plain data structs; InitCatch and
 * UpdateCatch are stateless algorithms that operate on them.  They
 * carry no hardware or application dependencies.
 */

#pragma once

#include <cstdint>

namespace alchemy {

/* ── Tuning constants ─────────────────────────────────────────────────── */

/** Proximity window: pot is caught immediately if within this distance. */
constexpr float    kCatchTolerance             = 0.005f;

/** Circular looping-automation buffer length in frames (≈4 s at 60 Hz). */
constexpr uint16_t kParamLockBufferLen         = 256u;

/** Minimum physical nudge (0..1) required to start / stop param-lock. */
constexpr float    kParamLockGestureThreshold  = 0.03f;

/* ── PotState ─────────────────────────────────────────────────────────── */

/**
 * Per-pot catch state.
 *
 * stored       Logical parameter value (0..1) visible to the engine.
 * caught       True once the physical position has caught the stored value.
 * sign_at_lock Sign of (phys - stored) at the moment catch was disabled;
 *              used for crossover detection.
 */
struct PotState
{
    float stored       = 0.5f;
    bool  caught       = true;
    int   sign_at_lock = 0;
};

/* ── ParamLockSlot ────────────────────────────────────────────────────── */

/**
 * Per-pot looping automation recorder / player.
 *
 * When recording, physical pot values are appended each UI tick.
 * When playing, the recorded delta (buf[play_head] - record_base) is
 * added to (stored + cv_sum) exactly like a CV modulation — the physical
 * pot and its catch state are never modified by this struct.
 *
 * This is the raw data slot.  The user-facing surface is `alchemy::ParamLock`
 * (in surface/param_lock.h), which owns an array of these.
 */
struct ParamLockSlot
{
    bool     active      = false;
    bool     recording   = false;
    float    buf[kParamLockBufferLen] = {};
    uint16_t length      = 0;
    uint16_t play_head   = 0;
    float    record_base = 0.0f;
};

/* ── Algorithms ───────────────────────────────────────────────────────── */

/**
 * (Re-)initialise catch state when a pot's stored value changes externally
 * (page switch, tap tempo, preset load).  The pot becomes uncaught if the
 * physical position is outside the tolerance window, recording the current
 * sign so the crossover path can catch it too.
 */
inline void InitCatch(PotState& s, float phys)
{
    const float diff = phys - s.stored;
    if (diff > -kCatchTolerance && diff < kCatchTolerance)
    {
        s.caught       = true;
        s.sign_at_lock = 0;
        s.stored       = phys;
    }
    else
    {
        s.caught       = false;
        s.sign_at_lock = (diff > 0.0f) ? 1 : -1;
    }
}

/** Update catch state each UI tick. */
inline void UpdateCatch(PotState& s, float phys)
{
    if (s.caught)
    {
        s.stored = phys;
        return;
    }

    const float diff = phys - s.stored;
    if (diff > -kCatchTolerance && diff < kCatchTolerance)
    {
        s.caught = true;
        s.stored = phys;
        return;
    }

    const int cur_sign = (diff > 0.0f) ? 1 : -1;
    if (s.sign_at_lock != 0 && cur_sign != s.sign_at_lock)
    {
        s.caught = true;
        s.stored = phys;
    }
}

} // namespace alchemy
