/**
 * @file alchemy_lab_v1.h
 * @brief alchemy::AlchemyLabV1 — Alchemy Lab V1 board class.
 *
 * AlchemyLabV1 is the board class for Alchemy Lab V1.  It composes all
 * libDaisy hardware primitives as public members in the Daisy style.
 */

#pragma once

#include "daisy_seed.h"
#include "alchemy/hw/alchemy_lab_v1_layout.h"
#include "alchemy/hw/button.h"
#include "alchemy/hw/cv_input.h"
#include "alchemy/hw/ws2812.h"
#include "alchemy/led/panel.h"

namespace alchemy {

class AlchemyLabV1
{
  public:
    daisy::DaisySeed seed;
    daisy::AnalogControl pots[kNumPots];
    CvInput cv[kNumCvInputs];
    Button buttons[kNumButtons];
    Ws2812Strip strip;

    LedPanel leds;

    /* ── Lifecycle ─────────────────────────────────────────────────────── */

    /**
     * Initialise all hardware: Daisy Seed, ADC (12 channels), buttons,
     * WS2812 LED strip, and LedPanel.  Audio is NOT started here.
     *
     * @param sample_rate  SAI sample rate.  Defaults to 48 kHz; pass
     *                     SAI_96KHZ, SAI_32KHZ, etc. to override.
     *                     Read the actual rate back with SampleRate() after Init().
     * @param block_size   Audio block size in samples (default: kEngineBlockSamples).
     *                     Smaller blocks reduce latency; larger blocks reduce CPU overhead.
     *
     * Call once from main() before constructing any surfaces that touch hardware.
     */
    void Init(daisy::SaiHandle::Config::SampleRate sample_rate
                  = daisy::SaiHandle::Config::SampleRate::SAI_48KHZ,
              uint32_t block_size = kEngineBlockSamples);


    void ProcessAllControls();

    /**
     * Start the audio interrupt with the given callback.
     *
     * @param cb  libDaisy audio callback (ISR context; must be real-time safe).
     */
    void StartAudio(daisy::AudioHandle::AudioCallback cb);

    /* ── Accessors ─────────────────────────────────────────────────────── */

    /** Hardware layout (ring/button geometry and counts). */
    const HardwareLayout& Layout() const { return kAlchemyLabV1Layout; }

    /** Standard arc geometry for this board's pot rings. */
    const ArcGeometry& Arc() const { return kAlchemyLabV1ArcGeometry; }
    float SampleRate() const { return sample_rate_hz_; }
    size_t BlockSize() const { return block_size_; }

  private:
    float  sample_rate_hz_ = 0.0f;  ///< Cached from seed.AudioSampleRate() at Init().
    size_t block_size_     = 0u;    ///< Cached from seed.AudioBlockSize()  at Init().
};

} // namespace alchemy
