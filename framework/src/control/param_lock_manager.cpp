/**
 * @file param_lock_manager.cpp
 */

#include "alchemy/control/param_lock_manager.h"

namespace alchemy {

void ParamLockManager::Init(ParamLockSlot* slots,
                            uint8_t        total_slots,
                            uint8_t        gesture_width,
                            float          gesture_threshold)
{
    slots_         = slots;
    total_slots_   = total_slots;
    gesture_width_ = gesture_width < kParamLockMaxGestureWidth
                         ? gesture_width
                         : kParamLockMaxGestureWidth;
    gesture_threshold_ = gesture_threshold;
    button_held_       = false;
    consumed_          = false;

    for (uint8_t i = 0; i < gesture_width_; i++)
    {
        baseline_[i]     = 0.5f;
        action_taken_[i] = false;
    }
}

void ParamLockManager::OnButtonDown(const float phys[])
{
    button_held_ = true;
    for (uint8_t i = 0; i < gesture_width_; i++)
    {
        baseline_[i]     = phys[i];
        action_taken_[i] = false;
    }
}

bool ParamLockManager::OnButtonUp()
{
    button_held_ = false;
    Finalise();
    const bool was_consumed = consumed_;
    consumed_ = false;
    return was_consumed;
}

void ParamLockManager::ProcessGestures(uint8_t offset, const float phys[])
{
    if (!button_held_ || slots_ == nullptr)
        return;

    for (uint8_t i = 0; i < gesture_width_; i++)
    {
        const uint8_t idx = static_cast<uint8_t>(offset + i);
        if (idx >= total_slots_)
            break;

        ParamLockSlot& lk    = slots_[idx];
        const float    diff  = phys[i] - baseline_[i];
        const float    delta = diff > 0.0f ? diff : -diff;
        const bool     nudged = (delta >= gesture_threshold_);

        if (nudged && !lk.recording && !action_taken_[i])
        {
            action_taken_[i] = true;
            consumed_        = true;
            baseline_[i]     = phys[i];

            if (lk.active)
            {
                lk = ParamLockSlot{};
            }
            else
            {
                lk.recording   = true;
                lk.active      = false;
                lk.length      = 0;
                lk.play_head   = 0;
                lk.record_base = phys[i];
            }
        }

        if (lk.recording)
        {
            if (lk.length < kParamLockBufferLen)
            {
                lk.buf[lk.length++] = phys[i];
            }
            else
            {
                lk.recording = false;
                lk.active    = true;
                lk.play_head = 0;
            }
        }
    }
}

void ParamLockManager::Advance()
{
    if (slots_ == nullptr) return;
    for (uint8_t i = 0; i < total_slots_; i++)
    {
        ParamLockSlot& lk = slots_[i];
        if (!lk.active || lk.length == 0) continue;
        lk.play_head = static_cast<uint16_t>((lk.play_head + 1u) % lk.length);
    }
}

float ParamLockManager::Read(uint8_t index) const
{
    if (slots_ == nullptr || index >= total_slots_) return 0.0f;
    const ParamLockSlot& lk = slots_[index];
    if (!lk.active || lk.length == 0) return 0.0f;
    return lk.buf[lk.play_head] - lk.record_base;
}

bool ParamLockManager::IsActive(uint8_t index) const
{
    return slots_ != nullptr && index < total_slots_ && slots_[index].active;
}

bool ParamLockManager::IsRecording(uint8_t index) const
{
    return slots_ != nullptr && index < total_slots_ && slots_[index].recording;
}

void ParamLockManager::Capture(SavedEntry out[], uint8_t count, uint8_t offset) const
{
    for (uint8_t i = 0; i < count; i++)
    {
        const uint8_t idx = static_cast<uint8_t>(offset + i);
        SavedEntry&   dst = out[i];

        const bool valid = slots_ != nullptr
                        && idx < total_slots_
                        && slots_[idx].active
                        && slots_[idx].length > 0;

        if (!valid)
        {
            dst.active      = 0u;
            dst.length      = 0u;
            dst.record_base = 0.0f;
            for (uint16_t j = 0; j < kParamLockBufferLen; j++)
                dst.buf[j] = 0.0f;
            continue;
        }

        const ParamLockSlot& lk = slots_[idx];
        dst.active      = 1u;
        dst.length      = lk.length;
        dst.record_base = lk.record_base;
        for (uint16_t j = 0; j < lk.length; j++)
            dst.buf[j] = lk.buf[j];
        for (uint16_t j = lk.length; j < kParamLockBufferLen; j++)
            dst.buf[j] = 0.0f;
    }
}

void ParamLockManager::Restore(const SavedEntry in[], uint8_t count, uint8_t offset)
{
    for (uint8_t i = 0; i < count; i++)
    {
        const uint8_t idx = static_cast<uint8_t>(offset + i);
        if (slots_ == nullptr || idx >= total_slots_)
            break;

        ParamLockSlot&    lk  = slots_[idx];
        const SavedEntry& src = in[i];

        lk = ParamLockSlot{};

        if (!src.active || src.length == 0 || src.length > kParamLockBufferLen)
            continue;

        lk.active      = true;
        lk.recording   = false;
        lk.length      = src.length;
        lk.play_head   = 0;
        lk.record_base = src.record_base;
        for (uint16_t j = 0; j < src.length; j++)
            lk.buf[j] = src.buf[j];
    }
}

void ParamLockManager::Clear()
{
    if (slots_ == nullptr)
        return;
    for (uint8_t i = 0; i < total_slots_; i++)
        slots_[i] = ParamLockSlot{};
}

void ParamLockManager::Finalise()
{
    if (slots_ == nullptr)
        return;
    for (uint8_t i = 0; i < total_slots_; i++)
    {
        ParamLockSlot& lk = slots_[i];
        if (lk.recording)
        {
            lk.recording = false;
            lk.active    = (lk.length > 0);
            lk.play_head = 0;
        }
    }
}

} // namespace alchemy
