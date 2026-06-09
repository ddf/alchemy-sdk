/**
 * @file alchemy/hw/button.h
 * @brief alchemy::Button — debounced momentary button with latched edges.
 */

#pragma once

#include "daisy_seed.h"
#include "alchemy/hw/i_button.h"

namespace alchemy {

class Button : public IButton
{
  public:
    void Init(daisy::Pin pin) { sw_.Init(pin); }

    void Poll()
    {
        sw_.Debounce();
        if (sw_.RisingEdge())  rising_  = true;
        if (sw_.FallingEdge()) falling_ = true;
    }

    bool Pressed() const override { return sw_.Pressed(); }

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

    float TimeHeldMs() const override { return sw_.TimeHeldMs(); }

  private:
    daisy::Switch sw_;
    volatile bool rising_  = false;
    volatile bool falling_ = false;
};

} // namespace alchemy
