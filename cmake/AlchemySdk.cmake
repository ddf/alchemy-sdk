# Alchemy SDK — daisy_executable() helper.
#
# Centralises everything needed to turn one or more .cpp files into a
# flashable Daisy Seed2 DFM firmware image.  Every example in the SDK uses
# this helper; new examples should too.
#
# Why one helper:
#   The Daisy bootloader is the only supported deployment target for the
#   Alchemy ecosystem — customer modules ship with it pre-installed in
#   internal flash and expect every firmware image to be programmed into
#   QSPI flash at 0x90040000.  Centralising the linker script, post-build
#   steps, and dfu-util incantation here keeps every example aligned and
#   leaves no opportunity for an example to silently link against a
#   different memory layout.
#
# What it produces:
#   <name>.elf  — fully-linked firmware (kept for debugger / size analysis)
#   <name>.bin  — raw binary, the format dfu-util uploads
#   <name>.hex  — Intel HEX (handy for JTAG/SWD tools)
#
# What auxiliary targets it adds:
#   <name>-size   prints arm-none-eabi-size memory summary for <name>.elf
#   <name>-flash  uploads <name>.bin to QSPI via dfu-util (see FLASHING in
#                 README.md for the boot-button procedure required first)
#
# This file is included only when cross-compiling.  In host builds it is
# not loaded and daisy_executable() is undefined — host targets continue
# to use add_executable() against the libDaisy stubs in stubs/.

if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "AlchemySdk.cmake must only be included from cross-compile builds. "
        "This is a guard against accidentally loading it in host CI.")
endif()

# ── libDaisy resources we rely on ─────────────────────────────────────────
# Resolved once so every daisy_executable() call uses the same paths.

# Self-locating (this file lives in <sdk>/cmake) so the SDK can be
# add_subdirectory()'d from a consuming project where CMAKE_SOURCE_DIR is
# the consumer.  Matches how cmake/toolchains/stm32h750xx.cmake locates
# libDaisy.
get_filename_component(_ALCHEMY_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_ALCHEMY_LIBDAISY_DIR "${_ALCHEMY_SDK_ROOT}/vendor/libDaisy"
    CACHE INTERNAL "Root of the vendored libDaisy submodule")

# Stock Daisy bootloader v6.x (the one preinstalled on every Alchemy Lab
# V1 module) operates in BOOT_SRAM mode: the firmware is flashed to QSPI
# at 0x90040000, copied to SRAM at 0x24000000 on boot, and executed from
# SRAM.  We MUST link against the SRAM script — the QSPI-XIP variant
# produces an image the stock bootloader can't load.
#
# The script is vendored into the SDK (cmake/linkers/) rather than borrowed
# from libDaisy/core/.  Memory-layout decisions are project-specific and
# evolve independently of libDaisy releases — vendoring keeps libDaisy
# upgrade-clean.  Additive section-table extensions (e.g. .ahb_sram_bss)
# ship as supplementary fragments under link/ and are passed alongside,
# so they're available to Make-based consumers without duplication.
#
# TODO: build a custom bootloader that supports QSPI execute-in-
# place, vendor in a _qspi variant alongside this one and switch the
# selection here; remove the `presets.Init()`-must-come-after-`hw.Init()`
# rule (the QSPI XIP mapping stays live in that mode).
set(_ALCHEMY_QSPI_LINKER_SCRIPT
    "${_ALCHEMY_SDK_ROOT}/cmake/linkers/alchemy_stm32h750ib_sram.lds"
    CACHE INTERNAL
    "Linker script for BOOT_SRAM mode (QSPI-flashed, copied-to-SRAM-on-boot).")

if(NOT EXISTS "${_ALCHEMY_QSPI_LINKER_SCRIPT}")
    message(FATAL_ERROR
        "alchemy-sdk: SRAM linker script not found at "
        "${_ALCHEMY_QSPI_LINKER_SCRIPT}.  The vendored script should ship "
        "with the SDK under cmake/linkers/ — check the repo state.")
endif()

# Supplemental linker fragments shipped under link/.  These define output
# sections that back SDK section-placement macros (AHB_SRAM_BSS, etc.) and
# use `INSERT BEFORE` so they compose with the primary linker without
# duplication.  Same files are consumed by Make-based projects via
# `LDFLAGS += -T $(ALCHEMY_SDK_DIR)/link/<fragment>.ld` — single source of
# truth across build systems.
set(_ALCHEMY_LINKER_FRAGMENTS
    "${_ALCHEMY_SDK_ROOT}/link/ahb_sram_bss.ld"
    CACHE INTERNAL
    "Supplementary -T fragments appended to every daisy_executable() link.")

foreach(_fragment IN LISTS _ALCHEMY_LINKER_FRAGMENTS)
    if(NOT EXISTS "${_fragment}")
        message(FATAL_ERROR
            "alchemy-sdk: linker fragment not found at ${_fragment}.  "
            "The SDK ships fragments under link/ — check the repo state.")
    endif()
endforeach()

# ── Discover dfu-util (optional; only required for the *-flash targets) ───
find_program(_ALCHEMY_DFU_UTIL dfu-util)

