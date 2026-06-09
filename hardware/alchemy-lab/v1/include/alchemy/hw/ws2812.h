/**
 * @file ws2812.h
 * @brief alchemy::Ws2812Strip — WS2812B/C LED strip driver for Alchemy Lab V1.
 *
 * Drives a WS2812B/C chain via TIM3 PWM + DMA, zero CPU overhead after Show().
 * Usage:
 *   alchemy::Ws2812Strip strip;
 *   strip.Init(102);
 *   strip.SetPixel(0, 255, 0, 0);
 *   strip.Show();
 */

#pragma once

#include "daisy_seed.h"
#include "alchemy/led/led_strip.h"

/** Maximum number of LEDs supported by one Ws2812Strip instance. */
constexpr uint16_t kWs2812MaxLeds = 128u;

namespace alchemy {

class Ws2812Strip : public ILedStrip
{
  public:
    Ws2812Strip()  = default;
    ~Ws2812Strip() = default;

    /**
     * Initialise TIM3 PWM + DMA for the WS2812 chain.
     *
     * @param num_leds  Chain length (max kWs2812MaxLeds).
     */
    void Init(uint16_t num_leds);

    /* ── ILedStrip ─────────────────────────────────────────────────────── */
    void SetPixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) override;
    void Clear() override;
    void Show() override;
    bool Busy() const override;
    uint16_t NumLeds() const override { return num_leds_; }

  private:
    uint16_t num_leds_ = 0;
    uint8_t  pixel_buf_[kWs2812MaxLeds * 3] = {};
};

} // namespace alchemy
