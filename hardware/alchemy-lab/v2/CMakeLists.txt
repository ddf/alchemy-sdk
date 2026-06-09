# ── alchemy::board_alchemy_lab_v2 ───────────────────────────────────────────
#
# V2 BSP is cross-compile only. The drivers depend on I²C, internal DAC,
# SDMMC and FatFS HAL types that aren't covered by the host stubs in
# stubs/.  When the SDK is built natively (CI), this subdirectory adds no
# targets — V1 continues to build for host smoke checks.

if(NOT CMAKE_CROSSCOMPILING)
    message(STATUS "alchemy-sdk: skipping V2 BSP (host build — V2 is cross-compile only)")
    return()
endif()

add_library(alchemy_board_alchemy_lab_v2_hal STATIC
    src/alchemy_lab_v2_layout.cpp
    src/ws2812.cpp
    src/pca9557.cpp
    src/mcp4728.cpp
)
add_library(alchemy::board_alchemy_lab_v2_hal ALIAS alchemy_board_alchemy_lab_v2_hal)

target_include_directories(alchemy_board_alchemy_lab_v2_hal PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
)

target_link_libraries(alchemy_board_alchemy_lab_v2_hal PUBLIC daisy)

add_library(alchemy_board_alchemy_lab_v2 STATIC
    src/alchemy_lab_v2.cpp
)
add_library(alchemy::board_alchemy_lab_v2 ALIAS alchemy_board_alchemy_lab_v2)

target_link_libraries(alchemy_board_alchemy_lab_v2 PUBLIC
    alchemy::board_alchemy_lab_v2_hal
    alchemy::primitives
)
