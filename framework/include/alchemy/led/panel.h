/**
 * @file panel.h
 * @brief alchemy::LedPanel — hardware-layout-aware LED panel class.
 *
 * LedPanel wraps an ILedStrip with addressing helpers that understand the
 * physical panel described by a HardwareLayout:
 *
 *   - N pot rings, each with its own chain start, LED count, and winding.
 *   - M buttons, each with a TOP and BOTTOM accent LED.
 *
 * LedPanel holds a reference to an ILedStrip and a pointer to a
 * HardwareLayout — it does not own either.  The strip and layout must
 * outlive the LedPanel instance.
 *
 * LedPanel is a normal class: default-constructible, then Init() before use.
 * Typically owned as a member variable (e.g. inside the AlchemyLab board
 * class) and consumed by surface classes like alchemy::LedRender.
 *
 * Usage:
 *   alchemy::Ws2812Strip  strip;
 *   alchemy::LedPanel     leds;
 *   strip.Init(alchemy::kLedTotal);
 *   leds.Init(strip, alchemy::kAlchemyLabLayout);
 *
 *   leds.Clear();
 *   leds.SetRingByHour(pot_idx, hour, colour);
 *   leds.Show();
 */

#pragma once

#include <cstdint>
#include "alchemy/hardware_types.h"
#include "alchemy/led/led_strip.h"

namespace alchemy {

/**
 * Hardware-layout-aware LED panel.
 *
 * Does not include any hardware driver headers — works with any
 * ILedStrip implementation.
 */
class LedPanel
{
  public:

    /* ── Colour types ─────────────────────────────────────────────────── */

    /** Single 8-bit RGB triplet. */
    struct Rgb { uint8_t r, g, b; };

    /** Button accent LED selector. */
    enum class ButtonLed : uint8_t { Bottom = 0, Top = 1 };

    /* ── Lifecycle ────────────────────────────────────────────────────── */

    LedPanel() = default;

    /**
     * Bind the panel to a strip and a hardware layout.
     *
     * The strip should already be initialised (strip.Init() called).
     * Calls Clear() and Show() to blank the chain on startup.
     *
     * @param strip   Initialised LED strip implementation.
     * @param layout  Board hardware layout (rings, buttons, chain positions).
     */
    void Init(ILedStrip& strip, const HardwareLayout& layout);

    /* ── Frame lifecycle ──────────────────────────────────────────────── */

    /** Zero every pixel (does not transmit). */
    void Clear();

    /**
     * Push the current frame to the strip.
     * Returns immediately; the strip transmits asynchronously via DMA.
     */
    void Show();

    /** True while a DMA transmission is in progress. */
    bool Busy() const;

    /* ── Raw chain access ─────────────────────────────────────────────── */

    /** Set any pixel by its 0-based chain index. */
    void SetChain(uint16_t chain_index, const Rgb& c);

    /* ── Pot ring access ──────────────────────────────────────────────── */

    /**
     * Set a pixel on a pot ring by its in-ring offset (0..led_count-1).
     * Useful for spinner animations that don't need absolute orientation.
     */
    void SetRingByOffset(uint8_t pot_idx, uint8_t ring_offset, const Rgb& c);

    /**
     * Convert a clock-face hour to the nearest chain offset on a ring.
     * 0 = noon, 3 = 3 o'clock, 6 = 6 o'clock, 9 = 9 o'clock.
     */
    uint8_t RingOffsetForHour(uint8_t pot_idx, float hour) const;

    /** Set the pixel on a pot ring closest to the given clock-face hour. */
    void SetRingByHour(uint8_t pot_idx, float hour, const Rgb& c);

    /** Clear every pixel on a single pot ring. */
    void ClearRing(uint8_t pot_idx);

    /* ── Button accent LEDs ───────────────────────────────────────────── */

    /** Set the TOP or BOTTOM accent LED for the given button. */
    void SetButton(uint8_t btn_idx, ButtonLed which, const Rgb& c);

    /** Set both TOP and BOTTOM accent LEDs for the given button. */
    void SetButtonPair(uint8_t btn_idx, const Rgb& c);

    /* ── Instance brightness ──────────────────────────────────────────── */

    /**
     * Set this panel's global brightness scalar (0..1).
     * Used by ScaleGlobal(); applied per-pixel on every lit LED.
     */
    void  SetBrightness(float k);
    float GetBrightness() const { return brightness_; }

    /**
     * Scale @p c by this panel's current brightness.
     * Call before passing colours to SetRingByOffset / SetChain.
     */
    Rgb ScaleGlobal(const Rgb& c) const;

    /* ── Static colour helpers ────────────────────────────────────────── */

    /** Convert HSV (0..1 each) to RGB (0..255). H wraps; S/V are clamped. */
    static Rgb Hsv(float h, float s, float v);

    /** Multiply each channel by k (0..1, clamped). */
    static Rgb Scale(const Rgb& c, float k);

    /** Linear blend: t=0 → a, t=1 → b. */
    static Rgb Mix(const Rgb& a, const Rgb& b, float t);

  private:
    ILedStrip*            strip_      = nullptr;
    const HardwareLayout* layout_     = nullptr;
    /* Default chosen so a fresh module is comfortable in a typical
     * room — full-bright on a 16-LED ring is genuinely uncomfortable
     * at arm's length.  Users opt up via SetBrightness() (often from
     * a settings page).  Existing modules that explicitly call
     * SetBrightness() are unaffected.                                */
    float                 brightness_ = 0.3f;
};

} // namespace alchemy
