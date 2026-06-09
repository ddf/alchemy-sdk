/**
 * @file pca9557_button.h
 * @brief alchemy::Pca9557Button — B3 wrapper for the V2 GPIO expander.
 *
 * B3 lives on PCA9557 IO6, not an on-MCU GPIO. We can't use libDaisy's
 * `daisy::Switch` (which assumes a GPIO pin) but we still want the same
 * external semantics as alchemy::Button — `Pressed()`, edge accessors,
 * `TimeHeldMs()` — so call sites don't care which side of the I²C bus the
 * button physically lives on.
 *
 * Debounce strategy:
 *   8-sample shift register, identical idea to libDaisy's `Switch`.
 *   `Poll()` reads IO6 over I²C (one register-read transaction) and
 *   shifts the result into a uint8_t. The button is "pressed" once eight
 *   consecutive samples read low (active-low, with an external pull-up).
 *   At the standard 1 ms poll cadence the debounce window is 8 ms.
 *
 * `TimeHeldMs()` is supported via a millisecond counter incremented on
 * every Poll() while the debounced state is pressed.
 *
 * I²C latency: at 400 kHz, a single-byte register read costs ~120 µs.
 * Adding that to a 1 ms ProcessAllControls loop is fine.
 */

#pragma once

#include <cstdint>
#include "alchemy/hw/i_button.h"
#include "alchemy/hw/pca9557.h"
#include "alchemy/hw/alchemy_lab_v2_layout.h"

namespace alchemy {

class Pca9557Button : public IButton
{
  public:
    /** Bind the button to a PCA9557 expander instance and an IO bit. */
    void Init(Pca9557& expander, uint8_t io_bit = kPca9557IoB3)
    {
        expander_  = &expander;
        io_bit_    = io_bit;
        state_     = 0xFFu;  /* assume released at boot */
        pressed_   = false;
        rising_    = false;
        falling_   = false;
        held_ms_   = 0;
    }

    /** Read IO6, shift into debouncer, update edge flags + held counter.
     *  Called by AlchemyLabV2::ProcessAllControls() at 1 ms cadence. */
    void Poll()
    {
        if (expander_ == nullptr) return;

        bool level = true;  /* default = released on I²C failure */
        (void)expander_->ReadInputBit(io_bit_, level);

        /* Shift the 8-sample history. Bit 0 = newest sample.
         * Button is active-low, so "pressed sample" = level == 0. */
        state_ = static_cast<uint8_t>((state_ << 1) | (level ? 1u : 0u));

        const bool now_pressed = (state_ == 0x00u) ? true
                               : (state_ == 0xFFu) ? false
                               : pressed_;  /* hold previous through transients */

        if (!pressed_ && now_pressed)  rising_  = true;
        if ( pressed_ && !now_pressed) falling_ = true;

        pressed_ = now_pressed;
        held_ms_ = pressed_ ? (held_ms_ + 1u) : 0u;
    }

    bool Pressed() const override { return pressed_; }

    bool RisingEdge() override
    {
        const bool e = rising_;
        rising_ = false;
        return e;
    }

    bool FallingEdge() override
    {
        const bool e = falling_;
        falling_ = false;
        return e;
    }

    float TimeHeldMs() const override { return static_cast<float>(held_ms_); }

  private:
    Pca9557*       expander_ = nullptr;
    uint8_t        io_bit_   = kPca9557IoB3;
    uint8_t        state_    = 0xFFu;
    bool           pressed_  = false;
    volatile bool  rising_   = false;
    volatile bool  falling_  = false;
    uint32_t       held_ms_  = 0u;
};

} // namespace alchemy
