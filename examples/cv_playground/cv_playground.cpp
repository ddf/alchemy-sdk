/**
 * @file cv_playground.cpp
 * @brief Alchemy Lab V2 — exercise of the unified jack API across all
 *        three jack kinds in one program.
 *
 * What runs:
 *
 *   - J9, J10 (codec out, 24-bit DC):
 *       Two saw waves at independent frequencies, always running. Patch
 *       to a scope or VCO to verify the codec-CV path.
 *
 *   - J1, J2 (codec in, AC-coupled):
 *       Rising-edge trigger detection. Each trigger flashes one of the
 *       B1 accent LEDs (J1 → bottom, J2 → top) for ~80 ms.
 *
 *   - J3..J8 (switchable analog):
 *       Press B2 to toggle direction.
 *         Output mode  — six saws at independent frequencies; each ring
 *                        shows the saw position as a single orange pixel
 *                        spinning clockwise.
 *         Input  mode  — each ring shows the calibrated jack voltage as
 *                        a cyan pixel at the corresponding clock-face
 *                        hour (−5 V = 6 o'clock, 0 V = noon, +5 V = noon
 *                        again — wraps once).
 */

#include "alchemy/hw/alchemy_lab_v2.h"
#include "alchemy/led/panel.h"

using namespace alchemy;

static AlchemyLabV2 hw;

/* ── Saw configuration ──────────────────────────────────────────────── */

static constexpr float kSawFreqHzAnalog[kNumCvInputs] = {
    0.40f, 0.55f, 0.80f, 1.10f, 1.50f, 2.00f
};
static constexpr float kSawFreqHzCodec[kNumCodecCvOuts] = {
    0.70f, 1.30f
};

static volatile float s_saw_phase_analog[kNumCvInputs] = {};
static volatile float s_saw_phase_codec [kNumCodecCvOuts] = {};

/* ── Mode + flash state ─────────────────────────────────────────────── */

static bool s_output_mode = false;

/* Tick-based flash counters, decremented at 1 kHz in the main loop. */
static constexpr uint8_t kFlashTicks = 80;   /* ~80 ms */
static uint8_t s_b1_bot_flash = 0;
static uint8_t s_b1_top_flash = 0;

/* ── Colors ────────────────────────────────────────────────────────── */

using Rgb = LedPanel::Rgb;
static constexpr Rgb kColSawOut  {255, 110,   0};   /* output: orange spinner   */
static constexpr Rgb kColCvIn    {  0, 180, 200};   /* input:  cyan voltmeter   */
static constexpr Rgb kColModeOut { 30, 110,  20};   /* B2: output mode green    */
static constexpr Rgb kColModeIn  { 25,  25,  30};   /* B2: input mode dim grey  */
static constexpr Rgb kColFlash   {220, 220, 220};   /* B1: trigger flash white  */
static constexpr Rgb kColB1Idle  { 10,   8,   2};   /* B1: idle warm dim        */

/* ── Helpers ────────────────────────────────────────────────────────── */

static inline float SawVolts(float phase)
{
    /* Up-ramp saw: 0..1 → −5..+5 V. */
    return (phase * 2.0f - 1.0f) * 5.0f;
}

/* ── Audio callback (block-rate ~2 kHz) ─────────────────────────────── */

static void AudioCb(daisy::AudioHandle::InputBuffer  /*in*/,
                    daisy::AudioHandle::OutputBuffer /*out*/,
                    size_t                           size)
{
    const float dt = static_cast<float>(size) / hw.SampleRate();

    for (uint8_t k = 0; k < kNumCodecCvOuts; ++k)
    {
        float p = s_saw_phase_codec[k] + kSawFreqHzCodec[k] * dt;
        if (p >= 1.0f) p -= 1.0f;
        s_saw_phase_codec[k] = p;
        hw.cv_jacks[kNumCvInputs + k].SetVolts(SawVolts(p));
    }

    for (uint8_t i = 0; i < kNumCvInputs; ++i)
    {
        float p = s_saw_phase_analog[i] + kSawFreqHzAnalog[i] * dt;
        if (p >= 1.0f) p -= 1.0f;
        s_saw_phase_analog[i] = p;
    }
}

/* ── LED render (called from main loop at ~30 Hz) ───────────────────── */

static void RenderLeds()
{
    hw.leds.Clear();

    hw.leds.SetButton(kButtonB1, LedPanel::ButtonLed::Bottom,
                      s_b1_bot_flash > 0 ? kColFlash : kColB1Idle);
    hw.leds.SetButton(kButtonB1, LedPanel::ButtonLed::Top,
                      s_b1_top_flash > 0 ? kColFlash : kColB1Idle);

    hw.leds.SetButtonPair(kButtonB2, s_output_mode ? kColModeOut : kColModeIn);

    for (uint8_t i = 0; i < kNumCvInputs; ++i)
    {
        if (s_output_mode)
        {
            const float hour = s_saw_phase_analog[i] * 12.0f;
            hw.leds.SetRingByHour(i, hour, kColSawOut);
        }
        else
        {
            float v = hw.cv_jacks[i].Volts();
            if (v < -5.0f) v = -5.0f;
            if (v >  5.0f) v =  5.0f;
            const float norm = (v + 5.0f) / 10.0f;   /* 0..1 */
            const float hour = norm * 12.0f;
            hw.leds.SetRingByHour(i, hour, kColCvIn);
        }
    }

    hw.leds.Show();
}

/* ── Main ───────────────────────────────────────────────────────────── */

int main()
{
    hw.Init();

    /* Codec outs claimed for CV from boot. */
    hw.j9.EnableCvOutput();
    hw.j10.EnableCvOutput();

    hw.StartAudio(AudioCb);

    uint32_t led_div = 0;
    while (true)
    {
        hw.ProcessAllControls();

        /* B2 toggles switchable-jack direction. */
        if (hw.buttons[kButtonB2].RisingEdge())
        {
            s_output_mode = !s_output_mode;
            for (uint8_t i = 0; i < kNumCvInputs; ++i)
            {
                if (s_output_mode) hw.cv_jacks[i].EnableCvOutput();
                else                hw.cv_jacks[i].DisableCvOutput();
            }
        }

        /* Trigger latch consume → B1 accent flash. */
        if (hw.j1.RisingEdge()) s_b1_bot_flash = kFlashTicks;
        if (hw.j2.RisingEdge()) s_b1_top_flash = kFlashTicks;
        if (s_b1_bot_flash > 0) --s_b1_bot_flash;
        if (s_b1_top_flash > 0) --s_b1_top_flash;
        if (s_output_mode)
        {
            for (uint8_t i = 0; i < kNumCvInputs; ++i)
                hw.cv_jacks[i].SetVolts(SawVolts(s_saw_phase_analog[i]));
        }
        if ((led_div % 33u) == 0u) RenderLeds();
        ++led_div;

        daisy::System::Delay(1);
    }
}
