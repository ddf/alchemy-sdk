# ── alchemy::board_alchemy_lab_v1_hal ──────────────────────────────────────

add_library(alchemy_board_alchemy_lab_v1_hal STATIC
    src/alchemy_lab_v1_layout.cpp
    src/ws2812.cpp
)
add_library(alchemy::board_alchemy_lab_v1_hal ALIAS alchemy_board_alchemy_lab_v1_hal)

target_include_directories(alchemy_board_alchemy_lab_v1_hal PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    # Self-locating (../../include == hardware/include) so the SDK can be
    # add_subdirectory()'d from a consuming project.
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
)

# Resolve daisy_seed.h / stm32h7xx_hal.h: cross-compile uses the real
# libDaisy headers via the `daisy` target; host build uses the stubs/.
# This is the single point in the SDK where the two libDaisy sources are
# selected — every other target inherits transitively.
if(CMAKE_CROSSCOMPILING)
    target_link_libraries(alchemy_board_alchemy_lab_v1_hal PUBLIC daisy)
elseif(DEFINED ALCHEMY_STUB_DIR)
    target_include_directories(alchemy_board_alchemy_lab_v1_hal PUBLIC
        "${ALCHEMY_STUB_DIR}"
    )
endif()

add_library(alchemy_board_alchemy_lab_v1 STATIC
    src/alchemy_lab_v1.cpp
)
add_library(alchemy::board_alchemy_lab_v1 ALIAS alchemy_board_alchemy_lab_v1)

target_link_libraries(alchemy_board_alchemy_lab_v1 PUBLIC
    alchemy::board_alchemy_lab_v1_hal
    alchemy::primitives
)
