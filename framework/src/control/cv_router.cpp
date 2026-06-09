/**
 * @file control/cv_router.cpp
 * @brief alchemy::CvRouter implementation.
 */

#include "alchemy/control/cv_router.h"

namespace alchemy {

const CvRoute CvRouter::kEmpty = {};

CvRouter::CvRouter(uint8_t num_pots)
    : num_pots_((num_pots <= kMaxCvRouterPots) ? num_pots : kMaxCvRouterPots)
{
}

void CvRouter::Route(uint8_t pot, uint8_t cv_index, float attenuation, bool bipolar)
{
    if (pot >= num_pots_) return;
    routes_[pot] = CvRoute{static_cast<int8_t>(cv_index), attenuation, bipolar};
}

void CvRouter::Clear(uint8_t pot)
{
    if (pot >= num_pots_) return;
    routes_[pot] = CvRoute{};
}

const CvRoute& CvRouter::Get(uint8_t pot) const
{
    if (pot >= num_pots_) return kEmpty;
    return routes_[pot];
}

float CvRouter::Delta(uint8_t pot, const float* cv, uint8_t num_cv) const
{
    if (pot >= num_pots_) return 0.0f;
    const CvRoute& r = routes_[pot];
    if (r.cv_index < 0 || static_cast<uint8_t>(r.cv_index) >= num_cv) return 0.0f;

    const float raw = cv[r.cv_index];
    const float v   = r.bipolar ? (raw - 0.5f) : raw;
    return v * r.attenuation;
}

} // namespace alchemy
