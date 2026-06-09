/**
 * @file cv_input.h
 * @brief alchemy::CvInput — Alchemy Lab V2 CV jack wrapper.
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

    void Process() { ac_.Process(); }

    /** Raw normalised reading, 0..1. 0.5 = 0 V at jack, 1.0 = +5 V, 0.0 = −5 V. */
    float Value() const { return ac_.Value(); }

    /** Signed jack voltage. `(Value() − 0.5) × 10`. */
    float Volts() const { return (ac_.Value() - 0.5f) * 10.0f; }

  private:
    daisy::AnalogControl ac_;
};

} // namespace alchemy
