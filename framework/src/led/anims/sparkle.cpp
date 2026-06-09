/**
 * @file sparkle.cpp
 */

#include "alchemy/led/anims/sparkle.h"
#include <cmath>

namespace alchemy {

namespace {

/** xorshift32 — fast, in-line, no globals.  Mutates @p state.rng. */
inline uint32_t XorshiftU32(SparkleState& state)
{
    uint32_t x = state.rng;
    if (x == 0u) x = 0xA5A5A5A5u;  /* guard against a zero seed killing the stream */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state.rng = x;
    return x;
}

/** Uniform float in [0, 1). */
inline float XorshiftUnit(SparkleState& state)
{
    return static_cast<float>(XorshiftU32(state))
         * (1.0f / 4294967296.0f);
}

inline bool HasBackground(const LedPanel::Rgb& c)
{
    return c.r != 0u || c.g != 0u || c.b != 0u;
}

} // namespace

void DrawSparkle(LedPanel&          dst,
                 uint8_t            pot_idx,
                 float              start_hour,
                 float              step_hours,
                 uint8_t            arc_leds,
                 float              density,
                 uint32_t           dt_ms,
                 SparkleState&      state,
                 const SparkleDesc& desc)
{
    if (arc_leds == 0u) return;
    if (arc_leds > kMaxSparkleArcLeds)
        arc_leds = kMaxSparkleArcLeds;

    /* ── Background ─────────────────────────────────────────────────── */

    const bool has_bg = HasBackground(desc.passive_color);
    if (has_bg && desc.compose == FillCompose::Replace)
    {
        for (uint8_t step = 0u; step < arc_leds; ++step)
        {
            const float   hour = start_hour + static_cast<float>(step) * step_hours;
            const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);
            dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(desc.passive_color));
        }
    }

    /* ── Decay any active sparkles ──────────────────────────────────── */
    /* decay_per_100ms is the *remaining* fraction after 100 ms; raising
     * it to (dt_ms / 100) gives the per-frame multiplier, so the visible
     * decay rate is independent of render-loop jitter.                  */
    if (dt_ms > 0u)
    {
        float decay_remaining = desc.decay_per_100ms;
        if (decay_remaining < 0.0f) decay_remaining = 0.0f;
        if (decay_remaining > 1.0f) decay_remaining = 1.0f;

        const float decay = std::pow(decay_remaining,
                                     static_cast<float>(dt_ms) * 0.01f);

        for (uint8_t s = 0u; s < arc_leds; ++s)
        {
            if (state.intensity[s] > 0.0f)
            {
                state.intensity[s] *= decay;
                if (state.intensity[s] < 0.01f)
                    state.intensity[s] = 0.0f;
            }
        }
    }

    /* ── Spawn new sparkles ─────────────────────────────────────────── */
    /* Expected count for this frame is lambda * dt.  We consume that
     * expectation one whole sparkle at a time: while >= 1 the spawn is
     * effectively guaranteed (uniform draw is always < remaining), and
     * the fractional remainder is the probability of the final spawn.
     * Guarded by arc_leds — a frame cannot light more LEDs than exist. */
    float d = density;
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;

    const float lambda_per_s = d * desc.max_rate_hz;
    float       p_remaining  = lambda_per_s
                             * (static_cast<float>(dt_ms) * 0.001f);

    const float spawn_lo = desc.min_brightness;
    const float spawn_hi = desc.max_brightness;
    const float spawn_span = (spawn_hi > spawn_lo) ? (spawn_hi - spawn_lo) : 0.0f;

    for (uint8_t guard = 0u;
         guard < arc_leds && p_remaining > 0.0f;
         ++guard)
    {
        if (XorshiftUnit(state) >= p_remaining) break;

        const uint8_t s         = static_cast<uint8_t>(XorshiftU32(state) % arc_leds);
        const float   intensity = spawn_lo + spawn_span * XorshiftUnit(state);

        /* Brighten only — never knock back an already-brighter sparkle. */
        if (intensity > state.intensity[s])
            state.intensity[s] = intensity;

        p_remaining -= 1.0f;
    }

    /* ── Paint sparkles on top of the background ────────────────────── */

    for (uint8_t step = 0u; step < arc_leds; ++step)
    {
        const float k = state.intensity[step];
        if (k <= 0.0f) continue;

        const float   hour = start_hour + static_cast<float>(step) * step_hours;
        const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);

        /* Blend the sparkle onto whatever is there: scale by intensity,
         * or mix with the background colour when one is in use so the
         * passive tint shows through at sub-LED brightness.            */
        const LedPanel::Rgb c = has_bg
            ? LedPanel::Mix(desc.passive_color, desc.spark_color, k)
            : LedPanel::Scale(desc.spark_color, k);
        dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(c));
    }
}

} // namespace alchemy
