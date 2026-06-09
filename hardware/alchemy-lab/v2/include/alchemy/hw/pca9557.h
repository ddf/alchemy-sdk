/**
 * @file pca9557.h
 * @brief alchemy::Pca9557 — minimal driver for the PCA9557PW 8-bit I²C
 *        GPIO expander on Alchemy Lab V2.
 *
 * The expander hosts three responsibilities on V2:
 *
 *   IO0..IO5  outputs — DG411 select lines (one per CV jack) gating the
 *                       DAC source onto the jack.
 *   IO6       input   — B3 button (see Pca9557Button).
 *   IO7       output  — LDAC for the MCP4728 quad DAC (active LOW).
 *
 * The driver keeps a shadow of the output and configuration registers in
 * RAM. Bit-wise mutators do read-modify-write against the shadow only,
 * then push a single byte to the chip — the chip itself is never read for
 * its current output state because the shadow is authoritative.
 */

#pragma once

#include "daisy_seed.h"
#include "per/i2c.h"

namespace alchemy {

class Pca9557
{
  public:
    /** Initialise on the given I²C handle at the given 7-bit address.
     *  Writes the boot configuration (IO6 input, all others output;
     *  outputs HIGH so DG411 switches are open and LDAC is inactive).
     *
     *  @return true if every config write ACKs, false otherwise.
     */
    bool Init(daisy::I2CHandle& i2c, uint8_t address_7bit);

    bool SetOutputBit(uint8_t bit, bool level);
    bool ToggleOutputBit(uint8_t bit);
    bool WriteOutputs(uint8_t value);
    bool ReadInputs(uint8_t& out);
    bool ReadInputBit(uint8_t bit, bool& level);
    uint8_t OutputShadow() const { return output_shadow_; }
    bool Ready() const { return ready_; }

  private:
    bool WriteReg(uint8_t reg, uint8_t value);
    bool ReadReg(uint8_t reg, uint8_t& value);

    daisy::I2CHandle* i2c_           = nullptr;
    uint8_t           address_       = 0u;
    uint8_t           output_shadow_ = 0u;
    bool              ready_         = false;
};

} // namespace alchemy
