/**
 * @file selector.cpp
 */

#include "alchemy/led/anims/selector.h"

namespace alchemy {

namespace {

/* Even-distribution position for zone @p z of @p num_zones across an
 * arc of @p arc_leds.  Zone 0 always lands on LED 0; zone num_zones-1
 * always lands on LED arc_leds-1.  Internal zones are spaced
 * proportionally with half-up rounding. */
inline uint8_t DistributedZonePosition(uint8_t z, uint8_t num_zones, uint8_t arc_leds)
{
    if (arc_leds == 0u)   return 0u;
    if (num_zones <= 1u)  return 0u;
    const float frac = static_cast<float>(z) / static_cast<float>(num_zones - 1u);
    const float pos  = frac * static_cast<float>(arc_leds - 1u) + 0.5f;
    uint8_t step = static_cast<uint8_t>(pos);
    if (step >= arc_leds) step = static_cast<uint8_t>(arc_leds - 1u);
    return step;
}

/* Pick the zone whose LED is nearest to where the pot is pointing.
 * With endpoint-anchored zone positions at fractions z/(N-1) of the
 * arc, the boundaries between zones sit at midpoints (z+0.5)/(N-1).
 * Implemented as round(value * (N-1)) with half-up rounding. */
inline uint8_t SelectNearestZone(float value, uint8_t num_zones)
{
    if (num_zones <= 1u) return 0u;
    const float pos = value * static_cast<float>(num_zones - 1u) + 0.5f;
    if (pos < 0.0f) return 0u;
    uint8_t z = static_cast<uint8_t>(pos);
    if (z >= num_zones) z = static_cast<uint8_t>(num_zones - 1u);
    return z;
}

/* Pick the color for an *available* zone: active or dimmed inactive. */
inline LedPanel::Rgb ZoneColor(const SelectorDesc& desc, uint8_t z, uint8_t selected)
{
    if (z == selected) return desc.active_color;
    if (desc.inactive_color.r == 0u
     && desc.inactive_color.g == 0u
     && desc.inactive_color.b == 0u)
    {
        return {0u, 0u, 0u};
    }
    return LedPanel::Scale(desc.inactive_color, desc.inactive_dim);
}

} // namespace

void DrawSelector(LedPanel&           dst,
                  uint8_t             pot_idx,
                  float               start_hour,
                  float               step_hours,
                  uint8_t             arc_leds,
                  float               value,
                  const SelectorDesc& desc)
{
    if (desc.num_zones == 0u) return;

    /* ── Distributed geometry (default) ──────────────────────────────
     * One LED per zone, endpoint-anchored: zone 0 → LED 0, zone N-1
     * → LED arc_leds-1, internals spaced proportionally with half-up
     * rounding.  Selection picks the zone whose LED is nearest the
     * pot's pointed position so the lit LED tracks pot angle. */
    if (desc.zone_geo == ZoneGeometry::Distributed)
    {
        const uint8_t selected = SelectNearestZone(value, desc.num_zones);

        for (uint8_t step = 0u; step < arc_leds; ++step)
        {
            const float   hour = start_hour + static_cast<float>(step) * step_hours;
            const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);
            dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(LedPanel::Rgb{0u, 0u, 0u}));
        }
        for (uint8_t z = 0u; z < desc.num_zones; ++z)
        {
            if (!(desc.avail_mask & static_cast<uint16_t>(1u << z))) continue;
            const uint8_t step = DistributedZonePosition(z, desc.num_zones, arc_leds);
            const float   hour = start_hour + static_cast<float>(step) * step_hours;
            const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);
            const LedPanel::Rgb c = ZoneColor(desc, z, selected);
            dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(c));
        }
        return;
    }

    /* ── Point geometry ──────────────────────────────────────────────
     * Legacy mode: zones clumped at LED 0..num_zones-1, rest dark. */
    if (desc.zone_geo == ZoneGeometry::Point)
    {
        uint8_t selected = static_cast<uint8_t>(value * static_cast<float>(desc.num_zones));
        if (selected >= desc.num_zones)
            selected = static_cast<uint8_t>(desc.num_zones - 1u);

        for (uint8_t step = 0u; step < arc_leds; ++step)
        {
            const float   hour = start_hour + static_cast<float>(step) * step_hours;
            const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);

            LedPanel::Rgb c{0u, 0u, 0u};

            if (step < desc.num_zones)
            {
                if (!(desc.avail_mask & static_cast<uint16_t>(1u << step)))
                    c = {0u, 0u, 0u};
                else
                    c = ZoneColor(desc, step, selected);
            }

            dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(c));
        }
        return;
    }

    /* ── Region geometry ─────────────────────────────────────────────
     * Each zone owns an equal slot of the arc: slot z covers LEDs
     * [floor(z·arc/N), floor((z+1)·arc/N)).  The last LED of every
     * non-final slot is the gap; the final slot extends through
     * LED arc_leds-1, so zone 0 is anchored at the CCW endpoint and
     * zone N-1 at the CW endpoint.  Falls back to Distributed when
     * slots are too narrow (arc_leds + 1 < 2N) for blocks + gaps. */
    {
        if (static_cast<uint16_t>(arc_leds) + 1u
            < static_cast<uint16_t>(desc.num_zones) * 2u)
        {
            SelectorDesc fb = desc;
            fb.zone_geo = ZoneGeometry::Distributed;
            DrawSelector(dst, pot_idx, start_hour, step_hours, arc_leds,
                         value, fb);
            return;
        }

        const uint16_t N = desc.num_zones;
        uint8_t selected = static_cast<uint8_t>(value * static_cast<float>(N));
        if (selected >= desc.num_zones)
            selected = static_cast<uint8_t>(desc.num_zones - 1u);

        for (uint8_t step = 0u; step < arc_leds; ++step)
        {
            const float   hour = start_hour + static_cast<float>(step) * step_hours;
            const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);

            const uint8_t zone_idx = static_cast<uint8_t>(
                (static_cast<uint16_t>(step) * N) / arc_leds);
            const uint8_t slot_end = static_cast<uint8_t>(
                ((static_cast<uint16_t>(zone_idx) + 1u) * arc_leds + N - 1u) / N - 1u);

            const bool is_final = (zone_idx == desc.num_zones - 1u);
            const bool is_gap   = (step == slot_end) && !is_final;

            LedPanel::Rgb c{0u, 0u, 0u};
            if (!is_gap
             && (desc.avail_mask & static_cast<uint16_t>(1u << zone_idx)))
                c = ZoneColor(desc, zone_idx, selected);

            dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(c));
        }
    }
}

} // namespace alchemy
