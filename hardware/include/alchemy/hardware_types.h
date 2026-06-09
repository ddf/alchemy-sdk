/**
 * @file hardware_types.h
 * @brief Abstract hardware layout types for the alchemy framework.
 *
 * These types are the contract between BSP files (hardware/vN/) and the
 * framework.  BSP files populate a HardwareLayout instance; framework code
 * reads it.  No libDaisy or platform headers are included here — this file
 * has no dependencies and can be included anywhere.
 *
 * Each hardware revision provides:
 *   extern const alchemy::HardwareLayout kAlchemy<Board>Layout;
 *
 * The framework receives a const reference to that instance at init and
 * never hard-codes peripheral counts or LED chain positions.
 */

#pragma once

#include <cstdint>

namespace alchemy {

/**
 * Physical description of one pot's LED ring in the WS2812 chain.
 *
 * start_hour / step_hours use a clock-face coordinate system:
 *   0.0 = noon, 3.0 = 3 o'clock, 6.0 = 6 o'clock, 9.0 = 9 o'clock.
 * step_hours is positive for clockwise winding, negative for CCW.
 *
 * The ideal vision is that a pot ring can be described for any 
 * arrangment of LEDs in a ws-style chain for future hardware.
 */
struct LedRingLayout {
    uint16_t chain_start;  ///< Chain index of ring offset 0.
    uint8_t  led_count;    ///< Number of LEDs in this ring.
    float    start_hour;   ///< Clock-face hour of ring offset 0.
    float    step_hours;   ///< Hours advanced per ring offset step.
};

/**
 * Chain indices of the two accent LEDs belonging to one button.
 */
struct LedButtonLayout {
    uint16_t bottom_chain;  ///< Chain index of the LED below the button.
    uint16_t top_chain;     ///< Chain index of the LED above the button.
};


/// Complete hardware description for one board revision.

struct HardwareLayout {
    /* Input counts */
    uint8_t num_pots;
    uint8_t num_cv;
    uint8_t num_btns;

    /* LED chain */
    uint16_t               led_total;
    const LedRingLayout*   rings;
    const LedButtonLayout* buttons;
};

struct ArcGeometry
{
    float   start_hour;   ///< Clock-hour of the CCW arc endpoint.
    float   step_hours;   ///< Hours advanced per arc step.
    uint8_t arc_leds;     ///< Number of usable arc steps.
};

} // namespace alchemy
