/**
 * @file alchemy/hw/i_button.h
 * @brief alchemy::IButton — abstract interface for a debounced momentary button.
 *
 * Every Alchemy board exposes its buttons through this interface so framework
 * code (Settings, Pager, ParamLock) can hold a reference without caring which
 * concrete driver — on-MCU GPIO, I²C expander, or future variants — is behind
 * it. The contract:
 *
 *   - Pressed()      — current debounced state, idempotent
 *   - RisingEdge()   — consume-on-read rising edge since last call
 *   - FallingEdge()  — consume-on-read falling edge since last call
 *   - TimeHeldMs()   — held time in ms while currently pressed
 *
 * Const qualifiers MUST match the concrete classes verbatim or `override`
 * fails to compile.
 *
 * Lifetime: framework code holds non-owning IButton references obtained from
 * the board class (e.g. `hw.buttons[i]`). The board class owns the storage.
 */

#pragma once

namespace alchemy {

class IButton
{
  public:
    virtual ~IButton() = default;
    virtual bool Pressed() const = 0;
    virtual bool RisingEdge() = 0;
    virtual bool FallingEdge() = 0;
    virtual float TimeHeldMs() const = 0;
};

} // namespace alchemy
