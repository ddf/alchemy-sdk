/**
 * @file v2_factory_cal.cpp
 * @brief V2 factory calibration — bare-metal ADC, loopback sweep, QSPI persist.
 *
 * Ported from the bench-validated `cal_writer` firmware
 * (alchemy-lab-v2-cal-sanity/cal_writer), minus USB logging. The numeric
 * procedure is identical:
 *
 *   1. VREFINT pass — ADC3 channel 19, 500-sample average, VDDA via the
 *      factory cal word at 0x1FF1E860. Out-of-range result falls back to
 *      an assumed 3.30 V (recorded in vdda_source).
 *   2. ADC zero pass — DG411s open, 500-sample average per jack.
 *   3. DAC sweep per jack — route via DG411, 17 codes across [0, 4095],
 *      500 samples per point, 5 ms settle. Jack volts derived from the
 *      MEASURED per-channel zero (keeps input-bias tolerance out of the
 *      fit). Linear fit over the trimmed middle; usable range found by
 *      walking outward until a point saturates (>100 mV off the fit).
 *   4. Persist — CRC32, QSPI sector erase + write + byte-verify.
 *   5. Wait for button release, NVIC_SystemReset().
 *
 * Bare-metal ADC notes (same rationale as the bench firmware):
 *   - One-shot polled conversions, 16-bit, 810.5-cycle sampling.
 *   - PCSEL and SMPRx are written directly: the HAL channel-config path
 *     requires LL-encoded channel macros and silently corrupts PCSEL when
 *     given raw integers (and the reverse mistake — an LL macro masked
 *     with `& 0x1F` — silently selects channel 0; both bit us during
 *     bring-up).
 *   - VREFINT's channel number is decoded from ADC_CHANNEL_VREFINT rather
 *     than hardcoded, via ADC_CHANNEL_ID_NUMBER_MASK.
 */

#include "alchemy/hw/v2_factory_cal.h"

#include <cmath>
#include <cstring>

#include "daisy_seed.h"
#include "stm32h7xx_hal.h"

#include "alchemy/hw/alchemy_lab_v2.h"
#include "alchemy/hw/v2_calibration.h"

