/**
 * @file mcp4728.cpp
 * @brief alchemy::Mcp4728 — implementation.
 *
 * Multi-Write command wire format (MCP4728 datasheet §5.6.2):
 *
 *   S | addr_W
 *     | 0b01000000 | 0b00000000      (Multi-Write, ch=0, UDAC=0 → load on LDAC)
 *     | VREF_PD_GAIN_HI | DATA_LO    (ch A)
 *     | 0b01000010 | 0b00000000      (Multi-Write, ch=1, UDAC=0)
 *     | VREF_PD_GAIN_HI | DATA_LO    (ch B)
 *     | 0b01000100 | ...             (ch C)
 *     | 0b01000110 | ...             (ch D)
 *     | P
 *
 * Each channel's first config byte:
 *   bit 7   VREF  (1 = internal 2.048 V, 0 = VDD)         → 0
 *   bit 6.5 PD    (00 = normal)                            → 00
 *   bit 4   Gain  (1 = ×2, 0 = ×1; only relevant when VREF=1) → 0
 *   bits 3..0   high nibble of 12-bit DAC value
 *
 * UDAC = 0 means the channel's analog output updates when LDAC is asserted.
 * We leave LDAC under PCA9557 control so the bring-up firmware can latch
 * all four channels at once.
 */

#include "alchemy/hw/mcp4728.h"
#include "alchemy/hw/pca9557.h"
#include "daisy_seed.h"  /* daisy::System::Delay */

namespace alchemy {

namespace {

constexpr uint32_t kI2cTimeoutMs = 5u;

/* Multi-Write command bits for channel N:
 *  010 0 DDD U   where DDD = channel index (0..3), U = UDAC (0 = update on LDAC).
 * High nibble of byte: 0b0100_0 → 0x40
 * Low  nibble of byte: 000 0 (ch=0, U=0) — channel index goes in bits 2..1.
 */
constexpr uint8_t kMultiWriteCmd(uint8_t channel)
{
    return static_cast<uint8_t>(0x40u | (static_cast<uint8_t>(channel & 0x03u) << 1));
}

constexpr uint8_t kVrefPdGainBoot = 0x00u;   /* VREF=VDD, PD=normal, Gain=×1 */

bool I2cWrite(daisy::I2CHandle& i2c, uint8_t addr, uint8_t* buf, uint16_t len)
{
    return i2c.TransmitBlocking(addr, buf, len, kI2cTimeoutMs)
           == daisy::I2CHandle::Result::OK;
}

/** Build the 12-byte Multi-Write payload for a given uniform 12-bit value
 *  applied to all four channels. */
void BuildMultiWriteUniform(uint16_t value12, uint8_t* buf)
{
    const uint16_t v = static_cast<uint16_t>(value12 & 0x0FFFu);
    const uint8_t  hi = static_cast<uint8_t>(kVrefPdGainBoot
                        | static_cast<uint8_t>((v >> 8) & 0x0Fu));
    const uint8_t  lo = static_cast<uint8_t>(v & 0xFFu);
    for (uint8_t ch = 0; ch < 4u; ++ch)
    {
        buf[ch * 3 + 0] = kMultiWriteCmd(ch);
        buf[ch * 3 + 1] = hi;
        buf[ch * 3 + 2] = lo;
    }
}

} // namespace

bool Mcp4728::Init(daisy::I2CHandle& i2c, uint8_t first_addr, uint8_t last_addr)
{
    i2c_   = &i2c;
    ready_ = false;

    /* Probe by attempting the actual mid-scale seed write at each
     * candidate address. First ACK wins. This is more reliable than a
     * zero-byte probe (some STM32 HAL versions reject Size=0 at the API
     * boundary) and leaves the chip in a known state on success. 
     * TODO: Update with new calibration procedure once implemented. */
    uint8_t buf[12];
    BuildMultiWriteUniform(0x800u, buf);

    for (uint8_t a = first_addr; a <= last_addr; ++a)
    {
        if (I2cWrite(i2c, a, buf, sizeof(buf)))
        {
            address_ = a;
            ready_   = true;
            return true;
        }
    }
    return false;
}

bool Mcp4728::WriteAll(uint16_t ch_a, uint16_t ch_b, uint16_t ch_c, uint16_t ch_d)
{
    if (!ready_ || i2c_ == nullptr) return false;

    const uint16_t v[4] = {
        static_cast<uint16_t>(ch_a & 0x0FFFu),
        static_cast<uint16_t>(ch_b & 0x0FFFu),
        static_cast<uint16_t>(ch_c & 0x0FFFu),
        static_cast<uint16_t>(ch_d & 0x0FFFu),
    };

    uint8_t buf[12];
    for (uint8_t ch = 0; ch < 4u; ++ch)
    {
        const uint8_t  hi = static_cast<uint8_t>(kVrefPdGainBoot
                            | static_cast<uint8_t>((v[ch] >> 8) & 0x0Fu));
        const uint8_t  lo = static_cast<uint8_t>(v[ch] & 0xFFu);
        buf[ch * 3 + 0]   = kMultiWriteCmd(ch);
        buf[ch * 3 + 1]   = hi;
        buf[ch * 3 + 2]   = lo;
    }

    return I2cWrite(*i2c_, address_, buf, sizeof(buf));
}

bool Mcp4728::PulseLdac(Pca9557& expander, uint8_t ldac_io)
{
    if (!ready_) return false;
    /* LDAC is active LOW. Pull low → minimal hold → release high.
     * MCP4728 datasheet requires LDAC LOW time ≥ 100 ns; an I²C round
     * trip is ~75 µs at 400 kHz so the spacing between the two writes
     * easily meets that. */
    if (!expander.SetOutputBit(ldac_io, false)) return false;
    return expander.SetOutputBit(ldac_io, true);
}

} // namespace alchemy
