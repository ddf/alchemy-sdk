/**
 * @file led_strip.h
 * @brief alchemy::ILedStrip — abstract pixel-strip interface.
 *
 * Decouples the framework's LedPanel class from any specific hardware driver.
 * Hardware layers provide a concrete implementation (e.g. Ws2812Strip).
 * Framework code (LedPanel, animations) depends only on this interface so
 * that the framework never includes hardware-specific headers.
 *
 * Lives in hardware/ alongside the hardware types so board drivers
 * (ws2812.h) can inherit from it without pulling in the framework.
 */

#pragma once

#include <cstdint>

namespace alchemy {

class ILedStrip
{
  public:
    virtual ~ILedStrip() = default;

    virtual void SetPixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) = 0;

    virtual void Clear() = 0;

    virtual void Show() = 0;

    virtual bool Busy() const = 0;

    virtual uint16_t NumLeds() const = 0;
};

} // namespace alchemy
