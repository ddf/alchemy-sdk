/**
 * @file cv_router.h
 * @brief alchemy::CvRouter — per-pot CV routing with attenuation.
 *
 * A CvRouter holds an assignment table mapping each pot to a CV input (or
 * none), with multiplicative attenuation and optional bipolar conversion.
 * Used at the composition site (`knob(p)`) to add a CV modulation amount
 * to the stored pot value.
 *
 * Why map to a pot? It simplifies the mental model and makes clear
 * the relationship between a param lock, pot position, and CV input.
 *
 * Usage:
 *
 *   alchemy::CvRouter cv(alchemy::kNumPots);
 *   cv.Route(1, 0);              // CV0 → pot 1, default bipolar
 *   cv.Route(3, 2, 0.5f);        // CV2 → pot 3 at half attenuation
 *
 *   float knob(uint8_t p, void*) {
 *       return hw.pots[p].Value() + cv.Delta(p, cv_vals, num_cv);
 *   }
 *
 * Bipolar conversion: when `bipolar` is true (the default), the raw 0..1 CV
 * reading is shifted to -0.5..+0.5 before applying attenuation.  This matches
 * the AlchemyLab CV jack hardware where 0 V sits at 0.5 normalised.
 */

#pragma once

#include <cstdint>

namespace alchemy {

/** Maximum pots a single CvRouter can address. */
constexpr uint8_t kMaxCvRouterPots = 8u;

/** One routing entry: which CV input drives one pot, and how. */
struct CvRoute
{
    int8_t cv_index    = -1;   ///< Negative = unrouted; Delta returns 0.
    float  attenuation = 1.0f;
    bool   bipolar     = true;
};

class CvRouter
{
  public:
    /** Initialise with @p num_pots routable slots, all unrouted. */
    explicit CvRouter(uint8_t num_pots);

    /** Assign one pot to a CV source. */
    void Route(uint8_t pot, uint8_t cv_index,
               float attenuation = 1.0f, bool bipolar = true);

    /** Clear the assignment for one pot. */
    void Clear(uint8_t pot);

    /** Read-only access to the route for one pot. */
    const CvRoute& Get(uint8_t pot) const;

    /**
     * Compute the CV modulation delta for one pot.
     *
     * @param pot     Pot index.
     * @param cv      CV readings [0..1], length ≥ num_cv.
     * @param num_cv  Number of valid CV readings.
     * @return        0 if no route is assigned, otherwise the routed
     *                CV value (optionally bipolar-shifted) × attenuation.
     */
    float Delta(uint8_t pot, const float* cv, uint8_t num_cv) const;

    uint8_t NumPots() const { return num_pots_; }

  private:
    CvRoute routes_[kMaxCvRouterPots] = {};
    uint8_t num_pots_ = 0u;

    static const CvRoute kEmpty;
};

} // namespace alchemy
