/**
 * @file alchemy/led/anim_desc.h
 * @brief LED ring animation style enumerations + ParamSlot descriptor.
 *
 * `ArcStyle` and `PipStyle` are used by `ParamSlot` (declared here) to
 * describe how each pot ring should be rendered by `PerfRenderer`.  The
 * developer populates a `ParamSlot[]` (or uses the `LedRender::Binder`
 * fluent wrapper) and `PerfRenderer` walks the array each frame.
 *
 * When arc_style is ArcStyle::None the renderer skips that pot entirely,
 * leaving the developer free to render it manually using the animation
 * primitives in alchemy/led/animations.h.
 */

#pragma once

#include <cstdint>
#include "alchemy/led/panel.h"
#include "alchemy/led/anims/fill.h"             /* FillAnim, FillMode */
#include "alchemy/led/anims/selector.h"         /* ZoneGeometry */
#include "alchemy/led/anims/color_morph_arc.h"  /* MorphSnapPoint */

namespace alchemy {

/**
 * How to draw the arc portion of a pot ring.
 *
 *  Level    — filled arc from the CCW stop toward CW.  The standard
 *             unipolar control (time, mix, depth, …).
 *  Bipolar  — fan from the centre step outward in two arms.  Use for
 *             bipolar controls (damping, attenuverter, …).
 *  Selector — discrete zone indicator driven by DrawSelector().
 *  Gradient — color-morphing filled arc with snap-point markers.  Use
 *             for model-morph / waveform-select / filter-type controls
 *             that interpolate between a handful of named positions.
 *  GradientFill — plain Level fill whose color is sampled from a sibling
 *             pot's value via the snap-point gradient (arc_color_src_pot).
 *             Use for a depth/amount control tinted by a companion
 *             morph/model control on the same voice.
 *  None     — developer handles this pot's arc completely.
 */
enum class ArcStyle : uint8_t
{
    None,
    Level,
    Bipolar,
    Selector,
    Gradient,
    GradientFill,
};

/**
 * How to draw the bottom-pip of a pot ring.
 *
 *  None           — no pip.
 *  Solid          — always-on pip in pip_color.
 *  ThresholdSnap  — pip_color when value is in [snap_lo, snap_hi];
 *                   snap_hi_color when value exceeds snap_hi;
 *                   off below snap_lo.
 *  GradientSnap   — for a Gradient arc: lit in the morph fill color when the
 *                   value sits on a snap point and the pot is caught; off
 *                   otherwise.  A snap-confirmation cue for model-morph pots.
 */
enum class PipStyle : uint8_t
{
    None,
    Solid,
    ThresholdSnap,
    GradientSnap,
};

/**
 * Per-pot arc descriptor consumed by PerfRenderer.
 *
 * The renderer treats arc_style == None as "skip this slot entirely" so the
 * developer can take it over.
 */
struct ParamSlot
{
    ArcStyle      arc_style        = ArcStyle::None;
    LedPanel::Rgb arc_color        = {0x80, 0x80, 0x80};
    LedPanel::Rgb arc_alt_color    = {0x00, 0x00, 0x00};
    LedPanel::Rgb arc_center_color = {0x80, 0x80, 0x80};
    FillAnim      arc_anim         = FillAnim::None;
    float         arc_anim_depth   = 0.4f;
    ZoneGeometry  arc_zone_geo     = ZoneGeometry::Distributed;
    uint8_t       arc_num_zones    = 8u;
    float         arc_inactive_dim = 0.0f;
    uint16_t      arc_avail_mask   = 0xFFFFu;
    /* Gradient (ArcStyle::Gradient) — snap-point array referenced by ptr to
     * keep ParamSlot trivially copyable.  Caller owns storage; the array
     * must outlive every Render() call that consults this slot. */
    const MorphSnapPoint* arc_snaps        = nullptr;
    uint8_t               arc_num_snaps    = 0;
    /* GradientFill (ArcStyle::GradientFill) — pot index whose value selects
     * the fill color through arc_snaps.  0xFF means "no source": the fill
     * falls back to arc_color. */
    uint8_t               arc_color_src_pot = 0xFFu;
    PipStyle      pip_style        = PipStyle::None;
    LedPanel::Rgb pip_color        = {0xFF, 0xFF, 0xFF};
    float         snap_lo          = 0.90f;
    float         snap_hi          = 0.92f;
    LedPanel::Rgb snap_hi_color    = {0xFF, 0x00, 0x00};
};

} // namespace alchemy
