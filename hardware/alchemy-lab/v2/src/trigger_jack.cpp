/**
 * @file trigger_jack.cpp
 * @brief TriggerJack — block-rate edge detector on a codec input channel.
 */

#include "alchemy/hw/trigger_jack.h"

namespace alchemy {

namespace {
constexpr float kCodecFullScaleVolts = 5.0f;
}

bool TriggerJack::RisingEdge()
{
    const bool was = latched_;
    latched_ = false;
    return was;
}

void TriggerJack::SetTriggerThreshold(float volts_at_jack)
{
    if (volts_at_jack <= 0.0f) volts_at_jack = 0.05f;
    if (volts_at_jack > kCodecFullScaleVolts)
        volts_at_jack = kCodecFullScaleVolts;
    threshold_sample_ = volts_at_jack / kCodecFullScaleVolts;
}

void TriggerJack::ProcessBlock(const float* samples, size_t n)
{
    /* The codec's centered sample is already 0-mean (AC coupling at the
     * analog stage strips DC), so we threshold against |sample|. Rising
     * edge = previous sample was below threshold, current is above. */
    const float th = threshold_sample_;
    bool prev_above = prev_above_;
    for (size_t i = 0; i < n; ++i)
    {
        const float s = samples[i];
        const bool above = (s > th);
        if (above && !prev_above) latched_ = true;
        prev_above = above;
    }
    prev_above_ = prev_above;
}

} // namespace alchemy
