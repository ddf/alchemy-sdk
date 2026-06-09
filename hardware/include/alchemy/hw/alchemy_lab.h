/**
 * @file alchemy/hw/alchemy_lab.h
 * @brief Unified board header — selects V1 or V2 Alchemy Lab board class.
 *
 * Firmware writes against `alchemy::AlchemyLab`; this header resolves it
 * to either `AlchemyLabV1` or `AlchemyLabV2` based on the ALCHEMY_BOARD_V2
 * macro (set by `framework/CMakeLists.txt` when ALCHEMY_BOARD=v2). User
 * firmware never has to care which.
 *
 * Frozen shared contract — every Alchemy Lab board class exposes
 * identically:
 *   - seed, pots[kNumPots], cv[kNumCvInputs]
 *   - buttons[kNumButtons]  (returns IButton&)
 *   - strip, leds
 *   - Init(sample_rate, block_size), ProcessAllControls(), StartAudio(cb)
 *   - Layout(), Arc(), SampleRate(), BlockSize()
 *
 * Anything outside this contract (e.g. V2's `hw.dac`, `hw.expander`,
 * `hw.sdmmc`) is board-unique. A firmware that touches a unique member
 * should pin its target board explicitly:
 *
 *   static_assert(std::is_same_v<alchemy::AlchemyLab,
 *                                alchemy::AlchemyLabV2>,
 *                 "this firmware requires Alchemy Lab V2 (-DALCHEMY_BOARD=v2)");
 *
 * This header pulls in the full board class (libDaisy peripherals
 * included). Framework headers that need only compile-time layout
 * constants should include `alchemy_lab_layout.h` instead.
 * 
 * It should be noted that completing this work has no real benefit
 * other than being an exercise in setting up for the future.
 * There are literally 10 V1 boards in existence. I mostly wanted to
 * prove out this idea of a hardware abstracted interface where an
 * SDK user could write code against any underlying hardware implementation.
 * IT WORKS!
 */

#pragma once

#if defined(ALCHEMY_BOARD_V2)
#include "alchemy/hw/alchemy_lab_v2.h"
namespace alchemy {
using AlchemyLab = AlchemyLabV2;
} // namespace alchemy
#else
#include "alchemy/hw/alchemy_lab_v1.h"
namespace alchemy {
using AlchemyLab = AlchemyLabV1;
} // namespace alchemy
#endif
