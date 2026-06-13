/**
 * @file sparkle.h
 * @brief DrawSparkle: sparse decaying-spark scatter across a pot ring.
 *
 * A density-driven generative animation suited to noise-like or
 * "shimmer" parameters.  Each frame, zero or more sparkles spawn on
 * random arc steps; existing sparkles fade exponentially to black.
 * Spawn rate and per-spark brightness are independently configurable,
 * so the same primitive renders anything from a barely-visible drizzle
 * (low density, short decay) to a continuous glitter (high density,
 * long decay).
 *
 * State (per-step intensities + RNG seed) lives in a caller-owned
 * SparkleState so the panel framework remains stateless — instantiate
 * one SparkleState per ring you want to sparkle.
 *
 * The animation is time-based: pass the milliseconds elapsed since the
 * previous call so the spawn rate and decay tracks wall-clock time and
 * is insensitive to render-loop jitter.
 *
 * Usage:
 *   alchemy::SparkleState air_state;       // typically a class member
 *   ...
 *   const uint32_t now    = daisy::System::GetNow();
 *   const uint32_t dt_ms  = now - last_render_ms;
 *   last_render_ms        = now;
 *
 *   alchemy::SparkleDesc desc;
 *   desc.spark_color   = {0xFF, 0xFF, 0xFF};
 *   desc.passive_color = {0x02, 0x02, 0x04};
 *   alchemy::DrawSparkle(leds, pot_idx, kAlchemyLabArcGeometry,
 *                        density, dt_ms, air_state, desc);
 */

#pragma once

#include <cstdint>
#include "alchemy/hardware_types.h"
#include "alchemy/led/panel.h"
#include "alchemy/led/anims/fill.h"   /* FillCompose */

namespace alchemy {

/* ── Capacity ────────────────────────────────────────────────────────────── */

/**
 * Largest arc the per-step intensity buffer in SparkleState can address.
 * Sized to the Alchemy Lab V1 ring (16 LEDs); covers every shipping
 * board and any reasonable single-ring layout.
 */
constexpr uint8_t kMaxSparkleArcLeds = 16u;

/* ── Sparkle state (caller-owned) ───────────────────────────────────────── */

/**
 * Per-ring sparkle bookkeeping.  Default-constructible.  Instantiate one
 * per ring you want to sparkle and pass it to DrawSparkle each frame.
 *
 * intensity[step] holds the current 0..1 brightness of a fading sparkle
 * at that arc step.  rng holds the xorshift32 stream.
 */
struct SparkleState
{
    float    intensity[kMaxSparkleArcLeds] = {};
    uint32_t rng                            = 0xA5A5A5A5u;
};

/* ── Sparkle descriptor ─────────────────────────────────────────────────── */

struct SparkleDesc
{
    LedPanel::Rgb spark_color     = {0xFF, 0xFF, 0xFF};    ///< Color of a freshly-spawned sparkle.
    LedPanel::Rgb passive_color   = {0u, 0u, 0u};          ///< Background fill; {0,0,0} = none.
    float         max_rate_hz     = 12.0f;                  ///< Spawn rate (sparkles/sec) at density = 1.
    float         min_brightness  = 0.5f;                   ///< Lower bound of random spawn intensity (0..1).
    float         max_brightness  = 1.0f;                   ///< Upper bound of random spawn intensity (0..1).
    float         decay_per_100ms = 0.20f;                  ///< Fraction *remaining* after 100 ms (0.2 ≈ 80% gone).
    FillCompose   compose         = FillCompose::Replace;   ///< Replace: write passive_color too. Overlay: only sparkles.
};

/* ── DrawSparkle ────────────────────────────────────────────────────────── */

/**
 * Render one frame of the sparkle animation on a pot ring.
 *
 * Advances @p state in place: decays existing sparkles by the
 * configured rate, then spawns 0..N new ones with probability scaled by
 * @p density and @p dt_ms.
 *
 * @param dst         Target LED panel.
 * @param pot_idx     Ring index (0-based).
 * @param start_hour  Clock-face hour of the CCW arc endpoint.
 * @param step_hours  Hours per arc step.
 * @param arc_leds    Total arc steps; clamped to kMaxSparkleArcLeds.
 * @param density     Normalised density 0..1; scales spawn rate against desc.max_rate_hz.
 * @param dt_ms       Milliseconds since the previous DrawSparkle call.
 * @param state       Caller-owned per-step intensity buffer + RNG seed.
 * @param desc        Sparkle style and timing.
 */
void DrawSparkle(LedPanel&          dst,
                 uint8_t            pot_idx,
                 float              start_hour,
                 float              step_hours,
                 uint8_t            arc_leds,
                 float              density,
                 uint32_t           dt_ms,
                 SparkleState&      state,
                 const SparkleDesc& desc = {});

/** ArcGeometry overload — replaces the three geometry scalar arguments. */
inline void DrawSparkle(LedPanel&          dst,
                        uint8_t            pot_idx,
                        const ArcGeometry& geo,
                        float              density,
                        uint32_t           dt_ms,
                        SparkleState&      state,
                        const SparkleDesc& desc = {})
{
    DrawSparkle(dst, pot_idx, geo.start_hour, geo.step_hours, geo.arc_leds,
                density, dt_ms, state, desc);
}

} // namespace alchemy
