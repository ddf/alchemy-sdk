/**
 * @file pip.cpp
 */

#include "alchemy/led/anims/pip.h"

namespace alchemy {

namespace {

/** True if @p c is not the literal black sentinel {0,0,0}. */
inline bool HasBackground(const LedPanel::Rgb& c)
{
    return c.r != 0u || c.g != 0u || c.b != 0u;
}

/**
 * Paint one step with weight @p k.  Blends over @p bg when a background
 * is set so the pip fades in from the ring tint instead of jumping to
 * full color; writes the color scaled by @p k directly otherwise.
 */
inline void PaintStep(LedPanel&            dst,
                      uint8_t              pot_idx,
                      uint8_t              ring_off,
                      const LedPanel::Rgb& color,
                      const LedPanel::Rgb& bg,
                      bool                 has_bg,
                      float                k)
{
    if (k <= 0.0f) return;
    if (k >  1.0f) k = 1.0f;

    const LedPanel::Rgb c = has_bg
        ? LedPanel::Mix(bg, color, k)
        : LedPanel::Scale(color, k);
    dst.SetRingByOffset(pot_idx, ring_off, dst.ScaleGlobal(c));
}

} // namespace

void DrawPip(LedPanel&      dst,
             uint8_t        pot_idx,
             float          start_hour,
             float          step_hours,
             uint8_t        arc_leds,
             float          value,
             const PipDesc& desc,
             uint32_t       t_ms)
{
    if (arc_leds == 0u) return;

    /* Blink: if blink_hz > 0, compute on/off state.  Returns before any
     * pixels are written — including the background — so a blinking pip
     * blinks the whole on/off cycle, background and all.                */
    if (desc.blink_hz > 0.0f)
    {
        const uint32_t half_ms = static_cast<uint32_t>(500.0f / desc.blink_hz);
        if (half_ms > 0u)
        {
            const bool blink_on = (t_ms / half_ms) % 2u == 0u;
            if (!blink_on) return;
        }
    }

    float v = value;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    const bool has_bg = HasBackground(desc.background);

    /* Fill the arc with the dim background first so subsequent pip writes
     * can blend over it.  Skipped when no background is set so this
     * primitive composes cleanly on top of other animations.            */
    if (has_bg)
    {
        for (uint8_t step = 0u; step < arc_leds; ++step)
        {
            const float   hour = start_hour + static_cast<float>(step) * step_hours;
            const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);
            dst.SetRingByOffset(pot_idx, off, dst.ScaleGlobal(desc.background));
        }
    }

    /* ── Smooth path: sub-LED two-tap interpolation + optional tails ── */
    if (desc.smooth && desc.width <= 1u)
    {
        /* center_f spans [0, arc_leds - 1] so endpoint values land
         * exactly on the first / last LED instead of beyond the arc. */
        const float center_f = v * static_cast<float>(arc_leds - 1u);
        const int   center_i = static_cast<int>(center_f);   /* truncation toward 0 — v ≥ 0 */
        const float frac     = center_f - static_cast<float>(center_i);

        auto step_at = [&](int step, float k) {
            if (step < 0 || step >= static_cast<int>(arc_leds)) return;
            const float   hour = start_hour + static_cast<float>(step) * step_hours;
            const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);
            PaintStep(dst, pot_idx, off, desc.color, desc.background, has_bg, k);
        };

        /* Two-tap pip — weights sum to 1.0 so total brightness is
         * conserved as the pip slides between adjacent LEDs.          */
        step_at(center_i,     1.0f - frac);
        step_at(center_i + 1, frac);

        /* Soft tails one step further out — anti-aliases motion at
         * higher speeds without smearing static positions.            */
        if (desc.tail_intensity > 0.0f)
        {
            const float tail = desc.tail_intensity;
            step_at(center_i - 1, tail * (1.0f - frac));
            step_at(center_i + 2, tail * frac);
        }
        return;
    }

    /* ── Quantised path: original LED-stepping behavior ─────────────── */

    uint8_t center_step = static_cast<uint8_t>(
        v * static_cast<float>(arc_leds - 1u) + 0.5f);
    if (center_step >= arc_leds)
        center_step = static_cast<uint8_t>(arc_leds - 1u);

    const int first_i = static_cast<int>(center_step) - static_cast<int>(desc.width) / 2;
    const int last_i  = first_i + static_cast<int>(desc.width) - 1;

    const int first = (first_i < 0) ? 0 : first_i;
    const int last  = (last_i >= static_cast<int>(arc_leds))
                          ? static_cast<int>(arc_leds) - 1
                          : last_i;

    for (int step = first; step <= last; ++step)
    {
        const float   hour = start_hour + static_cast<float>(step) * step_hours;
        const uint8_t off  = dst.RingOffsetForHour(pot_idx, hour);
        /* width-mode pips are full-intensity; blend with background
         * when one is set so the pip pixel doesn't clobber the tint. */
        PaintStep(dst, pot_idx, off, desc.color, desc.background, has_bg, 1.0f);
    }
}

} // namespace alchemy
