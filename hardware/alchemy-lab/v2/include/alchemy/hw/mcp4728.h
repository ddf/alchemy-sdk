/**
 * @file mcp4728.h
 * @brief alchemy::Mcp4728 — Microchip MCP4728 12-bit quad I²C DAC driver.
 *
 * Boot behaviour:
 *   - 7-bit address is set in EEPROM (factory: 0x60..0x67 by part suffix).
 *     `Init()` probes 0x60..0x67 sequentially and uses the first ACK.
 *   - All four channels are configured for VREF = VDD (3.3 V), Gain = 1,
 *     PD = normal. Initial value is mid-scale.
 *   - LDAC is controlled externally via PCA9557 IO7. This driver does
 *     not touch LDAC — see Mcp4728::WriteAll().
 *
 * Public API is intentionally tiny: write all four channels via one
 * Multi-Write command. The control loop calls `WriteAll()` at the saw-
 * update rate, then the bring-up firmware pulses LDAC to latch.
 */

#pragma once

#include <cstdint>
#include "daisy_seed.h"
#include "per/i2c.h"

namespace alchemy {

class Pca9557;  /* forward declaration — LDAC pulse helper */

class Mcp4728
{
  public:
    /** Probe 0x60..0x67 and configure all four channels.
     *  @return true on success, false if no chip ACKs in the range. */
    bool Init(daisy::I2CHandle& i2c, uint8_t first_addr, uint8_t last_addr);

    /** Write the 12-bit values for all four channels in one Multi-Write
     *  transaction. Bits 12..15 of each value are ignored.
     *
     *  Latch behaviour: after this call, the chip has the new values in
     *  its DAC input registers but the analog outputs do not change until
     *  LDAC pulses low. Use `PulseLdac()` to trigger the latch.
     */
    bool WriteAll(uint16_t ch_a, uint16_t ch_b, uint16_t ch_c, uint16_t ch_d);

    /** Drive LDAC LOW briefly via the given PCA9557 IO bit, then HIGH.
     *  Returns true if both I²C writes ACK. */
    bool PulseLdac(Pca9557& expander, uint8_t ldac_io);

    bool    Ready()  const { return ready_; }
    uint8_t Address() const { return address_; }

  private:
    daisy::I2CHandle* i2c_     = nullptr;
    uint8_t           address_ = 0u;
    bool              ready_   = false;
};

} // namespace alchemy
