/**
 * @file flash_ops.h
 * @brief alchemy::FlashOps — platform flash operation callbacks.
 *
 * FlashOps is a pure hardware abstraction: three C function-pointer callbacks
 * for erase, write, and D-cache invalidation.
 *
 * Board BSP files return a FlashOps from a helper such as PresetFlashOps();
 * the framework's PresetStore receives one at Init().
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace alchemy {

struct FlashOps
{
    /** Erase flash in the range [start, end). */
    bool (*erase)(void* ctx, uint32_t start, uint32_t end);

    /** Write bytes from buf to addr. */
    bool (*write)(void* ctx, uint32_t addr, const uint8_t* buf, uint32_t len);

    /**
     * Invalidate D-cache for [addr, addr+len).
     * Must be called before reading from memory-mapped flash after an
     * erase or write.  May be a no-op on targets without data cache.
     */
    void (*invalidate)(void* ctx, uint32_t addr, uint32_t len);

    void* ctx;
};

} // namespace alchemy
