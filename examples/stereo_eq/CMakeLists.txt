# stereo_eq — full opt-in surface stack: ControlLoop + VirtualKnob +
# Pager + ParamLock + Presets.  DSP and palettes are split into separate
# translation units so stereo_eq.cpp stays a pure SDK example.
#
# Two build modes are supported (see the top-level CMakeLists.txt):
#   • Cross-compile → daisy_executable() produces flashable .elf/.bin/.hex
#     for the Daisy QSPI bootloader.
#   • Host check    → add_executable() builds against the libDaisy stubs.

if(CMAKE_CROSSCOMPILING)

    daisy_executable(
        NAME    stereo_eq
        SOURCES stereo_eq.cpp stereo_eq_dsp.cpp
        LIBS    alchemy::surface)

else()

    add_executable(stereo_eq stereo_eq.cpp stereo_eq_dsp.cpp)
    target_link_libraries(stereo_eq PRIVATE alchemy::surface)

endif()
