/**
 * @file clock_follower.cpp
 * @brief External-clock PI-PLL.  See clock_follower.h.
 */

#include "alchemy/control/clock_follower.h"

#include <cassert>

namespace alchemy {

namespace {

inline double Clamp(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

} // namespace

/* ── Source selection ──────────────────────────────────────────────────── */

void ClockFollower::Enable(uint32_t ext_ppqn)
{
    assert(nco_ != nullptr);
    assert(ext_ppqn > 0u);
    assert(nco_->Ppqn() % ext_ppqn == 0u);

    ext_ppqn_          = ext_ppqn;
    enabled_           = true;
    locked_            = false;
    have_last_stamp_   = false;
    valid_pulse_count_ = 0u;
    period_est_us_     = 0.0;
    integrator_        = 0.0;
    loss_announced_    = false;
    pulse_pending_     = false;

    /* Stop the NCO and zero its rate so it does not free-run at the old
     * internal tempo while waiting for first lock (azoth
     * `extclock_enable`).  `SetSteered(true)` makes user `SetBpm` a
     * no-op so it can't fight us. */
    nco_->SetSteered(true);
    nco_->Stop();
    nco_->SetTicksPerUs(0.0);

    RecomputeLimits();
}

void ClockFollower::Disable()
{
    enabled_        = false;
    locked_         = false;
    loss_announced_ = false;
    pulse_pending_  = false;
    if (nco_ != nullptr)
        nco_->SetSteered(false);
}

void ClockFollower::SetExtPpqn(uint32_t ppqn)
{
    assert(ppqn > 0u);
    assert(nco_ != nullptr && nco_->Ppqn() % ppqn == 0u);

    ext_ppqn_          = ppqn;
    locked_            = false;
    have_last_stamp_   = false;
    valid_pulse_count_ = 0u;
    integrator_        = 0.0;
    /* period_est preserved — same wall-clock pulses, just different
     * musical meaning. */
    RecomputeLimits();
}

void ClockFollower::RecomputeLimits()
{
    const double fastest_q_us     = (60.0 / kClockBpmMax) * 1e6;
    const double fastest_pulse_us = fastest_q_us / static_cast<double>(ext_ppqn_);
    uint32_t     min_us           = static_cast<uint32_t>(fastest_pulse_us * 0.25);
    if (min_us < kClockMinPeriodFloorUs) min_us = kClockMinPeriodFloorUs;
    min_period_us_ = min_us;

    const double slowest_q_us     = (60.0 / kClockBpmMin) * 1e6;
    const double slowest_pulse_us = slowest_q_us / static_cast<double>(ext_ppqn_);
    uint32_t     max_us           = static_cast<uint32_t>(slowest_pulse_us * 2.0);
    if (max_us < kClockMaxPeriodFloorUs) max_us = kClockMaxPeriodFloorUs;
    max_period_us_ = max_us;
}

void ClockFollower::SetGains(float kp, float ki, float ema_alpha)
{
    kp_        = kp;
    ki_        = ki;
    ema_alpha_ = ema_alpha;
}

float ClockFollower::Bpm() const
{
    if (period_est_us_ <= 0.0) return 0.0f;
    return static_cast<float>(
        60.0 / (period_est_us_ * static_cast<double>(ext_ppqn_) * 1e-6));
}

void ClockFollower::OnReset()
{
    /* Re-anchor the phase reference: the NCO has just snapped to
     * master_tick=0/frac=0, so anchor there and zero the integrator so
     * we don't immediately apply a corrective burst against the
     * discontinuity. */
    last_pulse_tick_ = 0u;
    last_pulse_frac_ = 0.0;
    integrator_      = 0.0;
}

/* ── Pulse intake ──────────────────────────────────────────────────────── */

void ClockFollower::OnPulse(uint32_t stamp_us)
{
    if (!enabled_) return;
    /* Single-pulse mailbox: stamp first, then pending flag.  Reader
     * (Update) checks pending → clears it → reads stamp.  A concurrent
     * second pulse during Update overwrites stamp; the spec accepts
     * newest-wins (§4.1).  Safe from EXTI ISR on Cortex-M7 (word stores
     * are atomic). */
    pulse_stamp_us_ = stamp_us;
    pulse_pending_  = true;
}

/* ── Per-poll work ─────────────────────────────────────────────────────── */

void ClockFollower::Update(uint32_t now_us)
{
    if (!enabled_ || nco_ == nullptr) return;

    /* ── Loss detection (runs every poll, independent of pulse arrival) */
    if (have_last_stamp_ && period_est_us_ > 0.0 && !loss_announced_)
    {
        const uint32_t dt_since = now_us - last_pulse_stamp_;
        const double   thresh   = period_est_us_ * static_cast<double>(kClockLossFactor);
        if (static_cast<double>(dt_since) > thresh)
            HandleLoss(now_us);
    }

    /* ── Consume a pending pulse, if any */
    if (!pulse_pending_) return;
    pulse_pending_         = false;
    const uint32_t stamp   = pulse_stamp_us_;

    if (!have_last_stamp_)
    {
        last_pulse_stamp_  = stamp;
        have_last_stamp_   = true;
        valid_pulse_count_ = 1u;
        loss_announced_    = false;
        return;
    }

    const uint32_t dt = stamp - last_pulse_stamp_;

    if (dt < min_period_us_)
        return; /* glitch — ignore */

    if (dt > max_period_us_)
    {
        /* Outside the validity window: treat as a restart, not a missed
         * pulse.  Clear state, reseed from this pulse — same as the
         * existing ClockPll behaviour. */
        locked_            = false;
        loss_announced_    = false;
        valid_pulse_count_ = 1u;
        last_pulse_stamp_  = stamp;
        period_est_us_     = 0.0;
        integrator_        = 0.0;
        return;
    }

    /* ── Valid pulse ────────────────────────────────────────────────── */
    const double dt_d = static_cast<double>(dt);
    if (period_est_us_ <= 0.0)
        period_est_us_ = dt_d;
    else
        period_est_us_ += static_cast<double>(ema_alpha_)
                       * (dt_d - period_est_us_);

    last_pulse_stamp_ = stamp;
    ++valid_pulse_count_;
    loss_announced_   = false;

    /* ── Lock acquisition ──────────────────────────────────────────── */
    if (!locked_)
    {
        if (valid_pulse_count_ >= kClockLockPulses)
            DeclareLock();
        return; /* no PI step until locked */
    }

    /* ── PI controller (post-lock) ─────────────────────────────────── */
    const double ticks_per_pulse =
        static_cast<double>(nco_->Ppqn()) / static_cast<double>(ext_ppqn_);

    const uint64_t mtick = nco_->MasterTick();
    const double   frac  = nco_->FracTick();

    /* Wrap-safe even past 2^64 ticks because the unsigned subtraction
     * mods over 2^64; in practice master_tick_ tops out at ~6 trillion
     * years at 96 PPQN / 300 BPM so this is theoretical. */
    const double delta_int  = static_cast<double>(mtick - last_pulse_tick_);
    const double delta_frac = frac - last_pulse_frac_;
    const double delta      = delta_int + delta_frac;
    const double phase_err  = ticks_per_pulse - delta;

    integrator_ += phase_err;
    integrator_  = Clamp(integrator_,
                         -static_cast<double>(kClockPllIntegratorMax),
                          static_cast<double>(kClockPllIntegratorMax));

    const double base_per_sec = (static_cast<double>(nco_->Ppqn()) * 1e6)
                              / (static_cast<double>(ext_ppqn_) * period_est_us_);
    const double correction   = static_cast<double>(kp_) * phase_err
                              + static_cast<double>(ki_) * integrator_;
    double rate_per_sec       = base_per_sec + correction;

    /* Safety clamp on absolute rate.  The PI loop should never demand
     * a tempo beyond `kClockBpmMax`. */
    const double max_per_sec = (static_cast<double>(kClockBpmMax)
                                * static_cast<double>(nco_->Ppqn())) / 60.0;
    rate_per_sec = Clamp(rate_per_sec, 0.0, max_per_sec);

    nco_->SetTicksPerUs(rate_per_sec * 1e-6);

    /* Update anchor for next pulse's phase-error measurement. */
    last_pulse_tick_ = mtick;
    last_pulse_frac_ = frac;
}

/* ── State transitions ─────────────────────────────────────────────────── */

void ClockFollower::DeclareLock()
{
    locked_ = true;

    SeedRateFromPeriod();
    AnchorPhase();
    integrator_ = 0.0;

    nco_->OrPendingEvent(CLK_EV::EXT_LOCK);

    if (auto_start_)
        nco_->Start();
}

void ClockFollower::SeedRateFromPeriod()
{
    if (period_est_us_ <= 0.0)
        return;
    const double rate_per_sec = (static_cast<double>(nco_->Ppqn()) * 1e6)
                              / (static_cast<double>(ext_ppqn_) * period_est_us_);
    nco_->SetTicksPerUs(rate_per_sec * 1e-6);
}

void ClockFollower::AnchorPhase()
{
    last_pulse_tick_ = nco_->MasterTick();
    last_pulse_frac_ = nco_->FracTick();
}

void ClockFollower::HandleLoss(uint32_t /*now_us*/)
{
    /* All policies share: forget the pulse counter and the last-stamp
     * anchor (so the next pulse re-enters lock acquisition), preserve
     * `period_est` for warm re-lock, fire EXT_LOST once. */
    locked_            = false;
    have_last_stamp_   = false;
    valid_pulse_count_ = 0u;
    integrator_        = 0.0;
    loss_announced_    = true;

    nco_->OrPendingEvent(CLK_EV::EXT_LOST);

    switch (loss_policy_)
    {
        case ClockLossPolicy::Freewheel:
            /* Keep last rate; NCO keeps integrating in time. */
            break;
        case ClockLossPolicy::Freeze:
            /* Hold position by zeroing the rate.  Resume on re-lock. */
            nco_->SetTicksPerUs(0.0);
            break;
        case ClockLossPolicy::Stop:
            /* Halt transport.  Consumer may collapse to dry. */
            nco_->Stop();
            break;
    }
}

} // namespace alchemy
