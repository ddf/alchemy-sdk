/**
 * @file v2_calibration.h
 * @brief Alchemy Lab V2 — per-board DAC/ADC calibration (QSPI-persisted).
 *
 * Every V2 board carries a ~120-byte calibration record in a dedicated
 * QSPI sector. It is produced by the factory-cal procedure (see
 * v2_factory_cal.h, or the standalone `cal_writer` bench firmware) and
 * consumed by the SDK at AlchemyLabV2::Init():
 *
 *   - `CvJack::SetVolts()` inverts the per-jack DAC linear fit so a
 *     requested jack voltage lands within ~10 mV on a calibrated board.
 *   - `CvJack::Volts()` (analog jacks) applies the per-jack ADC zero +
 *     measured VDDA.
 *
 * QSPI read access is memory-mapped — loading is a memcpy + CRC check,
 * no bootloader involvement. Writes (factory cal only) temporarily
 * switch QSPI to indirect mode, which is safe because Alchemy firmware
 * executes from SRAM (BOOT_SRAM), never XIP.
 *
 * Design-fallback behavior: when the record is missing or corrupt the
 * SDK silently substitutes design-nominal constants so user firmware
 * never breaks; `AlchemyLabV2::IsCalibrated()` reports the distinction.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace alchemy {

/* ── Storage constants ───────────────────────────────────────────────── */

/** 'AC2C' in memory order (LSB-first read of bytes "AC2C"). */
constexpr uint32_t kV2CalMagic         = 0x43324341u;
constexpr uint16_t kV2CalSchemaVersion = 1u;

/** Memory-mapped QSPI address of the cal sector — the 4 KB sector
 *  immediately below the preset region (kPresetFlashBase = 0x90760000). */
constexpr uint32_t kV2CalQspiAddr       = 0x9075F000u;
constexpr uint32_t kV2CalQspiSectorSize = 4096u;

constexpr uint8_t kV2CalNumJacks = 6u;  /* J3..J8, same order as kCvPins */

/** vdda_source values. */
constexpr uint8_t kV2VddaMeasured = 0u;  /* via VREFINT */
constexpr uint8_t kV2VddaAssumed  = 1u;  /* VREFINT out of range; 3.30 V used */

/* ── Design-nominal analog constants (V2 panel schematic) ────────────── */
/*
 * Input (jack → ADC pin), inverting summer:  V_pin  = 1.65 − 0.165 × V_jack
 * Output (DAC pin → jack), inverting summer: V_jack = 5 − 3.03 × V_dac_pin
 */
constexpr float kV2CvInGainDesign     = 0.165f;  /* |dV_pin / dV_jack|       */
constexpr float kV2CvInBiasDesignV    = 1.65f;   /* V_pin at V_jack = 0      */
constexpr float kV2CvOutGainDesign    = -3.03f;  /* dV_jack / dV_dac_pin     */
constexpr float kV2CvOutOffsetDesignV = 5.0f;    /* V_jack at V_dac_pin = 0  */
constexpr float kV2VddaDesign         = 3.30f;

/* ── Per-jack record ─────────────────────────────────────────────────── */

struct V2JackCal
{
    float    dac_offset_v;        /* V_jack at DAC code 0 (fit intercept)    */
    float    dac_gain_v_per_code; /* fit slope (negative — inverting stage)  */
    uint16_t dac_max_linear_code; /* last code before output saturation      */
    uint16_t dac_min_linear_code; /* first code clear of saturation          */
    uint16_t adc_zero_code;       /* raw 16-bit ADC at V_jack = 0 (idle)     */
    uint16_t reserved;            /* pad to 16-byte stride                   */
};
static_assert(sizeof(V2JackCal) == 16u, "V2JackCal must stay 16 bytes");

/* ── Top-level record (persisted verbatim) ───────────────────────────── */

struct V2Calibration
{
    uint32_t  magic;              /* kV2CalMagic                            */
    uint16_t  schema_version;     /* kV2CalSchemaVersion                    */
    uint16_t  reserved;
    uint32_t  boot_count_at_cal;  /* diagnostic                             */
    float     vdda_at_cal;        /* measured via VREFINT (or 3.30 assumed) */
    uint8_t   vdda_source;        /* kV2VddaMeasured | kV2VddaAssumed       */
    uint8_t   reserved2[3];
    V2JackCal jack[kV2CalNumJacks];
    uint32_t  crc32;              /* covers every byte above this field     */
};
static_assert(sizeof(V2Calibration) == 120u,
              "V2Calibration must stay 120 bytes (20 hdr + 6*16 + 4 crc)");
static_assert(offsetof(V2Calibration, jack)  == 20u,  "layout drift");
static_assert(offsetof(V2Calibration, crc32) == 116u, "layout drift");

constexpr size_t kV2CalCrcCoveredBytes = offsetof(V2Calibration, crc32);

/* ── API ─────────────────────────────────────────────────────────────── */

/** CRC32 (reflected 0xEDB88320, same as Ethernet/ZIP) over `len` bytes. */
uint32_t V2CalCrc32(const void* data, size_t len);

/** CRC over a record's covered region (everything before .crc32). */
inline uint32_t V2CalComputeCrc(const V2Calibration& c)
{
    return V2CalCrc32(&c, kV2CalCrcCoveredBytes);
}

/** Read the record from memory-mapped QSPI into `out` and validate
 *  magic + schema + CRC. D-cache is invalidated for the region first.
 *  @return true if `out` holds a valid record. On false, `out` contents
 *          are unspecified — call V2CalDesignFallback() before use. */
bool V2CalLoadFromQspi(V2Calibration& out);

/** Fill `out` with design-nominal values (uncalibrated behavior):
 *  design slope/offset on every jack, full code range, mid-scale ADC
 *  zero, VDDA = 3.30 assumed. magic/crc are stamped so the struct is
 *  self-consistent, but it represents "no calibration". */
void V2CalDesignFallback(V2Calibration& out);

} // namespace alchemy
