/**
 * @file selector.h
 * @brief DrawSelector: discrete zone selector indicator for stepped parameters.
 *
 * Renders a ring where one of N zones is highlighted as the selected zone.
 * Three geometry modes are available — `Distributed` is the default
 * because it's the right shape for ~99 % of use cases (the user wants
 * to see "which slot of N am I at" and have every slot reachable
 * around the full ring):
 *
 *   ZoneGeometry::Distributed (default) — one LED per zone, positions
 *       evenly spread across the full arc.  Zone 0 lights LED 0, zone
 *       N-1 lights LED arc_leds-1, and the rest are placed
 *       proportionally in between.  Visually balanced for any N up to
 *       arc_leds; degrades gracefully (zones share LEDs) above that.
 *   ZoneGeometry::Point    — one LED per zone; zones clumped at arc
 *       steps 0..num_zones-1, leaving the rest of the ring dark.
 *       Rarely what the user wants; kept for callers who deliberately
 *       want zones stacked at one end.
 *   ZoneGeometry::Region   — each zone occupies a contiguous block of
 *       LEDs with single-LED gaps between zones.  Useful for binary
 *       selectors and 3-4 zone selectors where you want a "filled
 *       half/third of the ring" look rather than single pips.
 *
 * Unavailable zones (not set in avail_mask) are always drawn black.
 * The selected zone is drawn in active_color; other available zones in
 * a dimmed inactive_color (or black if inactive_color is {0,0,0}).
 *
 * Usage (default — Distributed):
 *   alchemy::SelectorDesc sel;
 *   sel.num_zones    = 8u;
 *   sel.active_color = {0xC0, 0x00, 0xFF};
 *   alchemy::DrawSelector(leds, pot_idx, kAlchemyLabArcGeometry, value, sel);
 */

#pragma once

#include <cstdint>
#include "alchemy/hardware_types.h"
#include "alchemy/led/panel.h"

namespace alchemy {

/* ── Zone geometry ──────────────────────────────────────────────────────── */

enum class ZoneGeometry : uint8_t
{
    /// One LED per zone, positions evenly distributed across the full arc.
    /// Zone 0 lights LED 0, zone N-1 lights LED arc_leds-1, the rest are
    /// spaced proportionally in between (`round(i * (arc_leds-1) / (N-1))`).
    /// Visually balanced and "fills the whole ring" for any N ≤ arc_leds.
    /// For N > arc_leds adjacent zones share LEDs (no fault, just collision).
    /// Default — almost always what the caller wants.
    Distributed,

    /// One LED per zone, zones clumped at arc steps 0..num_zones-1.
    /// The rest of the arc renders black.  Use only when you deliberately
    /// want zones stacked at one end of the ring.
    Point,

    /// Each zone occupies a contiguous block of LEDs with one-LED gaps.
    /// Requires `num_zones × 2 ≤ arc_leds + 1`; above that count
    /// `DrawSelector` auto-falls back to `Distributed`.
    Region,
};

/* ── Selector descriptor ────────────────────────────────────────────────── */

struct SelectorDesc
{
    uint8_t       num_zones      = 2u;
    ZoneGeometry  zone_geo       = ZoneGeometry::Distributed;
    LedPanel::Rgb active_color   = {0xFF, 0xFF, 0xFF};  ///< Selected zone color.
    LedPanel::Rgb inactive_color = {0u,   0u,   0u};    ///< Unselected color; {0,0,0} = off.
    float         inactive_dim   = 0.10f;                ///< Scale applied to inactive_color.
    uint16_t      avail_mask     = 0xFFFFu;              ///< Bitmask: bit N = zone N available.
};

/* ── DrawSelector ───────────────────────────────────────────────────────── */

/**
 * Draw a discrete zone selector on a pot ring.
 *
 * When `desc.zone_geo == Region` and the requested zone count is too
 * large to fit on the arc (each zone needs at least one LED plus a
 * one-LED gap to its neighbours), this function automatically falls
 * back to `Distributed` geometry for the draw rather than rendering a
 * fully blank ring.  No error is raised — the ring stays informative.
 *
 * @param dst        Target LED panel.
 * @param pot_idx    Ring index (0-based).
 * @param start_hour Clock-face hour of the CCW arc endpoint.
 * @param step_hours Hours per arc step.
 * @param arc_leds   Total arc steps (LED count for the ring).
 * @param value      Normalized value 0..1; maps to selected zone index.
 * @param desc       Selector style and zone parameters.
 */
void DrawSelector(LedPanel&           dst,
                  uint8_t             pot_idx,
                  float               start_hour,
                  float               step_hours,
                  uint8_t             arc_leds,
                  float               value,
                  const SelectorDesc& desc);

/** ArcGeometry overload — replaces the three geometry scalar arguments. */
inline void DrawSelector(LedPanel&           dst,
                         uint8_t             pot_idx,
                         const ArcGeometry&  geo,
                         float               value,
                         const SelectorDesc& desc)
{
    DrawSelector(dst, pot_idx, geo.start_hour, geo.step_hours, geo.arc_leds,
                 value, desc);
}

} // namespace alchemy
