/**
 * @file alchemy/hw/button.h
 * @brief alchemy::Button — debounced momentary button with latched edges.
 *
 * Composition over `daisy::Switch`.  Exposes a curated API where edge
 * detection is *consume-on-read* and safe to call from any context —
 * audio ISR, frame hook, OnPoll, or hand-rolled main loop.  Users do
 * not need to write a 1 ms-polled latch themselves; AlchemyLabV1 polls
 * every Button at 1 ms cadence via ProcessAllControls() and persists
 * edge events into per-button latches until they are read.
 *
 * Contract:
 *
 *   - Pressed()      — current debounced state.  Idempotent.
 *   - RisingEdge()   — true if a rising edge has fired since the last
 *                      call to RisingEdge().  Cleared on read.
 *   - FallingEdge()  — symmetrical to RisingEdge().
 *   - TimeHeldMs()   — held time in ms while currently pressed.
 *
 * Call each edge accessor at most once per logical handler.  A second
 * call in the same frame returns false (the edge is consumed by the
 * first read).  This makes the "I saw the press" handoff explicit —
 * no manual `volatile bool` flags, no `OnPoll` glue.
 */

#pragma once

#include "daisy_seed.h"
#include "alchemy/hw/i_button.h"

namespace alchemy {

class Button : public IButton
{
  public:
    /** Initialise the underlying daisy::Switch on the given GPIO pin. */
    void Init(daisy::Pin pin) { sw_.Init(pin); }

    /**
     * Debounce + latch.  Called by AlchemyLabV1::ProcessAllControls()
     * at 1 ms cadence; not intended for direct use.  Each invocation
     * runs daisy::Switch::Debounce() then ORs the transient libDaisy
     * edge state into persistent latches.
     */
    void Poll()
    {
        sw_.Debounce();
        if (sw_.RisingEdge())  rising_  = true;
        if (sw_.FallingEdge()) falling_ = true;
    }

    /** Current debounced press state.  Idempotent; safe from any context. */
    bool Pressed() const override { return sw_.Pressed(); }

    /**
     * True if a rising edge has occurred since the last call.  Cleared
     * on read.  Safe from any context including the audio ISR.
     */
    bool RisingEdge() override
    {
        const bool e = rising_;
        rising_ = false;
        return e;
    }

    /**
     * True if a falling edge has occurred since the last call.  Cleared
     * on read.  Safe from any context including the audio ISR.
     */
    bool FallingEdge() override
    {
        const bool e = falling_;
        falling_ = false;
        return e;
    }

    /** Held time in ms while currently pressed; 0 if not pressed. */
    float TimeHeldMs() const override { return sw_.TimeHeldMs(); }

  private:
    daisy::Switch sw_;
    volatile bool rising_  = false;
    volatile bool falling_ = false;
};

} // namespace alchemy
