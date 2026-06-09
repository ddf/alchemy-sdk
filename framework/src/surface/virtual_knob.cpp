/**
 * @file surface/virtual_knob.cpp
 * @brief alchemy::VirtualKnob implementation — math helpers only.
 *
 * Everything stateful is inline in the header (small, ISR-callable). The
 * Pow() helper lives here to keep <cmath> out of users' translation units.
 */

#include "alchemy/surface/virtual_knob.h"
#include <cmath>

namespace alchemy {

float VirtualKnob::Pow(float base, float exponent)
{
    return std::pow(base, exponent);
}

} // namespace alchemy
