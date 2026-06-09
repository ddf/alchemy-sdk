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
demonstrating how to build a fully featured module by bringing your own
DSP.

### Requirements

TODO: Verify with fresh install.

- `git`
- `cmake` ≥ 3.21
- `ninja`
- `arm-none-eabi-gcc` 13.x
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
cmake -B build-arm -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/stm32h750xx.cmake
cmake --build build-arm
```

### Flash an example

TODO - Verify

Put the module in DFU mode with the Daisy bootloader, reboot and you have two seconds. To extend this, press "RESET" during the "breathing animation" bootloader window. 

```sh
cmake --build build-arm --target stereo_eq-flash
```

Other examples flash the same way, e.g. `kick-flash`.

## Quick template

TODO: see [`alchemy-template`](#) for a project skeleton that vendors
the Alchemy SDK and libDaisy as git submodules.  This is the recommended
starting point for your own module firmware.

## Animation library

The framework ships a set of declarative ring-rendering primitives in
[`alchemy::led`](framework/include/alchemy/led/anims):

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
- Two clock primitives:
  - `ClockPll` — tempo-meter PLL for modules that only want a BPM
    number.
  - `MusicalClock` + `ClockFollower` — free-running NCO timeline plus
    PI-PLL follower for phase-aware, bar-aware, click-free synced
    audio.  Specified in [`docs/clock_spec.md`](docs/clock_spec.md);
    see [`clock_sync`](examples/clock_sync/clock_sync.cpp) for a
    minimal wiring example.

### Planned TODO

- Calibration usage and abstraction.
- Field-programmable Jack CV I/O configuration and example
- SD card support (just note to use Daisy primitives)
- Expansion-header support/pinout documentation
- CV out of codec via DC coupling
- Better QSPI safe read/write helpers (don't step on used regions)
- USB PC connection
- MIDI implementation example

### Bootloader TODO

Add the custom board bootloaders

### Calibration TODO

Note and link to calibration instructions in bootloader.

### Different board versions and build instructions TODO

Explain V1 board, V2 board

## License

The Alchemy SDK is released under the [MIT License](LICENSE) — build, fork,
remix, and ship commercial firmware modules freely.

Bundled dependencies retain their own licenses; notably,
[libDaisy](vendor/libDaisy) is independently MIT-licensed by Electrosmith. We love Electrosmith!
