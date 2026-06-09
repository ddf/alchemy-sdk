/**
 * @file alchemy_lab_v1.cpp
 * @brief alchemy::AlchemyLabV1 implementation.
 */

#include "alchemy/hw/alchemy_lab_v1.h"

namespace alchemy {

void AlchemyLabV1::Init(daisy::SaiHandle::Config::SampleRate sample_rate,
                  uint32_t                             block_size)
{
    seed.Configure();
    seed.Init();

    /* Set audio parameters before calling AudioCallbackRate(). */
    seed.SetAudioBlockSize(block_size);
    seed.SetAudioSampleRate(sample_rate);

    /* Cache for const accessors — see header. */
    sample_rate_hz_ = seed.AudioSampleRate();
    block_size_     = seed.AudioBlockSize();

    /* ADC: 6 pot channels (0–5) followed by 6 CV channels (6–11). */
    daisy::AdcChannelConfig adcCfg[kNumPots + kNumCvInputs];
    for (uint8_t i = 0; i < kNumPots; i++)
        adcCfg[i].InitSingle(kPotPins[i]);
    for (uint8_t i = 0; i < kNumCvInputs; i++)
        adcCfg[kCvAdcOffset + i].InitSingle(kCvPins[i]);
    seed.adc.Init(adcCfg, kNumPots + kNumCvInputs);
    seed.adc.Start();

    /* AnalogControl for pots: flip=true (CW → 1.0), no slew. */
    const float sr = seed.AudioCallbackRate();
    for (uint8_t i = 0; i < kNumPots; i++)
        pots[i].Init(seed.adc.GetPtr(i), sr, true, false, 0.0f);

    /* AnalogControl for CV inputs: flip=true, no slew. */
    for (uint8_t i = 0; i < kNumCvInputs; i++)
        cv[i].Init(seed.adc.GetPtr(kCvAdcOffset + i), sr, true, false, 0.0f);

    /* Buttons: active-low with internal pull-up via libDaisy default. */
    for (uint8_t i = 0; i < kNumButtons; i++)
        buttons[i].Init(kBtnPins[i]);

    /* LED chain + panel. */
    strip.Init(kLedTotal);
    leds.Init(strip, kAlchemyLabV1Layout);
}

void AlchemyLabV1::ProcessAllControls()
{
    for (uint8_t i = 0; i < kNumPots; i++)
        pots[i].Process();
    for (uint8_t i = 0; i < kNumCvInputs; i++)
        cv[i].Process();
    for (uint8_t i = 0; i < kNumButtons; i++)
        buttons[i].Poll();
}

void AlchemyLabV1::StartAudio(daisy::AudioHandle::AudioCallback cb)
{
    seed.StartAudio(cb);
}

} // namespace alchemy
