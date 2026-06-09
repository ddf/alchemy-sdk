/**
 * @file alchemy_lab_v2.h
 * @brief alchemy::AlchemyLabV2 — Alchemy Lab V2 board class.
 *
 * Production surface (used by the unified `alchemy::AlchemyLab` typedef and
 * shared firmware):
 *   - seed, pots[kNumPots], cv[kNumCvInputs]
 *   - buttons[kNumButtons]   uniform IButton& accessor (B1/B2 on-MCU, B3
 *                            routed through the PCA9557 expander)
 *   - strip, leds            WS2812 + LedPanel
 *   - dac                    MCP4728 quad I²C DAC, jack routing via DG411 + expander
 *   - stm_dac                STM32 internal DAC (DAC1 ch 1 + ch 2)
 *   - sdmmc, fatfs           SDMMC1 4-bit + FatFS volume "0:"
 *   - Init(), ProcessAllControls(), StartAudio()
 *   - Layout(), Arc(), SampleRate(), BlockSize()
 *   - I2cReady(), ExpanderReady()  — peripheral health, queryable at any time
 */

#pragma once

#include "daisy_seed.h"
#include "per/i2c.h"
#include "per/dac.h"
#include "per/sdmmc.h"
#include "sys/fatfs.h"

#include "alchemy/hw/alchemy_lab_v2_layout.h"
#include "alchemy/hw/button.h"
#include "alchemy/hw/cv_input.h"
#include "alchemy/hw/i_button.h"
#include "alchemy/hw/ws2812.h"
#include "alchemy/hw/pca9557.h"
#include "alchemy/hw/pca9557_button.h"
#include "alchemy/hw/mcp4728.h"

#include "alchemy/led/panel.h"

namespace alchemy {

class AlchemyLabV2
{
  public:
    /* ── Hardware peripherals (Daisy idiom: public members) ─────────────── */

    daisy::DaisySeed       seed;
    daisy::AnalogControl   pots[kNumPots];
    CvInput                cv[kNumCvInputs];
    Button                 on_mcu_buttons[kNumOnMcuButtons];   ///< B1, B2
    Pca9557Button          b3;                                 ///< B3 via expander

    struct ButtonArray
    {
        IButton* slots[kNumButtons];
        IButton&       operator[](uint8_t i)       { return *slots[i]; }
        const IButton& operator[](uint8_t i) const { return *slots[i]; }
    };
    ButtonArray buttons {{ &on_mcu_buttons[0], &on_mcu_buttons[1], &b3 }};

    Ws2812Strip            strip;
    LedPanel               leds;

    daisy::I2CHandle       i2c;
    Pca9557                expander;   ///< Active: routes B3 input + DG411 selects.

    Mcp4728                dac;
    daisy::DacHandle       stm_dac;
    daisy::SdmmcHandler    sdmmc;
    daisy::FatFSInterface  fatfs;

    /* ── Lifecycle ─────────────────────────────────────────────────────── */

    /**
     * Initialise hardware in dependency order:
     *   1.  Daisy Seed + audio params
     *   2.  ADC (pots + CV)
     *   3.  On-MCU buttons (B1, B2)
     *   4.  I²C1 @ 400 kHz
     *   5.  PCA9557 expander
     *   6.  MCP4728 quad DAC (seeds mid-scale, pulses LDAC via expander)
     *   7.  B3 via expander
     *   8.  WS2812 strip + LedPanel
     *   9.  STM32 internal DAC (DAC1 ch 1 + 2 seeded mid-scale)
     *   10. SDMMC1 @ MEDIUM_SLOW / BITS_1
     *   11. FatFS lazy mount on volume "0:"
     *
     * Optional peripherals (I²C, expander) record their health in
     * `I2cReady()` / `ExpanderReady()`; Init proceeds even on failure so
     * the board still boots into a usable state for diagnosis.
     */
    void Init(daisy::SaiHandle::Config::SampleRate sample_rate
                  = daisy::SaiHandle::Config::SampleRate::SAI_48KHZ,
              uint32_t block_size = kEngineBlockSamples);

    /**
     * Update all control state. Pots + CV via daisy::AnalogControl::Process,
     * on-MCU buttons via debounce, B3 via I²C-read + software debounce.
     *
     * Call at 1 ms cadence — see V1 docs for the rationale.
     */
    void ProcessAllControls();

    /** Start audio with the given callback. */
    void StartAudio(daisy::AudioHandle::AudioCallback cb);

    /* ── Accessors ─────────────────────────────────────────────────────── */

    const HardwareLayout& Layout() const { return kAlchemyLabV2Layout; }
    const ArcGeometry&    Arc()    const { return kAlchemyLabV2ArcGeometry; }
    float                 SampleRate() const { return sample_rate_hz_; }
    size_t                BlockSize()  const { return block_size_; }

    /** Peripheral health — non-aborting Init() records these so user
     *  firmware can guard before driving the corresponding hardware. */
    bool I2cReady()      const { return i2c_ready_; }
    bool ExpanderReady() const { return expander_ready_; }

  private:
    float    sample_rate_hz_ = 0.0f;
    size_t   block_size_     = 0u;

    bool i2c_ready_      = false;
    bool expander_ready_ = false;
};

} // namespace alchemy
