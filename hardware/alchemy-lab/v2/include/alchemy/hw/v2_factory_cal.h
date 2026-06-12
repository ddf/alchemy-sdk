/**
 * @file v2_factory_cal.h
 * @brief Alchemy Lab V2 — on-board factory calibration (SDK-resident).
 *
 * The full self-cal procedure (VREFINT → VDDA, per-jack ADC zero, per-jack
 * DAC loopback sweep with rail-saturation detection) runs on the board
 * itself with nothing patched in, persists a V2Calibration record to the
 * dedicated QSPI sector, then soft-resets into a normal boot.
 *
 * Entry: hold B1 + B2 while the board powers up / resets. The check and
 * the procedure run at the very top of AlchemyLabV2::Init() — before the
 * DMA ADC starts — so the cal's one-shot bare-metal ADC use can't collide
 * with the streaming pot/CV path.
 *
 * Operator feedback is on the 102-LED panel (no USB dependency). Ring k
 * mirrors jack k (ring 0 ↔ J3 … ring 5 ↔ J8):
 *   - B1+B2 pairs white      : button hold acknowledged, cal starting
 *   - button pairs amber     : VREFINT / VDDA measurement
 *   - rings fill blue (wave) : per-jack ADC zero pass
 *   - ring fills amber       : that jack's DAC sweep in progress
 *     (finished jacks hold dim green)
 *   - triple green flash,
 *     then solid green       : success — release the buttons; the board
 *                              resets into normal boot with cal applied
 *   - panel dim red, one
 *     ring blinking bright   : failure — the blinking ring is the jack
 *                              that failed (no blinking ring = failure
 *                              before the sweeps). Power-cycle, retry.
 *                              A previous good record survives failure
 *                              (a torn QSPI write fails CRC on the next
 *                              boot and falls back cleanly).
 *
 * Preconditions: nothing patched into any CV jack (the zero pass measures
 * open-jack idle), board warmed up ~30 s for best results.
 *
 * For bench-grade diagnostics (full USB dump of every sweep point) use
 * the standalone `cal_writer` firmware in alchemy-lab-v2-cal-sanity/.
 */

#pragma once

namespace alchemy {

class AlchemyLabV2;

/** True when B1 and B2 are both held (raw GPIO read with pull-ups —
 *  callable before any board init beyond DaisySeed::Init()). */
bool V2FactoryCalRequested();

/** Run the full cal procedure, persist to QSPI, soft-reset. Never
 *  returns. On unrecoverable failure, traps in a rapid-blink loop. */
[[noreturn]] void V2RunFactoryCalibrationAndReset(AlchemyLabV2& hw);

} // namespace alchemy
