/**
 * @file color_morph_arc.h
 * @brief Color-morphing arc with snap-point markers.
 *
 * DrawColorMorphArc renders a filled arc whose color interpolates
 * continuously between a set of named snap points as the pot value sweeps
 * from 0 to 1.  Dim markers at each snap position help the user find the
 * positions by feel.  As the value approaches a snap, the fill color
 * saturates toward that snap's color (approach cue).  Within snap_tol of
 * a snap point the function reports at_snap = true.
 *
 * ColorMorphColor is provided separately for cases where only the color
 * is needed without drawing (e.g. to tint a companion parameter's arc).
 *
 * DrawSnapMarkers renders only the dim markers without the arc fill,
 * allowing callers to layer markers behind a DrawArc call.  Call order:
 *   DrawSnapMarkers(...)  // markers in passive region
 *   DrawArc(...)          // fill overwrites the active region
 */

#pragma once

#include <cstdint>
#include "alchemy/hardware_types.h"
#include "alchemy/led/panel.h"
#include "alchemy/led/anims/fill.h"  /* ArcSnapFrac */

namespace alchemy {

/** A single snap point on a color morph arc. */
struct MorphSnapPoint
{
    float         position;  ///< Normalized position 0..1 along the arc.
    LedPanel::Rgb color;     ///< Color displayed when value is at this position.
};

/** Return value from DrawColorMorphArc. */
struct ColorMorphResult
{
    LedPanel::Rgb fill_color;  ///< Interpolated (+ approach-saturated) fill color.
    bool          at_snap;     ///< True if within snap_tol of any snap point.
};

/**
 * Compute the interpolated morph color without drawing.
 *
 * @param value     Normalized value 0..1.
 * @param snaps     Snap-point array, sorted ascending by position.
 * @param num_snaps Number of snap points (>= 1).
 */
LedPanel::Rgb ColorMorphColor(float                 value,
                              const MorphSnapPoint* snaps,
                              uint8_t               num_snaps);

/**
 * Draw dim markers at snap-point positions without drawing the arc fill.
 *
 * Call this before DrawArc so that the arc fill overwrites markers in the
 * active region, leaving markers visible only in the passive region.
 *
 * @param dst           Target LED panel.
 * @param pot_idx       Ring index.
 * @param arc_start_hour  CCW arc endpoint, clock-face hours.
 * @param arc_step_hours  Hours per arc step.
 * @param arc_leds      Number of arc steps.
 * @param snaps         Snap-point array sorted ascending by position.
 * @param num_snaps     Number of snap points (>= 1).
 * @param marker_bright Brightness of the dim markers (e.g. 0.12f).
 */
void DrawSnapMarkers(LedPanel&             dst,
                     uint8_t               pot_idx,
                     float                 arc_start_hour,
                     float                 arc_step_hours,
                     uint8_t               arc_leds,
                     const MorphSnapPoint* snaps,
                     uint8_t               num_snaps,
                     float                 marker_bright = 0.12f);

/** ArcGeometry overload. */
inline void DrawSnapMarkers(LedPanel&             dst,
                             uint8_t               pot_idx,
                             const ArcGeometry&    geo,
                             const MorphSnapPoint* snaps,
                             uint8_t               num_snaps,
                             float                 marker_bright = 0.12f)
{
    DrawSnapMarkers(dst, pot_idx,
                    geo.start_hour, geo.step_hours, geo.arc_leds,
                    snaps, num_snaps, marker_bright);
}

/**
 * Draw a color-morphing arc with snap-point markers in one call.
 *
 * Combines DrawSnapMarkers + the fill in a single pass for efficiency.
 * Markers are only visible in the passive (unfilled) region.
 *
 * @param dst           Target LED panel.
 * @param pot_idx       Ring index.
 * @param arc_start_hour  CCW arc endpoint, clock-face hours.
 * @param arc_step_hours  Hours per arc step.
 * @param arc_leds      Number of arc steps.
 * @param value         Normalized value 0..1.
 * @param caught        False dims the fill to 30%.
 * @param snaps         Snap-point array sorted ascending by position.
 * @param num_snaps     Number of snap points (>= 1).
 * @param snap_tol      Distance within which at_snap is reported true.
 * @param approach_dist Distance over which fill saturates toward snap color.
 * @param marker_bright Brightness of the dim snap-position markers.
 */
ColorMorphResult DrawColorMorphArc(LedPanel&             dst,
                                   uint8_t               pot_idx,
                                   float                 arc_start_hour,
                                   float                 arc_step_hours,
                                   uint8_t               arc_leds,
                                   float                 value,
                                   bool                  caught,
                                   const MorphSnapPoint* snaps,
                                   uint8_t               num_snaps,
                                   float                 snap_tol      = 0.03f,
                                   float                 approach_dist = 0.10f,
                                   float                 marker_bright = 0.12f);

/** ArcGeometry overload. */
inline ColorMorphResult DrawColorMorphArc(LedPanel&             dst,
                                          uint8_t               pot_idx,
                                          const ArcGeometry&    geo,
                                          float                 value,
                                          bool                  caught,
                                          const MorphSnapPoint* snaps,
                                          uint8_t               num_snaps,
                                          float                 snap_tol      = 0.03f,
                                          float                 approach_dist = 0.10f,
                                          float                 marker_bright = 0.12f)
{
    return DrawColorMorphArc(dst, pot_idx,
                             geo.start_hour, geo.step_hours, geo.arc_leds,
                             value, caught, snaps, num_snaps,
                             snap_tol, approach_dist, marker_bright);
}

} // namespace alchemy
