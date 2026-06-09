/**
 * kick_dsp.h — Trigger-driven kick voice.
 *
 * Pitch-swept sine body + hard-clip saturation, optional one-shot transient
 * layer. The DSP knows nothing about VirtualKnob, ControlLoop, or any SDK
 * surface — Params is plain floats + a transient zone index.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include "daisy_seed.h"

namespace kick_dsp {

/** Transient-layer mode. Matches the selector zones on P3 in the example. */
enum Transient : int {
    kTransientNone  = 0,   /* body only                                       */
    kTransientClick = 1,   /* broadband noise burst, ~2 ms decay              */
    kTransientTick  = 2,   /* 3 kHz sine ping,        ~7 ms decay             */
    kTransientKnock = 3,   /* 600 Hz sine burst,      ~12 ms decay            */
    kTransientCount = 4,
};

/**
 * Normalised per-frame parameter values (all 0..1 except transient).
 * The DSP applies its own mapping curves to keep the audio side
 * decoupled from any particular knob layout.
 */
struct Params
{
    float pitch;        /* CCW..CW → kPitchBaseHz..kPitchBaseHz+kPitchSpanHz  */
    float sweep;        /* CCW = fast, CW = slow                              */
    int   transient;    /* 0..3 (Transient enum)                              */
    float drive;        /* 1.0..kDriveMax                                     */
    float decay;        /* exponentially mapped to envelope multiplier        */
    float volume;       /* output gain, applied at the tail                   */
};

/**
 * Render @p n samples into @p out (stereo, summed mono). Drains @p trigger
 * as a single rising-edge fire signal. Returns the post-block envelope
 * value for the LED meter overdraw.
 *
 * @param sr Audio sample rate in Hz (typ. `hw.SampleRate()`).
 *
 * ISR-safe: no allocation, no locking.
 */
float Process(const Params&                     p,
              bool                              trigger,
              daisy::AudioHandle::OutputBuffer  out,
              size_t                            n,
              float                             sr);

} // namespace kick_dsp
