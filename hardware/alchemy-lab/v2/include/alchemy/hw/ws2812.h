/**
 * @file ws2812.h
 * @brief alchemy::Ws2812Strip — WS2812B/C LED strip driver for Alchemy Lab V2.
 *
 * V2 LED data pin is **PD11**, which has no TIM channel on STM32H7. The
 * driver uses **TIM6 update event → DMA → GPIOD->BSRR**: each WS2812 bit
 * is three GPIO BSRR writes clocked by TIM6 at 2.4 MHz (3 × 417 ns =
 * 1.25 µs per bit). The cell buffer lives in a dedicated non-cacheable
 * MPU region, so there is no cache-coherence hazard between the CPU's
 * encoding writes and the DMA's reads — by construction, not by per-frame
 * maintenance. See ws2812.cpp for the full design notes.
 *
 * Public API matches V1 exactly so `LedPanel` and all existing animation
 * code work unchanged.
 *
 * Usage:
 *   alchemy::Ws2812Strip strip;
 *   strip.Init(102);
 *   strip.SetPixel(0, 255, 0, 0);
 *   strip.Show();
 */

#pragma once

#include "daisy_seed.h"
#include "alchemy/led/led_strip.h"

namespace alchemy {

/* 102 = the full V2 panel (kLedTotal). Bounded by the 32 KB non-cacheable
 * buffer window: 102·72·4 B cells + 720·4 B latch = 32 256 B ≤ 32 KB. */
constexpr uint16_t kWs2812MaxLeds = 102u;

class Ws2812Strip : public ILedStrip
{
  public:
    Ws2812Strip()  = default;
    ~Ws2812Strip() = default;

    /**
     * Initialise TIM6 + DMA for the WS2812 chain on PD11.
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
