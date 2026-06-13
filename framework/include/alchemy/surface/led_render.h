/**
 * @file alchemy/surface/led_render.h
 * @brief alchemy::LedRender — opt-in declarative LED ring rendering surface.
 *
 * Owns a single-page Binder and a PerfRenderer.  The user populates the Binder
 * with per-pot arc styles (one ring per pot), and Render() walks them each
 * frame using a value-provider function pointer.
 *
 * Pages: LedRender is single-page.  If the user has multiple pages with
 * different stylings, they rebuild the bindings on page change (cheap — it's
 * a flat array of structs) or call animation primitives directly.
 *
 * Catch state: pass an optional `KnobStorage*` at construction so
 * unboundedly-bright rings dim when their pot is uncaught.  If null, all pots
 * render fully bright.  `Pager` is one `KnobStorage`, but any param-keyed or
 * custom storage works — the surface reads `State(ActivePage(), pot)`.
 * 
 * TODO: not a huge fan of this implementation.
 */

#pragma once

#include <cstdint>
#include "alchemy/hardware_types.h"        /* ArcGeometry */
#include "alchemy/led/anim_desc.h"
#include "alchemy/led/panel.h"
#include "alchemy/led/perf_renderer.h"     /* PerfRenderer */

namespace alchemy {

class KnobStorage;

/* ── LedBinder — single-page fluent arc-declaration table ────────────── */

class LedBinder
{
  public:
    static constexpr uint8_t kMaxPots = 8u;

    class PotRef
    {
      public:
        PotRef(LedBinder& b, uint8_t pot) : b_(b), pot_(pot) {}

        PotRef& Style       (ArcStyle s)        { Slot().arc_style        = s; return *this; }
        PotRef& Anim        (FillAnim a)        { Slot().arc_anim         = a; return *this; }
        PotRef& AnimDepth   (float    d)        { Slot().arc_anim_depth   = d; return *this; }
        PotRef& Color       (LedPanel::Rgb c)   { Slot().arc_color        = c; return *this; }
        PotRef& AltColor    (LedPanel::Rgb c)   { Slot().arc_alt_color    = c; return *this; }
        PotRef& CenterColor (LedPanel::Rgb c)   { Slot().arc_center_color = c; return *this; }
        PotRef& Pip         (PipStyle s)        { Slot().pip_style        = s; return *this; }
        PotRef& PipColor    (LedPanel::Rgb c)   { Slot().pip_color        = c; return *this; }
        PotRef& SnapRange   (float lo, float hi){ Slot().snap_lo = lo; Slot().snap_hi = hi; return *this; }
        PotRef& SnapOverColor(LedPanel::Rgb c)  { Slot().snap_hi_color    = c; return *this; }
        PotRef& ZoneGeo     (ZoneGeometry g)    { Slot().arc_zone_geo     = g; return *this; }
        PotRef& NumZones    (uint8_t n)         { Slot().arc_num_zones    = n; return *this; }
        PotRef& InactiveDim (float d)           { Slot().arc_inactive_dim = d; return *this; }
        PotRef& AvailMask   (uint16_t m)        { Slot().arc_avail_mask   = m; return *this; }

        /**
         * Snap-point array for ArcStyle::Gradient (model-morph) and
         * ArcStyle::GradientFill (cross-knob-tinted level fill).  The
         * caller owns the array; it must outlive every Render() that
         * consults this slot.  Identical contract to ParamSlot::arc_snaps
         * directly — this setter just routes through PotRef so callers
         * who already use the fluent binder don't have to break out into
         * direct ParamSlot mutation.
         */
        PotRef& Snaps(const MorphSnapPoint* snaps, uint8_t num_snaps)
        {
            Slot().arc_snaps     = snaps;
            Slot().arc_num_snaps = num_snaps;
            return *this;
        }

        /**
         * Pot index whose value selects the fill color for
         * ArcStyle::GradientFill (via the snap-point array set with
         * Snaps()).  0xFF (the ParamSlot default) means no source —
         * the fill falls back to arc_color.
         */
        PotRef& ColorSrcPot(uint8_t pot)
        {
            Slot().arc_color_src_pot = pot;
            return *this;
        }

        /**
         * Bulk-copy every field from @p src into this slot.  Convenient
         * when the user already has a fully-configured ParamSlot — for
         * example a `VirtualKnob::Slot()` from the declarative-knob path —
         * and wants the LedBinder to mirror it without spelling out every
         * setter call.  Replaces all prior settings on this slot.
         */
        PotRef& FromSlot(const ParamSlot& src) { Slot() = src; return *this; }

      private:
        ParamSlot& Slot();
        LedBinder& b_;
        uint8_t    pot_;
    };

    PotRef Pot(uint8_t pot) { return PotRef(*this, pot); }

    const ParamSlot* Slots() const { return slots_; }

    /** Reset every slot to default (ArcStyle::None — renderer skips). */
    void Clear()
    {
        for (uint8_t i = 0; i < kMaxPots; i++) slots_[i] = ParamSlot{};
    }

  private:
    friend class PotRef;
    ParamSlot slots_[kMaxPots] = {};
};

inline ParamSlot& LedBinder::PotRef::Slot() { return b_.slots_[pot_]; }

/* ── LedRender ───────────────────────────────────────────────────────── */

class LedRender
{
  public:
    using ValueProvider = float (*)(uint8_t pot, void* ctx);

    /**
     * @param panel    Initialised LED panel (e.g. hw.leds).
     * @param geo      Arc geometry (e.g. hw.Arc()).
     * @param value    Per-pot value provider (composition site).
     * @param ctx      Opaque context handed to @p value.
     * @param storage  Optional KnobStorage for catch-state dimming; null =
     *                 always caught.  (`Pager*` binds here implicitly.)
     * @param num_pots Pots to render (default: all of storage's, or kMaxPots).
     */
    LedRender(LedPanel&          panel,
              const ArcGeometry& geo,
              ValueProvider      value,
              void*              ctx,
              const KnobStorage* storage = nullptr,
              uint8_t            num_pots = 0u);

    /** Access the fluent binder to declare per-pot styles. */
    LedBinder& Binder() { return binder_; }

    /** Render all bound pots into the panel.  Does NOT clear or show. */
    void Render(uint32_t t_ms);

  private:
    LedPanel*          panel_;
    PerfRenderer       renderer_;
    ValueProvider      value_;
    void*              ctx_;
    const KnobStorage* storage_;
    uint8_t            num_pots_;
    LedBinder          binder_;
};

} // namespace alchemy
