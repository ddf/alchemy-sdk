/**
 * @file musical_clock.cpp
 * @brief Free-running musical NCO timeline.  See musical_clock.h.
 */

#include "alchemy/control/musical_clock.h"

#include <cassert>

namespace alchemy {

namespace {

/* `master_tick_ % period == 0` with a zero-period sentinel meaning
 * "not representable at this PPQN — never fires". */
inline uint32_t Fire(uint64_t master_tick, uint32_t period, uint32_t flag)
{
    if (period == 0u) return 0u;
    return (master_tick % period == 0u) ? flag : 0u;
}

} // namespace

MusicalClock::MusicalClock(uint32_t ppqn, uint8_t beats_per_bar)
{
    /* PPQN must be a positive multiple of 24 so every straight, triplet,
     * dotted, and 24-PPQN MIDI-clock subdivision lands on exact integer
     * ticks. */
    assert(ppqn > 0u && (ppqn % 24u) == 0u);
    assert(beats_per_bar > 0u);

    ppqn_          = ppqn;
    beats_per_bar_ = beats_per_bar;
    RecomputePeriods();
}

void MusicalClock::RecomputePeriods()
{
    /* Straight: power-of-two divisions of the quarter.  ppqn=24 cannot
     * represent 64ths exactly; signal that with 0. */
    p_64th_     = (ppqn_ % 16u == 0u) ? ppqn_ / 16u : 0u;
    p_32nd_     = (ppqn_ %  8u == 0u) ? ppqn_ /  8u : 0u;
    p_16th_     = (ppqn_ %  4u == 0u) ? ppqn_ /  4u : 0u;
    p_8th_      = (ppqn_ %  2u == 0u) ? ppqn_ /  2u : 0u;
    p_quarter_  = ppqn_;
    p_half_     = ppqn_ * 2u;
    p_whole_    = ppqn_ * 4u;
    p_bar_      = ppqn_ * static_cast<uint32_t>(beats_per_bar_);
    p_2bar_     = p_bar_ * 2u;
    p_4bar_     = p_bar_ * 4u;
    p_8bar_     = p_bar_ * 8u;

    /* Triplets: three notes per straight subdivision.  ppqn must be
     * divisible by 3 for triplets to be exact, which any multiple of 24
     * satisfies. */
    p_32nd_t_    = (ppqn_ % 12u == 0u) ? ppqn_ / 12u : 0u;
    p_16th_t_    = (ppqn_ %  6u == 0u) ? ppqn_ /  6u : 0u;
    p_8th_t_     = (ppqn_ %  3u == 0u) ? ppqn_ /  3u : 0u;
    p_quarter_t_ = ((2u * ppqn_) % 3u == 0u) ? (2u * ppqn_) / 3u : 0u;
    p_half_t_    = ((4u * ppqn_) % 3u == 0u) ? (4u * ppqn_) / 3u : 0u;
    p_whole_t_   = ((8u * ppqn_) % 3u == 0u) ? (8u * ppqn_) / 3u : 0u;

    /* Dotted: 1.5× the straight subdivision. */
    p_dotted_16th_  = ((3u * ppqn_) % 8u == 0u) ? (3u * ppqn_) / 8u : 0u;
    p_dotted_8th_   = ((3u * ppqn_) % 4u == 0u) ? (3u * ppqn_) / 4u : 0u;
    p_dotted_qtr_   = ((3u * ppqn_) % 2u == 0u) ? (3u * ppqn_) / 2u : 0u;
    p_dotted_half_  = 3u * ppqn_;
    p_dotted_whole_ = 6u * ppqn_;

    /* 24 PPQN MIDI-clock grid.  Guaranteed integer by the ppqn%24 invariant. */
    p_midi_clock_ = ppqn_ / kClockExtPpqnDefault;
}

void MusicalClock::Start() { pending_start_ = true; }
void MusicalClock::Stop()  { pending_stop_  = true; }
void MusicalClock::Reset() { pending_reset_ = true; }

void MusicalClock::SetBpm(float bpm)
{
    /* Silently no-op while an external follower is steering — the user's
     * `SetBpm` call would otherwise fight the PI loop.  See spec §5. */
    if (steered_) return;
    if (bpm < kClockBpmMin) bpm = kClockBpmMin;
    if (bpm > kClockBpmMax) bpm = kClockBpmMax;
    ticks_per_us_ = (static_cast<double>(bpm) * static_cast<double>(ppqn_))
                  / (60.0 * 1e6);
}

void MusicalClock::SetTimeSignature(uint8_t beats_per_bar)
{
    assert(beats_per_bar > 0u);
    beats_per_bar_ = beats_per_bar;
    RecomputePeriods();
}

void MusicalClock::SetTicksPerUs(double tpu)
{
    if (tpu < 0.0) tpu = 0.0;
    ticks_per_us_ = tpu;
}

float MusicalClock::BeatPhase() const
{
    if (p_quarter_ == 0u) return 0.0f;
    const double t = static_cast<double>(tick_in_beat_) + frac_tick_;
    return static_cast<float>(t / static_cast<double>(p_quarter_));
}

float MusicalClock::BarPhase() const
{
    if (p_bar_ == 0u) return 0.0f;
    const uint64_t into_bar_int = master_tick_ % p_bar_;
    const double   t            = static_cast<double>(into_bar_int) + frac_tick_;
    return static_cast<float>(t / static_cast<double>(p_bar_));
}

float MusicalClock::Bpm() const
{
    if (ticks_per_us_ <= 0.0 || ppqn_ == 0u) return 0.0f;
    return static_cast<float>((ticks_per_us_ * 60.0 * 1e6)
                              / static_cast<double>(ppqn_));
}

void MusicalClock::ApplyDeferredTransport()
{
    if (pending_reset_)
    {
        master_tick_  = 0u;
        frac_tick_    = 0.0;
        bar_          = 0u;
        beat_in_bar_  = 0u;
        tick_in_beat_ = 0u;
        events_      |= CLK_EV::RESET;
        FireDownbeatEvents();
        pending_reset_ = false;
    }
    if (pending_stop_)
    {
        running_      = false;
        events_      |= CLK_EV::STOP;
        pending_stop_ = false;
    }
    if (pending_start_)
    {
        running_       = true;
        events_       |= CLK_EV::START;
        /* Force the next dt to start from now, not from a stale stamp
         * captured before the user pressed Start. */
        have_last_us_  = false;
        pending_start_ = false;
    }
}

void MusicalClock::FireDownbeatEvents()
{
    /* All periodic flags whose period divides 0 fire at master_tick=0.
     * Reusing AdvanceOnePpqnTick keeps the logic in one place. */
    AdvanceOnePpqnTick();
}

void MusicalClock::Tick(uint32_t now_us)
{
    /* Each Tick is one event-batch.  External flags injected by the
     * follower since the previous Tick coalesce with this batch's
     * subdivision flags. */
    events_ = pending_ext_events_;
    pending_ext_events_ = 0u;

    ApplyDeferredTransport();

    if (!running_)
    {
        last_us_      = now_us;
        have_last_us_ = true;
        return;
    }

    if (!have_last_us_)
    {
        last_us_      = now_us;
        have_last_us_ = true;
        return;
    }

    /* Wrap-safe unsigned subtraction.  Caller's `now_us` is from the
     * platform monotonic counter; the implicit modulo-2^32 difference
     * recovers the true dt for any gap < ~71 minutes. */
    uint32_t dt = now_us - last_us_;
    last_us_    = now_us;

    /* A long stall (scheduler hiccup, settings menu) would otherwise
     * dump a flood of catch-up ticks the moment we resume.  Clamp. */
    if (dt > kClockMaxTickDtUs) dt = kClockMaxTickDtUs;

    frac_tick_ += ticks_per_us_ * static_cast<double>(dt);

    while (frac_tick_ >= 1.0)
    {
        frac_tick_ -= 1.0;
        ++master_tick_;
        AdvanceOnePpqnTick();
    }
}

void MusicalClock::AdvanceOnePpqnTick()
{
    /* Position: derived from master_tick_ each call so any path that
     * mutates master_tick_ (Reset, sub-tick loop) stays consistent. */
    if (p_quarter_ > 0u)
    {
        tick_in_beat_ = static_cast<uint32_t>(master_tick_ % p_quarter_);
    }
    if (p_bar_ > 0u)
    {
        beat_in_bar_ = static_cast<uint32_t>(
            (master_tick_ / p_quarter_) % static_cast<uint64_t>(beats_per_bar_));
        bar_ = static_cast<uint32_t>(master_tick_ / p_bar_);
    }

    /* OR in every subdivision whose period divides the current tick. */
    events_ |= Fire(master_tick_, p_64th_,         CLK_EV::SIXTYFOURTH);
    events_ |= Fire(master_tick_, p_32nd_,         CLK_EV::THIRTYSECOND);
    events_ |= Fire(master_tick_, p_16th_,         CLK_EV::SIXTEENTH);
    events_ |= Fire(master_tick_, p_8th_,          CLK_EV::EIGHTH);
    events_ |= Fire(master_tick_, p_quarter_,      CLK_EV::QUARTER);
    events_ |= Fire(master_tick_, p_half_,         CLK_EV::HALF);
    events_ |= Fire(master_tick_, p_whole_,        CLK_EV::WHOLE);
    events_ |= Fire(master_tick_, p_bar_,          CLK_EV::BAR);
    events_ |= Fire(master_tick_, p_2bar_,         CLK_EV::TWO_BAR);
    events_ |= Fire(master_tick_, p_4bar_,         CLK_EV::FOUR_BAR);
    events_ |= Fire(master_tick_, p_8bar_,         CLK_EV::EIGHT_BAR);

    events_ |= Fire(master_tick_, p_32nd_t_,       CLK_EV::THIRTYSECOND_T);
    events_ |= Fire(master_tick_, p_16th_t_,       CLK_EV::SIXTEENTH_T);
    events_ |= Fire(master_tick_, p_8th_t_,        CLK_EV::EIGHTH_T);
    events_ |= Fire(master_tick_, p_quarter_t_,    CLK_EV::QUARTER_T);
    events_ |= Fire(master_tick_, p_half_t_,       CLK_EV::HALF_T);
    events_ |= Fire(master_tick_, p_whole_t_,      CLK_EV::WHOLE_T);

    events_ |= Fire(master_tick_, p_dotted_16th_,  CLK_EV::DOTTED_16TH);
    events_ |= Fire(master_tick_, p_dotted_8th_,   CLK_EV::DOTTED_8TH);
    events_ |= Fire(master_tick_, p_dotted_qtr_,   CLK_EV::DOTTED_QUARTER);
    events_ |= Fire(master_tick_, p_dotted_half_,  CLK_EV::DOTTED_HALF);
    events_ |= Fire(master_tick_, p_dotted_whole_, CLK_EV::DOTTED_WHOLE);

    events_ |= Fire(master_tick_, p_midi_clock_,   CLK_EV::MIDI_CLOCK);
}

} // namespace alchemy
