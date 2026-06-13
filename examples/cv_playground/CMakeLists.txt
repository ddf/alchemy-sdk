# cv_playground — exercise of the unified jack API on Alchemy Lab V2.
#
# Triggers on J1/J2 flash B1 accents; J9/J10 emit codec saws; B2 toggles
# J3..J8 between saw out (orange spinner) and CV in (cyan voltmeter).

if(NOT CMAKE_CROSSCOMPILING)
    return()
endif()
if(NOT ALCHEMY_BOARD STREQUAL "v2")
    return()
endif()

daisy_executable(
    NAME    cv_playground
    SOURCES cv_playground.cpp
    LIBS    alchemy::board_alchemy_lab_v2)
