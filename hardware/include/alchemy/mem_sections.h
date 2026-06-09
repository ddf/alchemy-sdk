#pragma once
/**
 * @file mem_sections.h
 * @brief Alchemy SDK memory-placement attribute macros.
 *
 * Wraps the linker-script output sections owned by the Alchemy SDK
 * (cmake/linkers/alchemy_stm32h750ib_sram.lds). Drivers use these to
 * place buffers in specific physical memories without hard-coding
 * `__attribute__((section(...)))` strings at every call site.
 *
 * libDaisy provides DMA_BUFFER_MEM_SECTION (.sram1_bss / RAM_D2_DMA, 32 KB)
 * and DSY_SDRAM_BSS (.sdram_bss / 64 MB external SDRAM). Those remain the
 * right answer for small DMA buffers and bulk audio data respectively.
 * The macros below cover the gaps the SDK has hit:
 *
 *  - AHB_SRAM_BSS — 256 KB region in the D2 power domain (RAM_D2 at
 *    0x30008000). DMA1/DMA2 reach it directly through the D2 AHB matrix
 *    with single-cycle, jitter-free latency. NOT in the D-cache path —
 *    no cache management required. Use this when DMA timing is hard
 *    real-time and the buffer is too big for .sram1_bss (e.g. the V2
 *    WS2812 cell buffer, ~32 KB, with a 417 ns per-cell DMA deadline).
 *
 * TODO - verify this implementation decision.
 */

#define AHB_SRAM_BSS __attribute__((section(".ahb_sram_bss")))