# ── daisy_executable(NAME <name> SOURCES <src1> [<src2> ...] ──────────────
#                    [LIBS <lib1> [<lib2> ...]])
#
# Defines a Daisy QSPI-bootloader firmware target.
#
#   NAME     — target name (and resulting .elf / .bin / .hex stem).
#   SOURCES  — one or more .cpp/.c files containing main() and any
#              translation units unique to this firmware.
#   LIBS     — Alchemy / third-party libraries to link.  The daisy
#              library and the QSPI linker setup are added automatically;
#              callers list only their SDK targets
#              (e.g. alchemy::controller).
#
function(daisy_executable)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES;LIBS" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "daisy_executable: NAME is required")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "daisy_executable(${ARG_NAME}): SOURCES is required")
    endif()

    # ── The firmware ELF ──────────────────────────────────────────────────
    add_executable(${ARG_NAME} ${ARG_SOURCES})

    set_target_properties(${ARG_NAME} PROPERTIES
        SUFFIX ".elf"
        # Primary linker script and every supplementary fragment are
        # build dependencies — touching any of them must re-link,
        # otherwise edits to memory regions / section placement go
        # unnoticed.
        LINK_DEPENDS "${_ALCHEMY_QSPI_LINKER_SCRIPT};${_ALCHEMY_LINKER_FRAGMENTS}"
    )

    # daisy is the libDaisy CMake target; LIBS go on top.
    target_link_libraries(${ARG_NAME} PRIVATE daisy ${ARG_LIBS})

    # ── Daisy-bootloader build mode (BOOT_SRAM) ───────────────────────────
    # BOOT_APP is libDaisy's flag for "this firmware is loaded by the
    # bootloader rather than running from internal flash" — it gates the
    # SCB->VTOR setup and other early-boot behaviour inside libDaisy.
    # libDaisy's own Makefile sets the same -DBOOT_APP for both BOOT_SRAM
    # and BOOT_QSPI; the linker script (above) is what picks between them.
    target_compile_definitions(${ARG_NAME} PRIVATE BOOT_APP)

    # ── Linker flags ──────────────────────────────────────────────────────
    # Primary linker first, then each supplementary fragment — order
    # matches Make's `LDFLAGS += -T <primary> -T <fragment>` and lets
    # `INSERT BEFORE` directives in fragments find their anchor sections
    # in the primary.
    set(_linker_flags
        LINKER:-T,${_ALCHEMY_QSPI_LINKER_SCRIPT}
    )
    foreach(_fragment IN LISTS _ALCHEMY_LINKER_FRAGMENTS)
        list(APPEND _linker_flags LINKER:-T,${_fragment})
    endforeach()
    target_link_options(${ARG_NAME} PRIVATE
        ${_linker_flags}
        LINKER:--gc-sections
        LINKER:--check-sections
        LINKER:--unresolved-symbols=report-all
        LINKER:--warn-common
        LINKER:--print-memory-usage
        LINKER:-Map=${ARG_NAME}.map
        LINKER:--cref
    )

    # ── Post-build: .bin and .hex next to the .elf ────────────────────────
    # The .bin is what dfu-util uploads; the .hex is convenient for SWD
    # debuggers (Cube Programmer, OpenOCD).  Both are always produced so
    # downstream tooling has a single, predictable filename to look for.
    add_custom_command(TARGET ${ARG_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary -S
                $<TARGET_FILE:${ARG_NAME}>
                $<TARGET_FILE_DIR:${ARG_NAME}>/${ARG_NAME}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   -S
                $<TARGET_FILE:${ARG_NAME}>
                $<TARGET_FILE_DIR:${ARG_NAME}>/${ARG_NAME}.hex
        BYPRODUCTS
                ${ARG_NAME}.bin
                ${ARG_NAME}.hex
        COMMENT "Generating ${ARG_NAME}.bin and ${ARG_NAME}.hex"
        VERBATIM)

    # ── <name>-size — quick memory summary ───────────────────────────────
    add_custom_target(${ARG_NAME}-size
        COMMAND ${CMAKE_SIZE} -A -d $<TARGET_FILE:${ARG_NAME}>
        COMMAND ${CMAKE_SIZE} -B    $<TARGET_FILE:${ARG_NAME}>
        DEPENDS ${ARG_NAME}
        COMMENT "Memory footprint for ${ARG_NAME}"
        VERBATIM)

    # ── <name>-flash — dfu-util upload to QSPI 0x90040000 ────────────────
    # The Daisy bootloader exposes the QSPI region as an STM32 DFU
    # interface (VID:PID 0483:df11) when the user holds the BOOT button
    # and pulses RESET (see README "Flash an example").
    if(_ALCHEMY_DFU_UTIL)
        add_custom_target(${ARG_NAME}-flash
            COMMAND ${_ALCHEMY_DFU_UTIL}
                    -a 0
                    -s 0x90040000:leave
                    -D $<TARGET_FILE_DIR:${ARG_NAME}>/${ARG_NAME}.bin
                    -d ,0483:df11
            DEPENDS ${ARG_NAME}
            USES_TERMINAL
            COMMENT "Flashing ${ARG_NAME}.bin to QSPI via Daisy bootloader"
            VERBATIM)
    else()
        # Stand-in so `cmake --build --target <name>-flash` produces a
        # readable error instead of "no rule to make target".
        add_custom_target(${ARG_NAME}-flash
            COMMAND ${CMAKE_COMMAND} -E echo
                    "ERROR: dfu-util is not installed.  Install it"
                    "(e.g. `apt install dfu-util` / `brew install dfu-util`)"
                    "and re-configure."
            COMMAND ${CMAKE_COMMAND} -E false
            COMMENT "dfu-util not found at configure time"
            VERBATIM)
    endif()
endfunction()
