/**
 * @file pca9557.cpp
 * @brief alchemy::Pca9557 — PCA9557PW expander driver implementation.
 *
 * Wire format for every register access (per NXP PCA9557 datasheet):
 *   write: S | addr_W | reg | data    | P
 *   read : S | addr_W | reg | Sr | addr_R | data | P
 *
 * The chip auto-increments only when bit 7 of the command byte is set on
 * the PCA9555 / 9558; the PCA9557 has no auto-increment so each register
 * is accessed individually.
 */

#include "alchemy/hw/pca9557.h"
#include "alchemy/hw/alchemy_lab_v2_layout.h"

namespace alchemy {

namespace {
constexpr uint32_t kI2cTimeoutMs = 5u;
} // namespace

bool Pca9557::Init(daisy::I2CHandle& i2c, uint8_t address_7bit)
{
    i2c_     = &i2c;
    address_ = address_7bit;
    ready_   = false;

    /* Order matters */
    if (!WriteReg(kPca9557RegOutput, kPca9557OutputBoot))     return false;
    if (!WriteReg(kPca9557RegPolarity, kPca9557PolarityBoot)) return false;
    if (!WriteReg(kPca9557RegConfig, kPca9557ConfigBoot))     return false;

    output_shadow_ = kPca9557OutputBoot;
    ready_         = true;
    return true;
}

bool Pca9557::SetOutputBit(uint8_t bit, bool level)
{
    if (!ready_ || bit > 7u) return false;
    const uint8_t mask = static_cast<uint8_t>(1u << bit);
    const uint8_t next = level
        ? static_cast<uint8_t>(output_shadow_ | mask)
        : static_cast<uint8_t>(output_shadow_ & ~mask);
    if (next == output_shadow_) return true;  /* no-op */
    if (!WriteReg(kPca9557RegOutput, next))   return false;
    output_shadow_ = next;
    return true;
}

bool Pca9557::ToggleOutputBit(uint8_t bit)
{
    if (!ready_ || bit > 7u) return false;
    const uint8_t mask = static_cast<uint8_t>(1u << bit);
    const uint8_t next = static_cast<uint8_t>(output_shadow_ ^ mask);
    if (!WriteReg(kPca9557RegOutput, next)) return false;
    output_shadow_ = next;
    return true;
}

bool Pca9557::WriteOutputs(uint8_t value)
{
    if (!ready_) return false;
    if (!WriteReg(kPca9557RegOutput, value)) return false;
    output_shadow_ = value;
    return true;
}

bool Pca9557::ReadInputs(uint8_t& out)
{
    if (!ready_) return false;
    return ReadReg(kPca9557RegInput, out);
}

bool Pca9557::ReadInputBit(uint8_t bit, bool& level)
{
    if (bit > 7u) return false;
    uint8_t reg = 0u;
    if (!ReadInputs(reg)) return false;
    level = (reg & (1u << bit)) != 0u;
    return true;
}

bool Pca9557::WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return i2c_->TransmitBlocking(address_, buf, sizeof(buf), kI2cTimeoutMs)
           == daisy::I2CHandle::Result::OK;
}

bool Pca9557::ReadReg(uint8_t reg, uint8_t& value)
{
    return i2c_->ReadDataAtAddress(address_, reg, 1, &value, 1, kI2cTimeoutMs)
           == daisy::I2CHandle::Result::OK;
}

} // namespace alchemy
