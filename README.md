# alchemy-sdk

The Alchemy SDK is two code surfaces:

1. **A minimal hardware BSP (board support package)** for every Hermetic
   Modular hardware release.  Pin definitions, LED chain layout, and
   low-level drivers, conforming to the standards set out by the
   Electrosmith Daisy ecosystem.  Build from primitives and have full
   access to the interface board and Daisy DSP board at the core.

2. **A set of utilities under Framework** that unlock advanced features
   on Hermetic Modular hardware.  Declaratively leverage LED animations,
   field-programmable jacks, and conventions like Pot Catch, Parameter
   Lock, and settings pages — the same UX you would find on Hermetic
   Modular's formal hardware releases like [Echoa](https://hermeticmodular.com/modules/alchemy-lab/echoa) and
   [Spagyros](https://hermeticmodular.com/modules/alchemy-lab/spagyros).

Either build from scratch with the hardware BSP, or bring your own DSP
and get powerful out-of-the-box features with the Alchemy Framework, or
mix and match as you desire.

Supported boards:

- Hermetic Modular [Alchemy Lab](https://hermeticmodular.com/modules/alchemy-lab)
- Hermetic Modular [Azoth](https://hermeticmodular.com/modules/azoth) — coming soon

TODO: hardware images.

## Community discussion

Please get involved on [Discord](https://discord.gg/FEEEQFdd2G), provide feedback, and help us improve.

## Beta notice

The Alchemy SDK is in beta.  APIs, surface names, and on-disk preset
formats may change before the first stable release. I (Luke at Hermetic Modular) look forward to your feedback, and want to make this SDK the best possible for your needs. Please file issues on GitHub or Discord.

## Quickstart

[`stereo_eq.cpp`](examples/stereo_eq/stereo_eq.cpp) is an excellent
place to start: it opts into most Alchemy Framework features,
demonstrating how to build a fully featured module by bringing your own DSP.

The below build and requirements work with this repo, and the examples within. For a typical project, you'd clone and use the quick template below. 

> ➡️ [Go to the Quick Template](#quick-template)

### Requirements

- `git`
- `cmake` ≥ 3.21
- `ninja`
- `arm-none-eabi-gcc` ≥ 12
- `dfu-util`

#### Install the toolchain

Ubuntu / Debian:

```sh
sudo apt install git cmake ninja-build gcc-arm-none-eabi dfu-util
```

macOS (Homebrew):

```sh
brew install git cmake ninja dfu-util
brew install --cask gcc-arm-embedded
```

### Clone and build

```sh
git clone --recurse-submodules https://github.com/hermetic-modular/alchemy-sdk.git
cd alchemy-sdk
cmake --preset arm
cmake --build --preset arm
```

### Flash an example

Connect the front USB-C port and put the module in update mode: press
**B3** during the ~2 s bootloader window after power-on (rings
spin a warm-white comet), or simply hold while powering on.
The rings switch to a slow breathe and the
module stays in DFU mode until it's flashed or reset. Then:

```sh
cmake --build --preset arm --target stereo_eq-flash
```

Flashing also works without the button press if `dfu-util` starts
inside the 2 s window.

Other examples flash the same way, e.g. `kick-flash`, `clock_sync-flash`,
`v2_cal_test-flash`, `v2_cv_demo-flash`.

## Quick template

See [`alchemy-template`](https://github.com/hermetic-modular/alchemy-template)
for a project skeleton that vendors the Alchemy SDK and libDaisy as git
submodules.  This is the recommended starting point for your own module
firmware.

## Animation library

The framework ships a set of declarative ring-rendering primitives in
[`alchemy::led`](framework/include/alchemy/led/anims)

## Features

### Done

- A bunch of pot animations — Pulse, Ripple, level meter, color morph,
  selector
- Param lock — looping automation, per-pot
- Pagination and pot catch
- The settings gesture and comprehensive settings experience
- The preset saving and loading UX
- Flash management with wear levelling — declare a data model to save
- CV input, summed with pot position and parameter lock at the
  composition site
- Two clock primitives,`ClockPll` and `MusicalClock` + `ClockFollower`
- Self DAC/ADC calibration. See [Calibration](#calibration-v2-boards)
- Custom board bootloader (panel LED animations + `Alchemy Lab` USB name)
- Flash firmware via front USB-C + button (B3 latch update mode)

### Planned TODO

- Field-programmable Jack CV I/O configuration and example
- Expansion-header support/pinout documentation
- CV out of codec via DC coupling

Post release scope:
- Better QSPI safe read/write helpers (don't step on used regions)
- USB PC connection
- MIDI implementation example

## Bootloader Information

Alchemy Lab V2 boards run a board-specific fork of the Daisy bootloader
(`DaisyBootloader-AlchemyLabV2`). It serves DFU on the front-panel
USB-C port, renders
bootloader state on the panel, and latches into update mode
when B1 or B2 is pressed during the boot window.
Apps built with this SDK can also enter update mode programmatically:

```cpp
daisy::System::ResetToBootloader(
    daisy::System::BootloaderMode::DAISY_INFINITE_TIMEOUT);
```

This could be used to support multiple firmwares in flash at once without requiring a reflash as a future feature.

## Calibration (V2 boards)

Every V2 board self-calibrates its six CV jacks — DAC out and ADC in —
with no external equipment, using the board's built-in DAC→jack→ADC
loopback and the STM32's factory voltage reference.

**To calibrate:** unpatch all CV jacks, then hold **B1 + B2** while the
board resets.  The LED panel narrates the ~15 s procedure (rings fill
as each jack is swept), flashes green on success, and the board reboots
calibrated.  The result is a 120-byte record in a dedicated QSPI sector
that **survives firmware reflashes** — calibrate once per board, every
SDK firmware picks it up automatically.

**Using it** is invisible: `hw.SetCvOutVolts(jack, volts)` and
`hw.cv[jack].Volts()` apply the calibration transparently, and fall
back to design-nominal scaling when no record exists
(`hw.IsCalibrated()` tells you which).  See
[`v2_calibration.h`](hardware/alchemy-lab/v2/include/alchemy/hw/v2_calibration.h)
and [`v2_factory_cal.h`](hardware/alchemy-lab/v2/include/alchemy/hw/v2_factory_cal.h)
for the details, and
[`examples/v2_cal_test`](examples/v2_cal_test/v2_cal_test.cpp) for a
scope-driven acceptance test (step −5..+5 V, toggle calibration live).

## Board versions

Two Alchemy Lab board revisions are supported, selected by preset:

```sh
cmake --preset arm      # V2 (the default)
cmake --preset arm-v1   # V1
```

(Equivalent to setting `-DALCHEMY_BOARD=v2|v1` by hand.)

- **v1** — original dev board. You probably don't have one of these unless you were an alpha tester.
- **v2** — The standard production board shipped by Hermetic Modular.

V2-only examples (`v2_cal_test`, `v2_cv_demo`) build only when
`ALCHEMY_BOARD=v2`. V1 doesn't have CV out.

## License

The Alchemy SDK is released under the [MIT License](LICENSE) — build, fork,
remix, and ship commercial firmware modules freely.

Bundled dependencies retain their own licenses; notably,
[libDaisy](vendor/libDaisy) is independently MIT-licensed by Electrosmith. We love Electrosmith!
