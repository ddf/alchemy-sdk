/**
 * @file alchemy_lab_v2.cpp
 * @brief alchemy::AlchemyLabV2 — Init / ProcessAllControls / StartAudio.
 *
 * Production scope: Seed + audio, ADC (pots + CV), B1/B2 on-MCU buttons,
 * I²C1 + PCA9557 expander + B3, WS2812 + LedPanel, MCP4728 quad DAC,
 * STM32 internal DAC, SDMMC1 + FatFS.
 */

#include "alchemy/hw/alchemy_lab_v2.h"
#include "alchemy/hw/v2_factory_cal.h"

#include "ff.h"

namespace alchemy {

void AlchemyLabV2::Init(daisy::SaiHandle::Config::SampleRate sample_rate,
                        uint32_t                             block_size)
{
    /* 1) Seed bring-up + audio params ─────────────────────────────────── */
    seed.Configure();
    seed.Init();
    seed.SetAudioBlockSize(block_size);
    seed.SetAudioSampleRate(sample_rate);
    sample_rate_hz_ = seed.AudioSampleRate();
    block_size_     = seed.AudioBlockSize();

    /* 1b) Factory calibration hook — B1 + B2 held through boot. Must run
     *     BEFORE the DMA ADC starts: the cal procedure uses its own
     *     one-shot bare-metal ADC configuration on the same pins. The
     *     procedure persists to QSPI and soft-resets; it never returns. */
    if (V2FactoryCalRequested())
        V2RunFactoryCalibrationAndReset(*this);

    /* 1c) Load the per-board cal record from memory-mapped QSPI. On any
     *     validation failure fall back to design constants — user code
     *     keeps working, IsCalibrated() reports the difference. */
    cal_loaded_ = V2CalLoadFromQspi(cal_);
    if (!cal_loaded_)
        V2CalDesignFallback(cal_);

    /* 2) ADC: 6 pot channels followed by 6 CV channels ─────────────────── */
    daisy::AdcChannelConfig adcCfg[kNumPots + kNumCvInputs];
    for (uint8_t i = 0; i < kNumPots; i++)
        adcCfg[i].InitSingle(kPotPins[i]);
    for (uint8_t i = 0; i < kNumCvInputs; i++)
        adcCfg[kCvAdcOffset + i].InitSingle(kCvPins[i]);
    seed.adc.Init(adcCfg, kNumPots + kNumCvInputs);
    seed.adc.Start();

    const float sr = seed.AudioCallbackRate();
    for (uint8_t i = 0; i < kNumPots; i++)
        pots[i].Init(seed.adc.GetPtr(i), sr, kPotPolarityFlipped, false, 0.0f);
    for (uint8_t i = 0; i < kNumCvInputs; i++)
    {
        cv[i].Init(seed.adc.GetPtr(kCvAdcOffset + i), sr, /*flip=*/true,
                   /*invert=*/false, /*slew=*/0.0f);
        /* Per-channel zero + board VDDA. With the design-fallback record
         * this reproduces the uncalibrated transfer exactly, so it is
         * applied unconditionally. */
        cv[i].SetCalibration(cal_.jack[i].adc_zero_code, cal_.vdda_at_cal);
    }

    /* 3) On-MCU buttons (B1, B2) ──────────────────────────────────────── */
    for (uint8_t i = 0; i < kNumOnMcuButtons; i++)
        on_mcu_buttons[i].Init(kOnMcuBtnPins[i]);

    /* 4) I²C1 @ 400 kHz ──────────────────────────────────────────────── */
    daisy::I2CHandle::Config i2c_cfg;
    i2c_cfg.periph          = daisy::I2CHandle::Config::Peripheral::I2C_1;
    i2c_cfg.speed           = daisy::I2CHandle::Config::Speed::I2C_400KHZ;
    i2c_cfg.mode            = daisy::I2CHandle::Config::Mode::I2C_MASTER;
    i2c_cfg.pin_config.scl  = daisy::Pin(daisy::PORTB, 8);
    i2c_cfg.pin_config.sda  = daisy::Pin(daisy::PORTB, 9);
    i2c_ready_ = (i2c.Init(i2c_cfg) == daisy::I2CHandle::Result::OK);

    /* 5) PCA9557 expander ─────────────────────────────────────────────── */
    if (i2c_ready_)
        expander_ready_ = expander.Init(i2c, kPca9557Address);

    /* 6) MCP4728 quad DAC: probe on I²C, seed all four channels to
     *    mid-scale (0 V at the panel after the ±5 V conditioning), pulse
     *    LDAC through the expander to latch the outputs. */
    if (i2c_ready_)
    {
        mcp4728_ready_ = dac.Init(i2c, kMcp4728AddrFirst, kMcp4728AddrLast);
        if (mcp4728_ready_ && expander_ready_)
            dac.PulseLdac(expander, kPca9557IoLdac);
    }

    /* 7) B3 via expander ──────────────────────────────────────────────── */
    if (expander_ready_)
        b3.Init(expander, kPca9557IoB3);

    /* 8) WS2812 strip + LedPanel ──────────────────────────────────────── */
    strip.Init(kLedTotal);
    leds.Init(strip, kAlchemyLabV2Layout);

    /* 9) STM32 internal DAC (DAC1 ch 1 + 2). Both channels seeded to
     *    mid-scale so the analog outputs idle at 0 V. */
    {
        daisy::DacHandle::Config dac_cfg;
        dac_cfg.target_samplerate = 48000u;
        dac_cfg.chn               = daisy::DacHandle::Channel::BOTH;
        dac_cfg.mode              = daisy::DacHandle::Mode::POLLING;
        dac_cfg.bitdepth          = daisy::DacHandle::BitDepth::BITS_12;
        dac_cfg.buff_state        = daisy::DacHandle::BufferState::ENABLED;
        stm_dac_ready_ =
            (stm_dac.Init(dac_cfg) == daisy::DacHandle::Result::OK);
        if (stm_dac_ready_)
        {
            stm_dac.WriteValue(daisy::DacHandle::Channel::ONE, 2048u);
            stm_dac.WriteValue(daisy::DacHandle::Channel::TWO, 2048u);
        }
    }

    /* 10) SDMMC1 @ MEDIUM_SLOW / BITS_1 — most forgiving combo. Standard
     *     (25 MHz) and BITS_4 are reintroducible once 1-bit reads are
     *     proven stable in the field. */
    {
        daisy::SdmmcHandler::Config sd_cfg;
        sd_cfg.Defaults();
        sd_cfg.speed = daisy::SdmmcHandler::Speed::MEDIUM_SLOW;
        sd_cfg.width = daisy::SdmmcHandler::BusWidth::BITS_1;
        const bool sdmmc_ready =
            (sdmmc.Init(sd_cfg) == daisy::SdmmcHandler::Result::OK);
        (void)sdmmc_ready;
    }

    /* 11) FatFs lazy mount on volume "0:". opt=0 defers card init until
     *     first f_open, so a missing card doesn't fail boot. */
    {
        daisy::FatFSInterface::Config ff_cfg;
        ff_cfg.media = daisy::FatFSInterface::Config::MEDIA_SD;
        if (fatfs.Init(ff_cfg) == daisy::FatFSInterface::OK)
        {
            (void)f_mount(&fatfs.GetSDFileSystem(), fatfs.GetSDPath(), 0);
        }
    }
}

bool AlchemyLabV2::SetCvOutVolts(uint8_t jack, float volts)
{
    if (jack >= kNumDacOuts)
        return false;

    /* Invert the per-jack linear fit, clamp to the measured linear range
     * (rail-saturation bounds from the cal sweep; full range on the
     * design fallback). */
    const V2JackCal& jc = cal_.jack[jack];
    float code_f = (volts - jc.dac_offset_v) / jc.dac_gain_v_per_code;
    if (code_f < static_cast<float>(jc.dac_min_linear_code))
        code_f = static_cast<float>(jc.dac_min_linear_code);
    if (code_f > static_cast<float>(jc.dac_max_linear_code))
        code_f = static_cast<float>(jc.dac_max_linear_code);
    const uint16_t code = static_cast<uint16_t>(code_f + 0.5f);

    const DacRoute& route = kDacRouting[jack];
    const uint8_t   src   = static_cast<uint8_t>(route.source);

    if (src < kMcp4728NumChannels)  /* MCP4728 A..D */
    {
        if (!mcp4728_ready_ || !expander_ready_)
            return false;
        mcp_shadow_[src] = code;
        if (!dac.WriteAll(mcp_shadow_[0], mcp_shadow_[1],
                          mcp_shadow_[2], mcp_shadow_[3]))
            return false;
        return dac.PulseLdac(expander, kPca9557IoLdac);
    }

    /* STM32 DAC1 ch1 / ch2 */
    if (!stm_dac_ready_)
        return false;
    stm_dac.WriteValue((src == 4u) ? daisy::DacHandle::Channel::ONE
                                   : daisy::DacHandle::Channel::TWO,
                       code);
    return true;
}

bool AlchemyLabV2::RouteCvOut(uint8_t jack, bool enable)
{
    if (jack >= kNumDacOuts || !expander_ready_)
        return false;
    return expander.SetOutputBit(kDacRouting[jack].select_io,
                                 enable ? kDg411OnLevel : !kDg411OnLevel);
}

void AlchemyLabV2::ProcessAllControls()
{
    for (uint8_t i = 0; i < kNumPots; i++)        pots[i].Process();
    for (uint8_t i = 0; i < kNumCvInputs; i++)    cv[i].Process();
    for (uint8_t i = 0; i < kNumOnMcuButtons; i++) on_mcu_buttons[i].Poll();
    b3.Poll();
}

void AlchemyLabV2::StartAudio(daisy::AudioHandle::AudioCallback cb)
{
    seed.StartAudio(cb);
}

} // namespace alchemy
