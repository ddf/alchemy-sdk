/**
 * @file clock_div.h
 * @brief Musical clock-division table for clock-sync delay modes.
 *
 * When a clock source (ClockPll for a tempo number, or ClockFollower /
 * MusicalClock for a phase-aware timeline) has acquired lock, the
 * delay-time pot is reinterpreted as a discrete zone selector across 13
 * musical divisions of the locked quarter-note (index 0 = shortest, 12 =
 * longest):
 *
 *   0   64th        1   32nd        2   16th-triplet   3   16th
 *   4   8th-triplet 5   8th         6   Qtr-triplet    7   Quarter
 *   8   Half        9   Dotted-Half 10  Whole          11  Dotted-Whole
 *  12  Double-Whole
 *
 * TODO: Make this more generic.
 * Unavailable zones (delay outside [min_delay_s, max_delay_s]) are
 * excluded from the availability bitmask so the user can't select them.
 */

#pragma once

#include <cstdint>

namespace alchemy {

constexpr uint8_t kNumClockDivs = 13u;

/** Quarter-note multiplier for each division index. */
constexpr float kClockDivMul[kNumClockDivs] = {
    1.0f / 16.0f,   /* 64th              */
    1.0f / 8.0f,    /* 32nd              */
    1.0f / 6.0f,    /* 16th triplet      */
    1.0f / 4.0f,    /* 16th              */
    1.0f / 3.0f,    /* 8th triplet       */
    1.0f / 2.0f,    /* 8th               */
    2.0f / 3.0f,    /* Quarter triplet   */
    1.0f,           /* Quarter           */
    2.0f,           /* Half              */
    3.0f,           /* Dotted half       */
    4.0f,           /* Whole             */
    6.0f,           /* Dotted whole      */
    8.0f,           /* Double whole      */
};

/** True if the division at @p idx is a triplet subdivision. */
inline bool ClockDivIsTriplet(uint8_t idx)
{
    return (idx == 2u) || (idx == 4u) || (idx == 6u);
}

/** True if @p idx is the "Quarter" (1:1) anchor zone. */
inline bool ClockDivIsQuarter(uint8_t idx) { return idx == 7u; }

/** True if @p idx is a dotted value (Dotted-Half or Dotted-Whole). */
inline bool ClockDivIsDotted(uint8_t idx)
{
    return (idx == 9u) || (idx == 11u);
}

/**
 * Bitmask of divisions whose resulting delay time falls within
 * [min_delay_s, max_delay_s] for the given locked quarter-note period.
 * Bit i is set when division i is available for selection.
 *
 * @param quarter_seconds  Current quarter-note period from the active clock
 *                         source — `ClockPll::QuarterSeconds()`, or
 *                         `60.0f / MusicalClock::Bpm()` /
 *                         `60.0f / ClockFollower::Bpm()`.
 * @param min_delay_s      Platform minimum delay time in seconds.
 * @param max_delay_s      Platform maximum delay time in seconds.
 */
inline uint16_t ClockDivAvailableMask(float quarter_seconds,
                                      float min_delay_s,
                                      float max_delay_s)
{
    if (quarter_seconds <= 0.0f) return 0u;
    uint16_t mask = 0u;
    for (uint8_t i = 0; i < kNumClockDivs; i++)
    {
        const float s = quarter_seconds * kClockDivMul[i];
        if (s >= min_delay_s && s <= max_delay_s)
            mask |= static_cast<uint16_t>(1u << i);
    }
    return mask;
}

/** Delay time in seconds for division @p idx at the given quarter period. */
inline float ClockDivSeconds(uint8_t idx, float quarter_seconds)
{
    if (idx >= kNumClockDivs) idx = kNumClockDivs - 1u;
    return quarter_seconds * kClockDivMul[idx];
}

/**
 * Map a normalised pot value [0, 1] to the nearest available division zone.
 * Zones are evenly spaced; the result is clamped to the nearest set bit
 * in @p avail_mask so unavailable zones can never be selected.
 *
 * Returns 0 if @p avail_mask is 0 (caller must check before entering div mode).
 */
inline uint8_t ClockDivZoneFromNorm(float v, uint16_t avail_mask)
{
    if (avail_mask == 0u) return 0u;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    int z = static_cast<int>(v * static_cast<float>(kNumClockDivs));
    if (z < 0) z = 0;
    if (z >= static_cast<int>(kNumClockDivs)) z = kNumClockDivs - 1;

    if (avail_mask & static_cast<uint16_t>(1u << z))
        return static_cast<uint8_t>(z);

    for (int d = 1; d < kNumClockDivs; d++)
    {
        const int lo = z - d;
        const int hi = z + d;
        if (lo >= 0 && (avail_mask & (1u << lo))) return static_cast<uint8_t>(lo);
        if (hi < kNumClockDivs && (avail_mask & (1u << hi))) return static_cast<uint8_t>(hi);
    }
    return 0u;
}

/**
 * Normalised pot value at the centre of zone @p idx.
 * Use to snap the time-pot's stored value when the PLL first acquires lock.
 */
inline float ClockDivNormFromZone(uint8_t idx)
{
    if (idx >= kNumClockDivs) idx = kNumClockDivs - 1u;
    return (static_cast<float>(idx) + 0.5f) / static_cast<float>(kNumClockDivs);
}

/**
 * Find the available zone whose period is closest to @p target_seconds.
 * Used at lock-acquire to snap the pot to the current free-mode delay time.
 * @p avail_mask must be non-zero.
 */
inline uint8_t ClockDivNearestZone(float    target_seconds,
                                   float    quarter_seconds,
                                   uint16_t avail_mask)
{
    uint8_t best     = 0u;
    float   best_err = -1.0f;
    for (uint8_t i = 0; i < kNumClockDivs; i++)
    {
        if (!(avail_mask & static_cast<uint16_t>(1u << i))) continue;
        float err = quarter_seconds * kClockDivMul[i] - target_seconds;
        if (err < 0.0f) err = -err;
        if (best_err < 0.0f || err < best_err) { best_err = err; best = i; }
    }
    return best;
}

} // namespace alchemy
