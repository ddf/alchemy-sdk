/**
 * @file trigger_jack.h
 * @brief alchemy::TriggerJack — codec-input edge detector for J1, J2.
 *
 * J1 and J2 are the audio codec's stereo inputs. They are AC-coupled
 * (10 µF / 66.5k → 0.24 Hz HPF), which blocks DC and rules out absolute
 * voltage reads. Rising edges pass through cleanly, so these jacks make
 * good clock / trigger inputs.
 *
 * Detection runs inside the audio callback shim installed by
 * AlchemyLabV2::StartAudio(): every input sample is compared to a
 * threshold and a rising edge sets a latch. The user polls RisingEdge()
 * at control rate (typically 1 kHz inside ProcessAllControls). The latch
 * is cleared on read.
 *
 * Threshold is expressed in panel volts via SetTriggerThreshold(); the
 * jack converts internally using the standard Daisy codec input scaling
 * (±5 V at the panel ↔ ±1.0 normalised sample). Default is 1.0 V — safe
 * for any Eurorack-grade trigger source (typically 5–10 V).
 */

#pragma once

#include <cstddef>

namespace alchemy {

class AlchemyLabV2;  // friend for ProcessBlock + audio shim access

class TriggerJack
{
  public:
    /** True iff a rising edge crossed the threshold since the last call.
     *  Latched across audio blocks and cleared on read. Poll at control
     *  rate; you won't miss a trigger as long as polls are faster than
     *  the inter-trigger interval. */
    bool RisingEdge();

    /** Threshold in panel volts. Default 1.0 V. Lower it for weak
     *  trigger sources; raise it if a noisy line is firing false
     *  triggers. Clamped internally to (0, 5] V. */
    void SetTriggerThreshold(float volts_at_jack);

  private:
    friend class AlchemyLabV2;

    /** Called by the audio shim with the channel's input block. Sample-
     *  by-sample compares to threshold; sets latched_ on rising edge. */
    void ProcessBlock(const float* samples, size_t n);

    float threshold_sample_ = 0.2f;   /* = 1.0 V / 5.0 V fullscale */
    bool  prev_above_       = false;
    bool  latched_          = false;
};

} // namespace alchemy
