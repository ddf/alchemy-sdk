/**
 * @file cv_input.h
 * @brief alchemy::CvInput — Alchemy Lab V1 CV jack wrapper.
 *
 * Thin wrapper over `daisy::AnalogControl` that encodes the V1 panel's
 * bipolar ±5 V CV conditioning in the type.  `Value()` is forwarded
 * unchanged for callers that want the raw normalised ADC reading;
 * `Volts()` converts to signed jack volts so musical code never has to
 * carry the `(raw - 0.5f) * 10.0f` convention in prose comments.
 *
 *   hw.cv[3].Value()  → 0..1   (0.5 at 0 V, 1.0 at +5 V, 0.0 at -5 V)
 *   hw.cv[3].Volts()  → ±5 V   (0.0 at 0 V; signed, in jack volts)
 *
 */

#pragma once

#include "daisy_seed.h"

namespace alchemy {

class CvInput
{
  public:
    void Init(uint16_t* adcptr, float sample_rate, bool flip, bool invert, float slew)
    {
        ac_.Init(adcptr, sample_rate, flip, invert, slew);
    }

    /** Update the smoothed reading.  Call at the AnalogControl sample rate
     *  (see AlchemyLabV1::ProcessAllControls). */
    void Process() { ac_.Process(); }

    /** Raw normalised reading, 0..1.  With V1 bipolar conditioning:
     *  0.5 = 0 V at jack, 1.0 = +5 V, 0.0 = −5 V. */
    float Value() const { return ac_.Value(); }

    /** Signed jack voltage in volts, derived from the V1 panel's
     *  ±5 V bipolar conditioning: `(Value() - 0.5) * 10`.
     *
     *  Returns 0 V at rest, +5 V at full positive swing, −5 V at full
     *  negative swing.  TODO update for calibration when done. */
    float Volts() const { return (ac_.Value() - 0.5f) * 10.0f; }

  private:
    daisy::AnalogControl ac_;
};

} // namespace alchemy
