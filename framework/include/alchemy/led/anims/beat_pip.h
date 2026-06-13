/**
 * @file alchemy/led/anims/beat_pip.h
 * @brief alchemy::BeatPip — tempo-synced bottom pip.
 *
 * Two regimes, switched at `beat_threshold_ms`:
 *
 *   LONG period (≥ threshold): the pip blinks on-beat.  Each beat it
 *   lights for `on_window_ms` (capped at period/2 so the duty cycle
 *   never exceeds 50 %), then dark until the next beat boundary.
 *
 *   SHORT period (< threshold): blinking faster than ~16 Hz looks
 *   like a solid glow, so the pip transitions smoothly from the
 *   "on" color toward the "solid" color as the period approaches
 *   `min_period_ms`.
 *
 * Per-instance state machine; instantiate one per voice / line.
 * Main-thread only — Tick / Reset / Draw all assume the same caller
 * scope as the LED render path.
 *
 * Usage:
 *
 *   alchemy::BeatPip beat;
 *
 *   // per frame:
 *   if (user_tapped_this_frame) beat.Reset();
 *   beat.Tick(dt_ms, current_period_ms);
 *   beat.Draw(panel, pot_idx);
 *
 * BeatPip carries no concept of where the period came from — feed it
 * milliseconds.  Pair with `alchemy::ClockPll::QuarterSeconds()`, an
 * internal BPM constant, a delay-time pot reading, etc.
 */

#pragma once

#include <cstdint>

#include "alchemy/led/panel.h"

namespace alchemy {

class BeatPip
{
  public:
    struct Config
    {
        /** Period at and above which the pip blinks (default 60 ms).
         *  Below this the pip enters the solid-color-ramp regime. */
        float beat_threshold_ms = 60.0f;

        /** Max time lit per beat (default 30 ms).  Capped internally
         *  at period/2 so the duty cycle never exceeds 50 %. */
        float on_window_ms      = 30.0f;

        /** Period at which the solid-color ramp reaches `solid_color`
         *  (default 1 ms).  Periods between this and `beat_threshold_ms`
         *  interpolate between `on_color` and `solid_color`. */
        float min_period_ms     = 1.0f;
    };

    BeatPip() = default;
    explicit BeatPip(Config cfg) : cfg_(cfg) {}

    /** Replace the config at runtime.  Does NOT reset state — call
     *  Reset() separately if a regime change is wanted. */
    void Configure(Config cfg) { cfg_ = cfg; }

    /**
     * Restart phase from the current beat boundary; light the pip on
     * the next Tick.  Use on tap-fire, MIDI clock reset, gate trigger,
     * etc., when the user expects an immediate visual confirmation
     * aligned to the new beat.
     */
    void Reset();

    /**
     * Per-frame state advance.
     * @param dt_ms      elapsed milliseconds since the previous Tick
     * @param period_ms  current beat period; 0 is valid (forces solid regime)
     */
    void Tick(float dt_ms, float period_ms);

    /**
     * Paint the pip on @p pot's ring at @p hour (default 6 o'clock).
     * Reads cached state from the most recent Tick — call Tick first.
     * Does NOT clear the slot; designed to layer on top of whatever
     * the main render painted.
     */
    void Draw(LedPanel&     panel,
              uint8_t       pot,
              LedPanel::Rgb on_color    = {0xFF, 0xFF, 0xFF},
              LedPanel::Rgb solid_color = {0x00, 0xFF, 0x00},
              float         hour        = 6.0f) const;

  private:
    Config cfg_;
    float  phase_ms_        = 0.0f;
    float  on_remaining_ms_ = 0.0f;
    float  last_period_ms_  = 0.0f;
    bool   was_blinking_    = false;
};

} // namespace alchemy
