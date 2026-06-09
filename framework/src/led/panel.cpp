/**
 * @file panel.cpp
 * @brief alchemy::LedPanel implementation.
 */

#include "alchemy/led/panel.h"

#include <cmath>

namespace alchemy {

/* ========================================================================= */
/*  Lifecycle                                                                 */
/* ========================================================================= */

void LedPanel::Init(ILedStrip& strip, const HardwareLayout& layout)
{
    strip_  = &strip;
    layout_ = &layout;
    Clear();
    Show();
}

void LedPanel::Clear()  { if (strip_) strip_->Clear(); }
void LedPanel::Show()   { if (strip_) strip_->Show();  }
bool LedPanel::Busy() const { return strip_ && strip_->Busy(); }

/* ========================================================================= */
/*  Raw chain access                                                          */
/* ========================================================================= */

void LedPanel::SetChain(uint16_t chain_index, const Rgb& c)
{
    if (strip_) strip_->SetPixel(chain_index, c.r, c.g, c.b);
}

/* ========================================================================= */
/*  Pot ring access                                                           */
/* ========================================================================= */

void LedPanel::SetRingByOffset(uint8_t pot_idx, uint8_t ring_offset, const Rgb& c)
{
    if (!strip_ || !layout_ || pot_idx >= layout_->num_pots)
        return;
    const LedRingLayout& r = layout_->rings[pot_idx];
    if (ring_offset >= r.led_count)
        return;
    strip_->SetPixel(r.chain_start + ring_offset, c.r, c.g, c.b);
}

uint8_t LedPanel::RingOffsetForHour(uint8_t pot_idx, float hour) const
{
    if (!layout_ || pot_idx >= layout_->num_pots)
        return 0;
    const LedRingLayout& r = layout_->rings[pot_idx];
    float  steps   = (hour - r.start_hour) / r.step_hours;
    int    rounded = static_cast<int>(std::lround(steps));
    int    wrapped = rounded % static_cast<int>(r.led_count);
    if (wrapped < 0)
        wrapped += static_cast<int>(r.led_count);
    return static_cast<uint8_t>(wrapped);
}

void LedPanel::SetRingByHour(uint8_t pot_idx, float hour, const Rgb& c)
{
    SetRingByOffset(pot_idx, RingOffsetForHour(pot_idx, hour), c);
}

void LedPanel::ClearRing(uint8_t pot_idx)
{
    if (!strip_ || !layout_ || pot_idx >= layout_->num_pots)
        return;
    const LedRingLayout& r = layout_->rings[pot_idx];
    for (uint8_t i = 0; i < r.led_count; i++)
        strip_->SetPixel(r.chain_start + i, 0, 0, 0);
}

/* ========================================================================= */
/*  Button accent LEDs                                                        */
/* ========================================================================= */

void LedPanel::SetButton(uint8_t btn_idx, ButtonLed which, const Rgb& c)
{
    if (!strip_ || !layout_ || btn_idx >= layout_->num_btns)
        return;
    const LedButtonLayout& b   = layout_->buttons[btn_idx];
    const uint16_t         idx = (which == ButtonLed::Top) ? b.top_chain : b.bottom_chain;
    strip_->SetPixel(idx, c.r, c.g, c.b);
}

void LedPanel::SetButtonPair(uint8_t btn_idx, const Rgb& c)
{
    SetButton(btn_idx, ButtonLed::Bottom, c);
    SetButton(btn_idx, ButtonLed::Top,    c);
}

/* ========================================================================= */
/*  Brightness                                                                */
/* ========================================================================= */

void LedPanel::SetBrightness(float k)
{
    if (k < 0.0f) k = 0.0f;
    else if (k > 1.0f) k = 1.0f;
    brightness_ = k;
}

LedPanel::Rgb LedPanel::ScaleGlobal(const Rgb& c) const
{
    return Scale(c, brightness_);
}

/* ========================================================================= */
/*  Static colour helpers                                                     */
/* ========================================================================= */

LedPanel::Rgb LedPanel::Hsv(float h, float s, float v)
{
    h -= std::floor(h);
    if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
    if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;

    const float hh     = h * 6.0f;
    const int   sector = static_cast<int>(hh);
    const float f      = hh - static_cast<float>(sector);
    const float p      = v * (1.0f - s);
    const float q      = v * (1.0f - s * f);
    const float t      = v * (1.0f - s * (1.0f - f));

    float r, g, b;
    switch (sector)
    {
        case 0:  r = v; g = t; b = p; break;
        case 1:  r = q; g = v; b = p; break;
        case 2:  r = p; g = v; b = t; break;
        case 3:  r = p; g = q; b = v; break;
        case 4:  r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return { static_cast<uint8_t>(r * 255.0f + 0.5f),
             static_cast<uint8_t>(g * 255.0f + 0.5f),
             static_cast<uint8_t>(b * 255.0f + 0.5f) };
}

LedPanel::Rgb LedPanel::Scale(const Rgb& c, float k)
{
    if (k < 0.0f) k = 0.0f; else if (k > 1.0f) k = 1.0f;
    return { static_cast<uint8_t>(static_cast<float>(c.r) * k + 0.5f),
             static_cast<uint8_t>(static_cast<float>(c.g) * k + 0.5f),
             static_cast<uint8_t>(static_cast<float>(c.b) * k + 0.5f) };
}

LedPanel::Rgb LedPanel::Mix(const Rgb& a, const Rgb& b, float t)
{
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
    const float u = 1.0f - t;
    return { static_cast<uint8_t>(static_cast<float>(a.r) * u + static_cast<float>(b.r) * t + 0.5f),
             static_cast<uint8_t>(static_cast<float>(a.g) * u + static_cast<float>(b.g) * t + 0.5f),
             static_cast<uint8_t>(static_cast<float>(a.b) * u + static_cast<float>(b.b) * t + 0.5f) };
}

} // namespace alchemy
