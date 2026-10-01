# KLIPSLICE

**A slicer for Klipper printers, and only for Klipper printers.**

KLIPSLICE is an open-source 3D printing slicer built exclusively for printers that
run real [Klipper](https://www.klipper3d.org/). It is a hard fork of OrcaSlicer with
every path that does not lead to a Klipper machine removed.

**Documentation:** <https://extrutex.github.io/KlipSlice-Wiki/> (English and German)

> KLIPSLICE is an independent community project and is not affiliated with or
> endorsed by the Klipper project.

## What KLIPSLICE is

- **Klipper only.** The G-code flavor is always `klipper`. Older projects and
  presets with another flavor are converted when they load.
- **Moonraker is the only print host.** G-code upload and print start go through
  [Moonraker](https://moonraker.readthedocs.io/). There are no vendor-specific
  printer protocols.
- **No vendor network, no cloud.** The vendor network plugin and vendor cloud
  services are not part of KLIPSLICE; removing the inherited code is ongoing work.
- **Profiles only for printers that verifiably run Klipper.** 174 printer models
  ship with a system profile, each one backed by per-model evidence (stock
  Klipper, Klipper via an established community mod, or a self-built Klipper
  machine) in
  [`docs/klipslice/printer-firmware.json`](docs/klipslice/printer-firmware.json).
- **Your own printer is first-class.** Self-built and converted machines (for
  example a Marlin printer converted to Klipper) are set up with the
  **Generic Klipper Printer** profile or through the **Create printer** dialog.
  The flavor is always Klipper, the host always Moonraker.
- **Side by side with OrcaSlicer.** KLIPSLICE uses its own binaries and its own
  data directory and never reads or changes an installed OrcaSlicer.

## Vision

KLIPSLICE aims at machine-aware slicing: a slicer that knows the machine it
slices for. The following is **roadmap, not implemented yet**:

- **Machine sync.** Read the live Klipper configuration through Moonraker
  (`max_accel`, `max_velocity`, `square_corner_velocity`,
  `minimum_cruise_ratio`, input shaper) and plan against the real limits of the
  connected printer instead of values typed into a profile.
- **Klipper-accurate print time.** Estimate print time with Klipper's own motion
  model instead of a generic approximation.
- **Native adaptive mesh.** Probe only the area that is actually printed, without
  separate macro packages.

## Status

**Pre-alpha.** There are no releases and no installers yet. Settings, profiles
and file formats can still change without migration. Do not rely on KLIPSLICE for
prints that matter.

CI builds KLIPSLICE for Windows, macOS and Linux on every push.

## Build from source

Every platform builds in two steps: first the dependencies (`deps/`, built once
and reused), then the slicer. The dependency build takes a long time and needs
several gigabytes of disk space. Run any build script with `-h` for all options.

```bash
git clone https://github.com/Extrutex/KlipSlice.git
cd KlipSlice
```

### Windows

Requires Visual Studio 2019, 2022 or 2026, CMake, Perl and Git.

```bat
build_win.bat -d
build_win.bat -s
```

Output: `build\src\Release\klipslice.exe`

### macOS

Requires Xcode or the Command Line Tools, CMake 3.31 and gettext.
CMake 4.x currently breaks the dependency build; use CMake 3.31.

With the Command Line Tools only (no full Xcode), export the SDK path first:

```bash
export SDKROOT=$(xcrun --show-sdk-path)
```

Then build the dependencies and the slicer with the Ninja generator:

```bash
./build_release_macos.sh -dx
./build_release_macos.sh -sx
```

Output: `build/<arch>/OrcaSlicer/KLIPSLICE.app` (`<arch>` is `arm64` or `x86_64`)

### Linux

Requires more than 10 GiB of available memory and free disk space.

```bash
./build_linux.sh -u      # install system dependencies (asks for sudo)
./build_linux.sh -dsi    # dependencies, slicer and AppImage
```

Output: `build/package/bin/klipslice` and `build/KLIPSLICE_Linux_V<version>.AppImage`

## Credits and license

KLIPSLICE is based on [OrcaSlicer](https://github.com/OrcaSlicer/OrcaSlicer) by
SoftFever and the OrcaSlicer contributors. OrcaSlicer is based on
[Bambu Studio](https://github.com/bambulab/BambuStudio) by Bambu Lab, which is
based on [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research,
which comes from [Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro
Ranellucci and the RepRap community. KLIPSLICE would not exist without their work.

KLIPSLICE is licensed under the **GNU Affero General Public License, version 3**.
See [LICENSE.txt](LICENSE.txt). Anyone who distributes KLIPSLICE, modified or not,
must make the corresponding source code available under the same terms.