namespace alchemy {

namespace {

/* ── Tunables (bench-validated values from Phase 1) ───────────────────── */

constexpr uint32_t kSamplesPerPoint = 500u;
constexpr uint32_t kSweepPoints     = 17u;
constexpr uint32_t kSweepTrim       = 2u;     /* fit excludes N pts each end */
constexpr uint32_t kDacSettleMs     = 5u;
constexpr double   kSatResidualV    = 0.100;  /* saturation threshold        */

constexpr uint32_t kVrefintCalAddr  = 0x1FF1E860u;  /* H750 factory word    */
constexpr float    kVrefintCalVdda  = 3.30f;        /* condition of the word */
constexpr double   kAdcMaxCode      = 65535.0;

/* ── Bare-metal one-shot ADC (ADC1 jacks + ADC3 VREFINT) ─────────────── */

ADC_HandleTypeDef s_hadc1 = {};
ADC_HandleTypeDef s_hadc3 = {};

bool AdcInitOne(ADC_HandleTypeDef& h, ADC_TypeDef* instance)
{
    h.Instance                      = instance;
    h.Init.ClockPrescaler           = ADC_CLOCK_ASYNC_DIV2;
    h.Init.Resolution               = ADC_RESOLUTION_16B;
    h.Init.ScanConvMode             = ADC_SCAN_DISABLE;
    h.Init.EOCSelection             = ADC_EOC_SINGLE_CONV;
    h.Init.LowPowerAutoWait         = DISABLE;
    h.Init.ContinuousConvMode       = DISABLE;
    h.Init.NbrOfConversion          = 1;
    h.Init.DiscontinuousConvMode    = DISABLE;
    h.Init.ExternalTrigConv         = ADC_SOFTWARE_START;
    h.Init.ExternalTrigConvEdge     = ADC_EXTERNALTRIGCONVEDGE_NONE;
    h.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    h.Init.Overrun                  = ADC_OVR_DATA_OVERWRITTEN;
    h.Init.LeftBitShift             = ADC_LEFTBITSHIFT_NONE;
    h.Init.OversamplingMode         = DISABLE;

    if (HAL_ADC_Init(&h) != HAL_OK) return false;
    return HAL_ADCEx_Calibration_Start(&h, ADC_CALIB_OFFSET_LINEARITY,
                                       ADC_SINGLE_ENDED) == HAL_OK;
}

bool CalAdcInit()
{
    /* Jack ADC pins to analog mode (daisy GPIO handles port clocks). */
    for (uint8_t i = 0; i < kNumCvInputs; ++i)
    {
        daisy::GPIO g;
        g.Init(kCvPins[i], daisy::GPIO::Mode::ANALOG);
    }

    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();

    if (!AdcInitOne(s_hadc1, ADC1)) return false;
    if (!AdcInitOne(s_hadc3, ADC3)) return false;

    /* PCSEL + max sampling time, written directly (see file header). */
    uint32_t pcsel = 0;
    for (uint8_t i = 0; i < kNumCvInputs; ++i)
        pcsel |= (1u << kCvAdc1Channels[i]);
    ADC1->PCSEL = pcsel;
    ADC1->SMPR1 = 0x3FFFFFFFu;  /* 810.5 cycles on channels 0-9   */
    ADC1->SMPR2 = 0x3FFFFFFFu;  /* 810.5 cycles on channels 10-19 */

    /* VREFINT on ADC3: decode the channel from the HAL macro. */
    constexpr uint32_t vrefint_ch =
        (ADC_CHANNEL_VREFINT & ADC_CHANNEL_ID_NUMBER_MASK)
        >> ADC_CHANNEL_ID_NUMBER_BITOFFSET_POS;
    ADC3->PCSEL      |= (1u << vrefint_ch);
    ADC3->SMPR2      |= (0b111u << ((vrefint_ch - 10u) * 3u));
    ADC3_COMMON->CCR |= ADC_CCR_VREFEN;
    return true;
}

bool AdcReadSingle(ADC_HandleTypeDef& h, uint32_t channel, uint16_t& out)
{
    h.Instance->SQR1 = ((channel & 0x1Fu) << ADC_SQR1_SQ1_Pos);
    if (HAL_ADC_Start(&h) != HAL_OK)                  return false;
    if (HAL_ADC_PollForConversion(&h, 100) != HAL_OK) return false;
    out = static_cast<uint16_t>(HAL_ADC_GetValue(&h));
    HAL_ADC_Stop(&h);
    return true;
}

bool ReadJackRaw(uint8_t jack, uint16_t& out)
{
    return AdcReadSingle(s_hadc1, kCvAdc1Channels[jack], out);
}

bool ReadVrefintRaw(uint16_t& out)
{
    constexpr uint32_t ch =
        (ADC_CHANNEL_VREFINT & ADC_CHANNEL_ID_NUMBER_MASK)
        >> ADC_CHANNEL_ID_NUMBER_BITOFFSET_POS;
    return AdcReadSingle(s_hadc3, ch, out);
}

/* ── Stats + fit ─────────────────────────────────────────────────────── */

bool SampleMean(bool (*reader)(uint8_t, uint16_t&), uint8_t arg,
                uint32_t n, double& mean_out)
{
    double sum = 0.0;
    for (uint32_t i = 0; i < n; ++i)
    {
        uint16_t code = 0;
        if (!reader(arg, code)) return false;
        sum += static_cast<double>(code);
    }
    mean_out = sum / static_cast<double>(n);
    return true;
}

bool VrefintReader(uint8_t /*unused*/, uint16_t& out) { return ReadVrefintRaw(out); }

void LinearFit(const double* x, const double* y, uint32_t n,
               double& a, double& b)
{
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    for (uint32_t i = 0; i < n; ++i)
    {
        sx += x[i];  sy += y[i];
        sxx += x[i] * x[i];
        sxy += x[i] * y[i];
    }
    const double denom = static_cast<double>(n) * sxx - sx * sx;
    a = (denom == 0.0) ? 0.0
                       : (static_cast<double>(n) * sxy - sx * sy) / denom;
    b = (sy - a * sx) / static_cast<double>(n);
}

/* ── DAC + routing helpers (drive the board's own members) ───────────── */

uint16_t s_mcp_shadow[4] = { 2048u, 2048u, 2048u, 2048u };

void WriteJackDac(AlchemyLabV2& hw, uint8_t jack, uint16_t code)
{
    code = static_cast<uint16_t>(code & 0x0FFFu);
    const DacRoute& r = kDacRouting[jack];
    const uint8_t src = static_cast<uint8_t>(r.source);
    if (src < 4u)
    {
        s_mcp_shadow[src] = code;
        hw.dac.WriteAll(s_mcp_shadow[0], s_mcp_shadow[1],
                        s_mcp_shadow[2], s_mcp_shadow[3]);
        hw.dac.PulseLdac(hw.expander, kPca9557IoLdac);
    }
    else
    {
        hw.stm_dac.WriteValue((src == 4u) ? daisy::DacHandle::Channel::ONE
                                          : daisy::DacHandle::Channel::TWO,
                              code);
    }
}

void Dg411AllOff(AlchemyLabV2& hw)
{
    hw.expander.WriteOutputs(kPca9557OutputBoot);
}

/* ── QSPI persist (same dance as the bench cal_writer) ───────────────── */

daisy::QSPIHandle::Config QspiConfig(daisy::QSPIHandle::Config::Mode mode)
{
    /* Mirrors DaisySeed::ConfigureQspi — fixed pins on the Seed module. */
    daisy::QSPIHandle::Config q;
    q.device = daisy::QSPIHandle::Config::Device::IS25LP064A;
    q.mode   = mode;
    q.pin_config.io0 = daisy::Pin(daisy::PORTF, 8);
    q.pin_config.io1 = daisy::Pin(daisy::PORTF, 9);
    q.pin_config.io2 = daisy::Pin(daisy::PORTF, 7);
    q.pin_config.io3 = daisy::Pin(daisy::PORTF, 6);
    q.pin_config.clk = daisy::Pin(daisy::PORTF, 10);
    q.pin_config.ncs = daisy::Pin(daisy::PORTG, 6);
    return q;
}

bool PersistToQspi(AlchemyLabV2& hw, const V2Calibration& cal)
{
    auto& qspi = hw.seed.qspi;

    auto indirect = QspiConfig(daisy::QSPIHandle::Config::Mode::INDIRECT_POLLING);
    if (qspi.Init(indirect) != daisy::QSPIHandle::Result::OK) return false;

    const uint32_t end = kV2CalQspiAddr + kV2CalQspiSectorSize - 1u;
    if (qspi.Erase(kV2CalQspiAddr, end) != daisy::QSPIHandle::Result::OK)
        return false;

    if (qspi.Write(kV2CalQspiAddr, sizeof(cal),
                   reinterpret_cast<uint8_t*>(
                       const_cast<V2Calibration*>(&cal)))
        != daisy::QSPIHandle::Result::OK)
        return false;

    auto mapped = QspiConfig(daisy::QSPIHandle::Config::Mode::MEMORY_MAPPED);
    if (qspi.Init(mapped) != daisy::QSPIHandle::Result::OK) return false;

    /* Verify through the freshly-invalidated memory-mapped alias. */
    V2Calibration readback{};
    if (!V2CalLoadFromQspi(readback)) return false;
    return std::memcmp(&cal, &readback, sizeof(cal)) == 0;
}

/* ── Panel feedback ──────────────────────────────────────────────────── */
/*
 * The 102-LED panel narrates the whole procedure. Ring k mirrors jack k
 * (ring 0 ↔ J3 … ring 5 ↔ J8):
 *
 *   boot ack   : B1 + B2 accent pairs white (your hold registered)
 *   VREFINT    : all button pairs amber
 *   zero pass  : rings fill BLUE one after another (wave across panel)
 *   DAC sweeps : active jack's ring fills AMBER point-by-point with a
 *                bright head pixel; finished jacks hold dim GREEN
 *   persist    : button pairs white (panel holds latched frame — the
 *                WS2812s keep displaying with zero data traffic while
 *                QSPI leaves memory-mapped mode)
 *   success    : whole panel flashes green ×3, holds green till release
 *   failure    : whole panel dim red; the jack that failed (if any)
 *                blinks bright red — walk straight to the bad channel
 *
 * Frames are pushed during DAC settle windows (DMA ~3 ms < 5 ms settle),
 * so the LED data line is always quiet while the ADC samples.
 *
 * Colours are pre-dimmed: full-panel holds stay under ~300 mA.
 */

using Rgb = LedPanel::Rgb;

constexpr Rgb kLedAmber    {120,  60,   0};
constexpr Rgb kLedAmberHead{210, 120,  10};
constexpr Rgb kLedBlue     { 16,  40, 120};
constexpr Rgb kLedGreen    {  0, 140,  30};
constexpr Rgb kLedGreenHold{  0,  48,  10};
constexpr Rgb kLedRed      {170,  16,  16};
constexpr Rgb kLedRedDim   { 40,   6,   6};
constexpr Rgb kLedWhite    { 70,  70,  70};

/* Which jack was being swept when a failure occurred (0xFF = none). */
uint8_t s_fail_jack = 0xFFu;

void LedsShow(AlchemyLabV2& hw)
{
    while (hw.leds.Busy())
        daisy::System::Delay(1);
    hw.leds.Show();
}

void RingSolid(AlchemyLabV2& hw, uint8_t ring, const Rgb& c)
{
    for (uint8_t off = 0; off < kLedsPerRing; ++off)
        hw.leds.SetRingByOffset(ring, off, c);
}

void AllRings(AlchemyLabV2& hw, const Rgb& c)
{
    for (uint8_t r = 0; r < kNumLedRings; ++r)
        RingSolid(hw, r, c);
}

void AllButtons(AlchemyLabV2& hw, const Rgb& c)
{
    for (uint8_t b = 0; b < kNumButtons; ++b)
        hw.leds.SetButtonPair(b, c);
}

/** Active-sweep ring: n_lit progress pixels, brighter head. */
void RingProgress(AlchemyLabV2& hw, uint8_t ring, uint8_t n_lit)
{
    hw.leds.ClearRing(ring);
    for (uint8_t off = 0; off < n_lit && off < kLedsPerRing; ++off)
        hw.leds.SetRingByOffset(ring, off,
                                (off + 1u == n_lit) ? kLedAmberHead : kLedAmber);
}

[[noreturn]] void FailForever(AlchemyLabV2& hw)
{
    /* Whole panel dim red; the failing jack's ring (when known) blinks
     * bright red so the bad channel is identifiable from across the
     * room. The previous QSPI record is only touched after every sweep
     * succeeded, so failure here leaves earlier calibration intact
     * (a torn QSPI write fails CRC on next boot → clean fallback). */
    bool phase = false;
    while (true)
    {
        hw.leds.Clear();
        AllRings(hw, kLedRedDim);
        AllButtons(hw, kLedRed);
        if (s_fail_jack < kNumLedRings && phase)
            RingSolid(hw, s_fail_jack, kLedRed);
        LedsShow(hw);
        hw.seed.SetLed(phase);
        phase = !phase;
        daisy::System::Delay(160);
    }
}

/* ── The procedure ───────────────────────────────────────────────────── */

bool RunProcedure(AlchemyLabV2& hw, V2Calibration& out)
{
    std::memset(&out, 0, sizeof(out));
    out.magic          = kV2CalMagic;
    out.schema_version = kV2CalSchemaVersion;

    /* Pass 1: VREFINT → VDDA.  Panel: button pairs amber. */
    AllButtons(hw, kLedAmber);
    LedsShow(hw);
    daisy::System::Delay(10);   /* let the frame finish before sampling */

    double vrefint_mean = 0.0;
    if (!SampleMean(VrefintReader, 0u, kSamplesPerPoint, vrefint_mean))
        return false;
    const uint16_t cal_word =
        *reinterpret_cast<const volatile uint16_t*>(kVrefintCalAddr);
    const double vdda = static_cast<double>(kVrefintCalVdda)
                        * static_cast<double>(cal_word) / vrefint_mean;
    if (vdda >= 3.0 && vdda <= 3.6)
    {
        out.vdda_at_cal = static_cast<float>(vdda);
        out.vdda_source = kV2VddaMeasured;
    }
    else
    {
        out.vdda_at_cal = kV2VddaDesign;
        out.vdda_source = kV2VddaAssumed;
    }

    /* Pass 2: per-jack ADC zero (jacks open, DG411s open).
     * Panel: rings fill blue one-by-one — a wave across the panel. */
    Dg411AllOff(hw);
    daisy::System::Delay(20);
    for (uint8_t j = 0; j < kV2CalNumJacks; ++j)
    {
        double mean = 0.0;
        if (!SampleMean(ReadJackRaw, j, kSamplesPerPoint, mean)) return false;
        out.jack[j].adc_zero_code = static_cast<uint16_t>(mean + 0.5);

        RingSolid(hw, j, kLedBlue);
        LedsShow(hw);
        daisy::System::Delay(5);  /* frame DMA done before next sample burst */
    }

    /* Pass 3: DAC sweep per jack.
     * Panel: the active jack's ring fills amber point-by-point (bright
     * head pixel); each finished jack holds dim green. Frames go out
     * during the DAC settle window so the LED data line is quiet while
     * the ADC samples. */
    const double vdda_d = static_cast<double>(out.vdda_at_cal);
    for (uint8_t j = 0; j < kV2CalNumJacks; ++j)
    {
        s_fail_jack = j;
        Dg411AllOff(hw);
        if (!hw.expander.SetOutputBit(kDacRouting[j].select_io, kDg411OnLevel))
            return false;

        const double v_pin_zero =
            vdda_d * static_cast<double>(out.jack[j].adc_zero_code) / kAdcMaxCode;

        double xs[kSweepPoints] = {};
        double ys[kSweepPoints] = {};
        for (uint32_t k = 0; k < kSweepPoints; ++k)
        {
            const uint16_t code = static_cast<uint16_t>(
                (k * 4095u + (kSweepPoints - 1u) / 2u) / (kSweepPoints - 1u));
            WriteJackDac(hw, j, code);

            const uint8_t fill = static_cast<uint8_t>(
                ((k + 1u) * kLedsPerRing) / kSweepPoints);
            RingProgress(hw, j, fill);
            LedsShow(hw);                       /* ~3 ms DMA…           */
            daisy::System::Delay(kDacSettleMs); /* …inside 5 ms settle  */

            double mean = 0.0;
            if (!SampleMean(ReadJackRaw, j, kSamplesPerPoint, mean))
            {
                Dg411AllOff(hw);
                return false;
            }
            const double v_pin = vdda_d * mean / kAdcMaxCode;
            xs[k] = static_cast<double>(code);
            ys[k] = (v_pin_zero - v_pin)
                    / static_cast<double>(kV2CvInGainDesign);
        }

        double a = 0.0, b = 0.0;
        LinearFit(xs + kSweepTrim, ys + kSweepTrim,
                  kSweepPoints - 2u * kSweepTrim, a, b);
        if (a == 0.0) { Dg411AllOff(hw); return false; }

        /* Usable range: widen from the trimmed region until saturation. */
        const uint32_t last = kSweepPoints - 1u;
        uint32_t lo = kSweepTrim;
        while (lo > 0u
               && std::fabs(ys[lo - 1u] - (a * xs[lo - 1u] + b)) <= kSatResidualV)
            --lo;
        uint32_t hi = last - kSweepTrim;
        while (hi < last
               && std::fabs(ys[hi + 1u] - (a * xs[hi + 1u] + b)) <= kSatResidualV)
            ++hi;

        out.jack[j].dac_gain_v_per_code = static_cast<float>(a);
        out.jack[j].dac_offset_v        = static_cast<float>(b);
        out.jack[j].dac_min_linear_code =
            (lo == 0u) ? 0u : static_cast<uint16_t>(xs[lo] + 0.5);
        out.jack[j].dac_max_linear_code =
            (hi == last) ? 4095u : static_cast<uint16_t>(xs[hi] + 0.5);

        /* Park this DAC at mid-scale and disconnect before the next jack;
         * its ring holds dim green = done. */
        WriteJackDac(hw, j, 2048u);
        RingSolid(hw, j, kLedGreenHold);
        LedsShow(hw);
    }
    Dg411AllOff(hw);
    s_fail_jack = 0xFFu;

    out.crc32 = V2CalComputeCrc(out);
    return true;
}

}  // namespace

/* ── Public entry points ─────────────────────────────────────────────── */

bool V2FactoryCalRequested()
{
    daisy::GPIO b1, b2;
    b1.Init(kOnMcuBtnPins[0], daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    b2.Init(kOnMcuBtnPins[1], daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    daisy::System::Delay(2);  /* pull-up settle */
    /* Active LOW. Require both, sampled twice 30 ms apart (debounce). */
    if (b1.Read() || b2.Read()) return false;
    daisy::System::Delay(30);
    return !b1.Read() && !b2.Read();
}

[[noreturn]] void V2RunFactoryCalibrationAndReset(AlchemyLabV2& hw)
{
    /* Panel first, so even bring-up failures get the red display. The
     * white B1+B2 pairs acknowledge the operator's button hold. */
    hw.strip.Init(kLedTotal);
    hw.leds.Init(hw.strip, kAlchemyLabV2Layout);
    hw.leds.SetButtonPair(kButtonB1, kLedWhite);
    hw.leds.SetButtonPair(kButtonB2, kLedWhite);
    LedsShow(hw);

    /* Bring up exactly the peripherals the procedure needs, on the
     * board's own members. Init() hasn't configured them yet (this runs
     * at the top of Init), and we soft-reset on the way out, so there is
     * no double-init hazard. */
    daisy::I2CHandle::Config i2c_cfg;
    i2c_cfg.periph         = daisy::I2CHandle::Config::Peripheral::I2C_1;
    i2c_cfg.speed          = daisy::I2CHandle::Config::Speed::I2C_400KHZ;
    i2c_cfg.mode           = daisy::I2CHandle::Config::Mode::I2C_MASTER;
    i2c_cfg.pin_config.scl = daisy::Pin(daisy::PORTB, 8);
    i2c_cfg.pin_config.sda = daisy::Pin(daisy::PORTB, 9);
    if (hw.i2c.Init(i2c_cfg) != daisy::I2CHandle::Result::OK) FailForever(hw);

    if (!hw.expander.Init(hw.i2c, kPca9557Address))           FailForever(hw);
    if (!hw.dac.Init(hw.i2c, kMcp4728AddrFirst, kMcp4728AddrLast))
        FailForever(hw);
    hw.dac.PulseLdac(hw.expander, kPca9557IoLdac);

    daisy::DacHandle::Config dac_cfg;
    dac_cfg.target_samplerate = 48000u;
    dac_cfg.chn               = daisy::DacHandle::Channel::BOTH;
    dac_cfg.mode              = daisy::DacHandle::Mode::POLLING;
    dac_cfg.bitdepth          = daisy::DacHandle::BitDepth::BITS_12;
    dac_cfg.buff_state        = daisy::DacHandle::BufferState::ENABLED;
    if (hw.stm_dac.Init(dac_cfg) != daisy::DacHandle::Result::OK)
        FailForever(hw);
    hw.stm_dac.WriteValue(daisy::DacHandle::Channel::ONE, 2048u);
    hw.stm_dac.WriteValue(daisy::DacHandle::Channel::TWO, 2048u);

    if (!CalAdcInit()) FailForever(hw);

    V2Calibration cal{};
    if (!RunProcedure(hw, cal)) FailForever(hw);

    /* Persist. Button pairs go white; the rings hold their latched
     * frame with zero data traffic while QSPI leaves memory-mapped
     * mode for the erase + write. */
    AllButtons(hw, kLedWhite);
    LedsShow(hw);
    daisy::System::Delay(5);
    if (!PersistToQspi(hw, cal)) FailForever(hw);

    /* Success: triple green flash, then solid green until the operator
     * releases both buttons (so the reboot doesn't re-enter cal). */
    hw.seed.SetLed(true);
    for (uint8_t i = 0; i < 3u; ++i)
    {
        hw.leds.Clear();
        AllRings(hw, kLedGreen);
        AllButtons(hw, kLedGreen);
        LedsShow(hw);
        daisy::System::Delay(180);
        hw.leds.Clear();
        LedsShow(hw);
        daisy::System::Delay(140);
    }
    hw.leds.Clear();
    AllRings(hw, kLedGreenHold);
    AllButtons(hw, kLedGreen);
    LedsShow(hw);

    daisy::GPIO b1, b2;
    b1.Init(kOnMcuBtnPins[0], daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    b2.Init(kOnMcuBtnPins[1], daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    while (!b1.Read() || !b2.Read())
        daisy::System::Delay(10);
    hw.seed.SetLed(false);
    hw.leds.Clear();
    LedsShow(hw);
    daisy::System::Delay(100);

    NVIC_SystemReset();
    while (true) {}  /* unreachable; satisfies [[noreturn]] on all paths */
}

} // namespace alchemy
