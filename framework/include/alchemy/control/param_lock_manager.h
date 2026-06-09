/**
 * @file param_lock_manager.h
 * @brief Internal looping-automation engine used by the ParamLock surface.
 *
 * ParamLockManager is a low-level primitive: it owns nothing, allocates
 * nothing, and is driven by an external array of ParamLockSlot.  The
 * public-facing API for application code is the `alchemy::ParamLock`
 * surface in surface/param_lock.h — this header is the algorithm it
 * delegates to.
 *
 * Lifecycle (driven by the surface):
 *
 *   mgr.Init(slots, total_slots, gesture_width);
 *   on button down:                 mgr.OnButtonDown(phys);
 *   while button held, each tick:   mgr.ProcessGestures(offset, phys);
 *   on button up:                   bool consumed = mgr.OnButtonUp();
 *   every frame:                    mgr.Advance();         (advances all heads)
 *   when reading slot N:            float mod = mgr.Read(N);
 */

#pragma once

#include <cstdint>
#include "alchemy/control/pot_catch.h"

namespace alchemy {

/** Maximum pots-per-page supported by the gesture engine. */
constexpr uint8_t kParamLockMaxGestureWidth = 8u;

class ParamLockManager
{
  public:

    /**
     * Serialisable snapshot of one ParamLockSlot.
     * Packed so its binary layout is unambiguous across a CRC boundary.
     */
    struct SavedEntry
    {
        uint8_t  active;
        uint16_t length;
        float    record_base;
        float    buf[kParamLockBufferLen];
    } __attribute__((packed));

    static_assert(sizeof(SavedEntry) == 1u + 2u + 4u + kParamLockBufferLen * 4u,
                  "ParamLockManager::SavedEntry size mismatch");

    /**
     * Initialise the manager with an external slot array.
     *
     * @param slots             Caller-owned slot array.
     * @param total_slots       Length of the array.
     * @param gesture_width     Inputs per gesture frame (≤ kParamLockMaxGestureWidth).
     * @param gesture_threshold Minimum physical nudge (0..1) required to arm/disarm a lock.
     */
    void Init(ParamLockSlot* slots,
              uint8_t        total_slots,
              uint8_t        gesture_width,
              float          gesture_threshold = kParamLockGestureThreshold);

    /** Called when the trigger button goes down.  Snapshots baselines. */
    void OnButtonDown(const float phys[]);

    /** Called when the trigger button goes up.  Returns true if any gesture fired. */
    bool OnButtonUp();

    /**
     * Process one gesture frame.  Call every tick while button may be held.
     * @param offset  Index of the first slot to consider (e.g. page * pots).
     * @param phys    Physical pot positions; length must equal gesture_width.
     */
    void ProcessGestures(uint8_t offset, const float phys[]);

    /** Advance the play head on every active slot by one frame. */
    void Advance();

    /** Read the current modulation delta for one slot without advancing it. */
    float Read(uint8_t index) const;

    bool IsActive   (uint8_t index) const;
    bool IsRecording(uint8_t index) const;
    bool IsButtonHeld() const { return button_held_; }
    bool WasConsumed()  const { return consumed_; }

    /** Serialise count slots starting at offset into out[0..count-1]. */
    void Capture(SavedEntry  out[], uint8_t count, uint8_t offset = 0) const;

    /** Restore count slots starting at offset from in[0..count-1]. */
    void Restore(const SavedEntry in[], uint8_t count, uint8_t offset = 0);

    /** Reset every slot to its default (inactive) state. */
    void Clear();

  private:
    void Finalise();

    ParamLockSlot* slots_            = nullptr;
    uint8_t        total_slots_      = 0;
    uint8_t        gesture_width_    = 0;
    float          gesture_threshold_= kParamLockGestureThreshold;
    float          baseline_   [kParamLockMaxGestureWidth] = {};
    bool           action_taken_[kParamLockMaxGestureWidth] = {};
    bool           button_held_ = false;
    bool           consumed_    = false;
};

} // namespace alchemy
