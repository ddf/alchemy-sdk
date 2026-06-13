/**
 * kick.cpp — Alchemy Lab trigger-driven kick drum.
 *
 * Single-page percussion instrument: pitch-swept sine body, hard-clip
 * saturation, switchable one-shot transient layer, looping automation per
 * pot, and a live envelope meter overdrawn on the volume ring.
 *
 * SDK features OPTED IN:
 *   - ControlLoop                 frame timing + render bracket
 *   - VirtualKnob                 six declarative knobs with per-knob
 *                                 transforms, ring styles, and pip overlays
 *   - Page                        single Page(0) holding all six knobs
 *   - ParamLock<6> (unpaged)      hold-B1-and-nudge to record automation
 *   - .Overdraw(fn)               envelope meter painted on top of P6
 *   - Button::RisingEdge()        latched B2 edge read from audio callback
 *
 * SDK features NOT used:
 *   - Pager                       single page, B1 owned by ParamLock
 *   - Settings                    no settings page
 *   - Presets                     no persistence
 *   - KnobStorage                 VirtualKnob reads phys[] directly
 *   - CV inputs                   no pot routed to CV
 *
 * Controls:
 *   P1 — Trigger pitch         (level fill, orange + Pulse)
 *   P2 — Sweep speed           (level fill, magenta + Ripple)
 *   P3 — Transient zone        (4-zone selector: none / click / tick / knock)
 *   P4 — Drive                 (bipolar fill, lime → cyan)
 *   P5 — Decay                 (level fill, violet; ThresholdSnap pip at 0.5)
 *   P6 — Output volume         (level fill, dim grey; envelope meter on top)
 *   B1 — Hold + nudge a pot to record a ParamLock loop on that pot
 *   B2 — Trigger the kick (rising edge)
 */

#include "daisy_seed.h"
#include "alchemy/hw/alchemy_lab.h"
#include "alchemy/led/animations.h"
#include "alchemy/surface/control_loop.h"
#include "alchemy/surface/page.h"
#include "alchemy/surface/param_lock.h"
#include "alchemy/surface/virtual_knob.h"

#include "kick_dsp.h"

using namespace alchemy;

static constexpr uint8_t kTriggerBtn = kButtonB2;
static volatile float env_meter      = 0.f;

/* Knobs */

static VirtualKnob pitch     = VirtualKnob(0, "Pitch")
    .Ring(Level({0xFF, 0x40, 0x00}, FillAnim::Pulse));

static VirtualKnob sweep     = VirtualKnob(1, "Sweep")
    .Ring(Level({0xFF, 0x00, 0x80}, FillAnim::Ripple));

static VirtualKnob transient = VirtualKnob(2, "Transient")
    .Selector(kick_dsp::kTransientCount)
    .Ring(SelectorRing({0xFF, 0x80, 0x00},
                       {0x18, 0x10, 0x08},
                       kick_dsp::kTransientCount,
                       ZoneGeometry::Region));

static VirtualKnob drive     = VirtualKnob(3, "Drive")
    .Ring(Bipolar({0xC0, 0xFF, 0x00},
                  {0x00, 0xC0, 0xFF},
                  {0x80, 0x80, 0x80}));

static VirtualKnob decay     = VirtualKnob(4, "Decay")
    .Ring(Level({0x80, 0x00, 0xFF}))
    .Pip(ThresholdSnapPip({0xFF, 0xFF, 0xFF}, 0.45f, 0.55f, {0xFF, 0x00, 0x00}));

/* Volume's base ring is dim; the envelope meter overdraws in green /
 * yellow / red on top. */
static void DrawVolumeMeter(LedPanel&          panel,
                            uint8_t            pot,
                            const ArcGeometry& geo,
                            float              /*norm*/,
                            uint32_t           /*t_ms*/,
                            void*              ctx)
{
    const float env = *static_cast<volatile float*>(ctx);
    DrawLevelArc(panel, pot, geo, env,
                 {0.5f, 0.85f},
                 {0x00, 0xC0, 0x00},   /* green  — body                  */
                 {0xC0, 0xC0, 0x00},   /* yellow — approaching clip      */
                 {0xFF, 0x00, 0x00});  /* red    — clipping              */
}

static VirtualKnob volume    = VirtualKnob(5, "Volume")
    .Ring(Level({0x40, 0x40, 0x40}))
    .Overdraw(DrawVolumeMeter, const_cast<float*>(&env_meter));

static Page main_page = Page(0).Knobs(pitch, sweep, 
                                      transient, drive,
                                      decay, volume);

static AlchemyLab       hw;
static ControlLoop      loop  (hw);
static ParamLock<6>     locks (hw.buttons[0]);   /* unpaged, 6 slots */

/* ── Glue: knob values → DSP each audio block ────────────────────────── */
static kick_dsp::Params CurrentParams()
{
    return {
        pitch.Norm(),
        sweep.Norm(),
        static_cast<int>(transient.Value()), 
        drive.Norm(),
        decay.Norm(),
        volume.Norm(),
    };
}

static void AudioCallback(daisy::AudioHandle::InputBuffer  /*in*/,
                          daisy::AudioHandle::OutputBuffer out, size_t n)
{
    const bool trig = hw.buttons[kTriggerBtn].RisingEdge();
    env_meter = kick_dsp::Process(CurrentParams(), trig, out, n, hw.SampleRate());
}

int main()
{
    hw.Init();
    hw.StartAudio(AudioCallback);

    loop.Use(locks)
        .Use(main_page);

    for (;;) loop.Tick();
}
