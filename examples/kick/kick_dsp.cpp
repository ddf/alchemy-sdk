/**
 * kick_dsp.cpp — Kick voice implementation.
 *
 * Direct port of the DSP body from the original kick.cpp. Mapping curves
 * for pitch / sweep / drive / decay are baked in here, so the control side
 * just supplies normalised pot values.
 */

#include "kick_dsp.h"
#include <cmath>

namespace kick_dsp {

namespace {

/* ── Tuning constants ─────────────────────────────────────────────────── */

constexpr float kPitchMinHz     = 30.f;    /* sweep floor — never below this */
constexpr float kPitchBaseHz    = 40.f;    /* trigger pitch at P1 = 0         */
constexpr float kPitchSpanHz    = 200.f;   /* full P1 range adds 0..span Hz   */
constexpr float kSweepMinHzPerS = 50.f;    /* CW (P2 = 1) → slow sweep        */
constexpr float kSweepMaxHzPerS = 1000.f;  /* CCW (P2 = 0) → fast sweep       */
constexpr float kDriveMax       = 9.f;     /* 1 + P4 × 8                      */

/* ── Persistent voice state ──────────────────────────────────────────── */

float    env     = 0.f, pitch_ = 0.f, phase_ = 0.f;
float    t_env   = 0.f, t_phase = 0.f;
uint32_t noise_s = 0x12345678u;

} // namespace

float Process(const Params& p, bool trigger,
              daisy::AudioHandle::OutputBuffer out, size_t n, float sr)
{
    /* Hard retrigger: body + transient envelopes / phases all reset. */
    if (trigger)
    {
        env     = 1.f;
        pitch_  = kPitchBaseHz + p.pitch * kPitchSpanHz;
        phase_  = 0.f;
        t_env   = 1.f;
        t_phase = 0.f;
    }

    /* CW (P2 = 1) → slower sweep → longer pitch tail. */
    const float swp   = kSweepMinHzPerS
                      + (1.f - p.sweep) * (kSweepMaxHzPerS - kSweepMinHzPerS);
    const float drv   = 1.f + p.drive * (kDriveMax - 1.f);

    /* Exponential decay mapping: (1-dec) sweeps from 1e-3 (~21 ms) down to
     * 2.5e-5 (~833 ms) — perceptually even across the pot. */
    const float dec   = 1.f - .001f * powf(.025f, p.decay);

    const int   trans = p.transient;
    const float dt    = (sr > 0.f) ? (1.f / sr) : (1.f / 48000.f);

    for (size_t i = 0; i < n; i++)
    {
        /* ── Body: pitch-swept sine into hard clip ── */
        env    *= dec;
        pitch_  = fmaxf(kPitchMinHz, pitch_ - swp * dt);
        phase_  = fmodf(phase_ + pitch_ * dt, 1.f);
        float x = sinf(phase_ * 6.2832f) * env * drv;
        x       = fmaxf(-1.f, fminf(1.f, x));

        /* ── Transient layer (one-shot, additive) ── */
        float t = 0.f;
        switch (trans)
        {
            case kTransientNone:
                /* Zero the state so leaving this zone never pops with stale
                 * envelope from the previous trigger. */
                t_env = 0.f; t_phase = 0.f;
                break;

            case kTransientClick:  /* noise burst */
                t_env *= .990f;
                noise_s = noise_s * 1664525u + 1013904223u;
                t = (int32_t(noise_s) * (1.f / 2147483648.f)) * t_env;
                break;

            case kTransientTick:   /* 3 kHz sine ping */
                t_env  *= .997f;
                t_phase = fmodf(t_phase + 3000.f * dt, 1.f);
                t       = sinf(t_phase * 6.2832f) * t_env;
                break;

            case kTransientKnock:  /* 600 Hz sine burst */
                t_env  *= .998f;
                t_phase = fmodf(t_phase + 600.f * dt, 1.f);
                t       = sinf(t_phase * 6.2832f) * t_env * .8f;
                break;
        }

        out[0][i] = out[1][i] = (x + t * 2.f) * p.volume;
    }

    return env;
}

} // namespace kick_dsp
