/**
 * @file alchemy/hw/alchemy_lab_layout.h
 * @brief Unified layout-only shim for the active Alchemy Lab board.
 *
 * Selects between V1 and V2 layout headers based on the ALCHEMY_BOARD_V2
 * macro (set by `framework/CMakeLists.txt` when ALCHEMY_BOARD=v2).
 *
 * Also re-exports `kAlchemyLabArcGeometry` as a board-agnostic alias of
 * the active board's ArcGeometry instance, so framework code can refer to
 * it by a single name regardless of which board is compiled in.
 */

#pragma once

#if defined(ALCHEMY_BOARD_V2)
#include "alchemy/hw/alchemy_lab_v2_layout.h"
namespace alchemy {
inline constexpr ArcGeometry kAlchemyLabArcGeometry = kAlchemyLabV2ArcGeometry;
} // namespace alchemy
#else
#include "alchemy/hw/alchemy_lab_v1_layout.h"
namespace alchemy {
inline constexpr ArcGeometry kAlchemyLabArcGeometry = kAlchemyLabV1ArcGeometry;
} // namespace alchemy
#endif
