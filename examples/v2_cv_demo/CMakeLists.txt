# v2_cv_demo — calibrated CV output demo (Alchemy Lab V2 only).
#
# Steps every CV jack through ±4 V using the per-board calibration; the
# Seed LED reports whether a real cal record is active. Doubles as the
# quickest bench check that calibration + routing work on a given board.

if(NOT CMAKE_CROSSCOMPILING)
    return()
endif()
if(NOT ALCHEMY_BOARD STREQUAL "v2")
    return()
endif()

daisy_executable(
    NAME    v2_cv_demo
    SOURCES v2_cv_demo.cpp
    LIBS    alchemy::board_alchemy_lab_v2)
