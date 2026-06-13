/**
 * @file v2_cv_demo.cpp
 * @brief Calibrated CV output demo for Alchemy Lab V2.
 *
 * Demonstrates the V2 calibration surface end-to-end:
 *
 *   - `hw.IsCalibrated()`           — was a per-board cal record loaded?
 *   - `hw.cv_jacks[j].EnableCvOutput()` — close DG411, jack drives panel
 *   - `hw.cv_jacks[j].SetVolts(v)`  — calibrated voltage out (±10 mV)
 *   - `hw.cv_jacks[j].Volts()`      — calibrated voltage in
 *
 * Behavior: all six switchable jacks step through −4, −2, 0, +2, +4 V,
 * two seconds per step, forever. Scope any jack to verify. The Seed LED
 * shows cal status: solid = calibrated, slow blink = design fallback.
 *
 * To (re)calibrate the board: hold B1 + B2 while resetting. The on-board
 * factory cal runs (~15 s, LED pulses), persists to QSPI, and the board
 * reboots into this firmware with cal applied — no reflash needed.
 * Ensure no patch cables are plugged in.
 */

#include "alchemy/hw/alchemy_lab_v2.h"

using namespace alchemy;

static AlchemyLabV2 hw;

int main()
{
    hw.Init();

    /* Drive every switchable jack as a CV output (J3..J8). */
    for (uint8_t j = 0; j < kNumCvInputs; ++j)
        hw.cv_jacks[j].EnableCvOutput();

    constexpr float kSteps[]   = { -4.0f, -2.0f, 0.0f, 2.0f, 4.0f };
    constexpr size_t kNumSteps = sizeof(kSteps) / sizeof(kSteps[0]);

    size_t   step       = 0;
    uint32_t step_start = daisy::System::GetNow();
    uint32_t led_tick   = 0;

    for (uint8_t j = 0; j < kNumCvInputs; ++j)
        hw.cv_jacks[j].SetVolts(kSteps[step]);

    while (true)
    {
        const uint32_t now = daisy::System::GetNow();

        if (now - step_start >= 2000u)
        {
            step       = (step + 1u) % kNumSteps;
            step_start = now;
            for (uint8_t j = 0; j < kNumCvInputs; ++j)
                hw.cv_jacks[j].SetVolts(kSteps[step]);
        }

        /* LED: solid when calibrated, 1 Hz blink on design fallback. */
        hw.seed.SetLed(hw.IsCalibrated() ? true : ((led_tick / 100u) & 1u));

        daisy::System::Delay(5);
        ++led_tick;
    }
}
