/* Output section for the AHB_SRAM_BSS macro
 * (alchemy-sdk/hardware/include/alchemy/mem_sections.h).
 *
 * Places `.ahb_sram_bss` input sections in RAM_D2 (256 KB D2 AHB SRAM at
 * 0x30008000), reachable by DMA1 with single-cycle, jitter-free latency.
 * Used by buffers larger than the 32 KB RAM_D2_DMA region and that need
 * to stay off the FMC bus (e.g. the V2 WS2812 cell buffer).
 *
 * INSERT BEFORE .heap so newlib's _sbrk (which grows heap from the `end`
 * symbol) cannot allocate over these buffers. The heap reserves whatever
 * is left in RAM_D2 after this section.
 *
 * Usage:
 *   Make:  LDFLAGS += -T $(ALCHEMY_SDK_DIR)/link/ahb_sram_bss.ld
 *   CMake: handled by daisy_executable() in cmake/AlchemySdk.cmake.
 *
 * Requirements on the primary linker script:
 *   - MEMORY { RAM_D2 (RWX) : ORIGIN = 0x30008000, LENGTH = 256K }
 *   - A `.heap` output section (true for libDaisy stock SRAM/QSPI/FLASH
 *     scripts and the SDK's vendored SRAM script).
 * 
 * This was done to prevent LED glitching on the non-timer driven pin
 *    selected for the V2 revision of the Alchemy Lab. Still, there 
 *    are occasional glitches.
 *    TODO - remove this entire implemenetation OR make it more robust.
 *    May be a bit above my pay grade.
 */

SECTIONS
{
    .ahb_sram_bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sahb_sram_bss = .;

        PROVIDE(__ahb_sram_bss_start__ = _sahb_sram_bss);
        *(.ahb_sram_bss)
        *(.ahb_sram_bss*)
        . = ALIGN(4);
        _eahb_sram_bss = .;

        PROVIDE(__ahb_sram_bss_end__ = _eahb_sram_bss);
    } > RAM_D2
} INSERT BEFORE .heap;

/* RAM_D2 overflow protection: would normally express this as a top-level
 * ASSERT against ORIGIN(RAM_D2) + LENGTH(RAM_D2), but supplementary
 * scripts (used via `-T` alongside a primary) can't reliably name the
 * primary's MEMORY regions during parse. The .heap output section
 * immediately follows in the primary's RAM_D2 region; an overflow here
 * displaces the heap and surfaces as a region-overflow link error from
 * the primary script — same diagnostic, one step removed. */
