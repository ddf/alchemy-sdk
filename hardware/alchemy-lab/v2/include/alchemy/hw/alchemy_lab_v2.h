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
 *   - sdmmc, fatfs           SDMMC1 (1-bit, MEDIUM_SLOW) + FatFS volume "0:"
 *   - Init(), ProcessAllControls(), StartAudio()
 *   - Layout(), Arc(), SampleRate(), BlockSize()
 *   - I2cReady(), ExpanderReady(), Mcp4728Ready(), StmDacReady()
 *
 * Calibration (see v2_calibration.h / v2_factory_cal.h):
 *   - Init() loads the per-board V2Calibration record from QSPI; missing
 *     or corrupt records fall back to design constants silently.
 *   - cv[i].Volts() and SetCvOutVolts() are calibrated automatically.
 *   - IsCalibrated() reports whether a real record was loaded.
 *   - Holding B1 + B2 through boot runs the on-board factory cal
 *     procedure and soft-resets (see v2_factory_cal.h).
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
#include "alchemy/hw/v2_calibration.h"

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
     *   1b. Factory-cal hook — B1+B2 held → run cal, persist, reset
     *       (never returns); see v2_factory_cal.h
     *   1c. Load V2Calibration from QSPI (design fallback on failure)
     *   2.  ADC (pots + CV; CV channels get their per-jack calibration)
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

    /* ── Calibrated CV output ──────────────────────────────────────────── */

    /**
     * Drive a CV jack to `volts` using the per-board calibration (or the
     * design transfer when uncalibrated). The request is clamped to the
     * jack's measured linear range — note J3..J6 (MCP4728) physically top
     * out around −4.4 V because the MCP output is VDD-limited; J7/J8
     * (STM DAC) reach the full ±5 V.
     *
     * The jack must be routed (RouteCvOut) for the voltage to reach the
     * panel. @param jack 0..5 = J3..J8.
     * @return false if the backing DAC (or, for MCP jacks, the expander
     *         LDAC path) isn't ready, or `jack` is out of range.
     */
    bool SetCvOutVolts(uint8_t jack, float volts);

    /** Connect / disconnect a jack's DG411 routing switch so its DAC
     *  drives the panel jack. @return false if expander not ready. */
    bool RouteCvOut(uint8_t jack, bool enable);

    /* ── Accessors ─────────────────────────────────────────────────────── */

    const HardwareLayout& Layout() const { return kAlchemyLabV2Layout; }
    const ArcGeometry&    Arc()    const { return kAlchemyLabV2ArcGeometry; }
    float                 SampleRate() const { return sample_rate_hz_; }
    size_t                BlockSize()  const { return block_size_; }

    /** Peripheral health — non-aborting Init() records these so user
     *  firmware can guard before driving the corresponding hardware. */
    bool I2cReady()      const { return i2c_ready_; }
    bool ExpanderReady() const { return expander_ready_; }
    bool Mcp4728Ready()  const { return mcp4728_ready_; }
    bool StmDacReady()   const { return stm_dac_ready_; }

    /** True when a valid per-board cal record was loaded from QSPI.
     *  False means cv[i].Volts() / SetCvOutVolts() run on design-nominal
     *  constants (~3-5 % absolute error). */
    bool IsCalibrated() const { return cal_loaded_; }

    /** The active calibration record (measured or design fallback). */
    const V2Calibration& Calibration() const { return cal_; }

  private:
    float    sample_rate_hz_ = 0.0f;
    size_t   block_size_     = 0u;

    bool i2c_ready_      = false;
    bool expander_ready_ = false;
    bool mcp4728_ready_  = false;
    bool stm_dac_ready_  = false;

    bool          cal_loaded_ = false;
    V2Calibration cal_{};

    /** MCP4728 writes are all-four-channels; shadow the last value per
     *  channel so per-jack SetCvOutVolts doesn't disturb the others. */
    uint16_t mcp_shadow_[kMcp4728NumChannels] = { 2048u, 2048u, 2048u, 2048u };
};

} // namespace alchemy
