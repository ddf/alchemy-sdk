# Alchemy SDK — STM32H750 (Daisy Seed2 DFM) cross toolchain.
#
# Selects the ARM GNU toolchain (arm-none-eabi-gcc) and applies the MCU /
# architecture flags required by the Daisy Seed2 DFM (Cortex-M7, FPv5-D16,
# hard-float, Thumb).  All the heavy lifting is delegated to libDaisy's
# upstream toolchain file at vendor/libDaisy/cmake/toolchains/stm32h750xx.cmake
# so the flags stay in lock-step with the libDaisy release that this SDK is
# pinned against.
#
# Usage:
#   cmake -B build-arm -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/stm32h750xx.cmake
#   cmake --build build-arm
#
# Compiler discovery:
#   1. If CMAKE_C_COMPILER / CMAKE_CXX_COMPILER are passed on the command line
#      or already in the cache, those are used verbatim.
#   2. Otherwise, arm-none-eabi-gcc / arm-none-eabi-g++ are looked up on PATH
#      (the standard Ubuntu / Homebrew / Arch package names).
#
# To use a non-default install location, point the variables at it:
#   -DCMAKE_C_COMPILER=/opt/gcc-arm-13/bin/arm-none-eabi-gcc
#   -DCMAKE_CXX_COMPILER=/opt/gcc-arm-13/bin/arm-none-eabi-g++

# ── Locate libDaisy (must be present before this file is included) ────────
# Resolve to an absolute path so the upstream toolchain file can use it
# regardless of how it was invoked.
get_filename_component(_alchemy_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.."
                       ABSOLUTE)
set(_alchemy_libdaisy_root "${_alchemy_repo_root}/vendor/libDaisy")

if(NOT EXISTS "${_alchemy_libdaisy_root}/cmake/toolchains/ArmGNUToolchain.cmake")
    message(FATAL_ERROR
        "alchemy-sdk: vendor/libDaisy is not initialised.  Run:\n"
        "    git submodule update --init --recursive\n"
        "from ${_alchemy_repo_root}")
endif()

# ── Auto-discover compilers on PATH if not specified ─────────────────────
if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED ENV{CMAKE_C_COMPILER})
    find_program(_alchemy_arm_gcc arm-none-eabi-gcc)
    find_program(_alchemy_arm_gxx arm-none-eabi-g++)
    if(_alchemy_arm_gcc AND _alchemy_arm_gxx)
        set(CMAKE_C_COMPILER   "${_alchemy_arm_gcc}" CACHE FILEPATH "C compiler"   FORCE)
        set(CMAKE_CXX_COMPILER "${_alchemy_arm_gxx}" CACHE FILEPATH "C++ compiler" FORCE)
        set(CMAKE_ASM_COMPILER "${_alchemy_arm_gcc}" CACHE FILEPATH "ASM compiler" FORCE)
    else()
        message(FATAL_ERROR
            "alchemy-sdk: arm-none-eabi-gcc / arm-none-eabi-g++ not found on PATH.\n"
            "Install the ARM GNU toolchain (e.g. `apt install gcc-arm-none-eabi`,\n"
            "`brew install arm-none-eabi-gcc`, or download from\n"
            "https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)\n"
            "or pass -DCMAKE_C_COMPILER=/path/to/arm-none-eabi-gcc.")
    endif()
    unset(_alchemy_arm_gcc CACHE)
    unset(_alchemy_arm_gxx CACHE)
endif()

# Delegate the real configuration to libDaisy — system name, MCU/FPU/Thumb
# flags, linker defaults, find_root_path setup, ABI/triple selection.
# ArmGNUToolchain.cmake is the GNU entry point; it pulls in stm32h750xx.cmake
# for the per-MCU architecture flags.  Both files live in libDaisy and stay
# in lock-step with the libDaisy release this SDK is pinned against.
list(INSERT CMAKE_MODULE_PATH 0 "${_alchemy_libdaisy_root}/cmake/toolchains")
include("${_alchemy_libdaisy_root}/cmake/toolchains/ArmGNUToolchain.cmake")
