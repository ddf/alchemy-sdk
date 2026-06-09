/**
 * @file cv_edge.cpp
 * @brief alchemy::CvEdge implementation — see cv_edge.h.
 */

#include "alchemy/control/cv_edge.h"

namespace alchemy {

void CvEdge::Init(uint8_t num_channels)
{
    Init(num_channels, Config{});
}

void CvEdge::Init(uint8_t num_channels, Config cfg)
{
    num_channels_ = (num_channels > kMaxCvEdgeChannels)
                        ? kMaxCvEdgeChannels
                        : num_channels;
    cfg_ = cfg;
    for (uint8_t i = 0; i < kMaxCvEdgeChannels; ++i)
        channels_[i] = ChannelState{};
    last_rising_mask_  = 0u;
    last_falling_mask_ = 0u;
}

uint32_t CvEdge::Tick(const float* cv, uint8_t num_cv, uint32_t now_us)
{
    const uint8_t n = (num_cv < num_channels_) ? num_cv : num_channels_;
    uint32_t      rising_mask  = 0u;
    uint32_t      falling_mask = 0u;

    for (uint8_t i = 0; i < n; ++i)
    {
        ChannelState& s = channels_[i];
        s.just_rose = false;
        s.just_fell = false;

        const float v = cv[i];

        if (!s.level)
        {
            if (v >= cfg_.hi_threshold)
            {
                const uint32_t since = now_us - s.last_rise_us;
                if (cfg_.debounce_us == 0u || since >= cfg_.debounce_us)
                {
                    s.level        = true;
                    s.just_rose    = true;
                    s.last_rise_us = now_us;
                    rising_mask   |= (1u << i);
                }
            }
        }
        else
        {
            if (v <= cfg_.lo_threshold)
            {
                s.level        = false;
                s.just_fell    = true;
                s.last_fall_us = now_us;
                falling_mask  |= (1u << i);
            }
        }
    }

    last_rising_mask_  = rising_mask;
    last_falling_mask_ = falling_mask;
    return rising_mask;
}

bool CvEdge::Level(uint8_t ch) const
{
    return (ch < num_channels_) ? channels_[ch].level : false;
}

uint32_t CvEdge::LastRiseUs(uint8_t ch) const
{
    return (ch < num_channels_) ? channels_[ch].last_rise_us : 0u;
}

uint32_t CvEdge::LastFallUs(uint8_t ch) const
{
    return (ch < num_channels_) ? channels_[ch].last_fall_us : 0u;
}

bool CvEdge::JustRose(uint8_t ch) const
{
    return (ch < num_channels_) ? channels_[ch].just_rose : false;
}

bool CvEdge::JustFell(uint8_t ch) const
{
    return (ch < num_channels_) ? channels_[ch].just_fell : false;
}

} // namespace alchemy
