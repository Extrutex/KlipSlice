# KLIPSLICE documentation

The user-facing documentation lives in the wiki at
<https://extrutex.github.io/KlipSlice-Wiki/> (English and German). This directory holds
what belongs next to the code: the pages the README links to, the design of each
subsystem, and the figures.

## For users of KLIPSLICE

| Page | What it answers |
|---|---|
| [`klipslice/printer-connection.md`](klipslice/printer-connection.md) | How a printer is connected through Moonraker: the address, the Device tab's status line, filament sync per changer, printing, the usual connection errors. |
| [`klipslice/kinematics.md`](klipslice/kinematics.md) | What each Klipper `kinematics` moves, which motors share a move, and which limit in `printer.cfg` a toolpath is bounded by. |
| [`klipslice/printer-firmware.json`](klipslice/printer-firmware.json) | The evidence behind every shipped printer profile: stock firmware, Moonraker, mod project, sources and verdict per model. `scripts/klipslice_profile_guard.py` keeps it in step with the profiles. |

## For people changing the code

High-level subsystem designs are in [`HLSD/`](HLSD/), one page per subsystem,
written as the design stands:

| Page | Subsystem |
|---|---|
| [`HLSD/moonraker-device.md`](HLSD/moonraker-device.md) | The printer side: Device tab, status strip, printing, filament sync, and what the Moonraker-only design rules out. |
| [`HLSD/filament_id.md`](HLSD/filament_id.md) | How filament presets are identified, and how a slot of the printer's changer is resolved to one. |
| [`HLSD/preset-cache.md`](HLSD/preset-cache.md) | The system preset cache that shortens start-up. |
| [`HLSD/deferred-page-construction.md`](HLSD/deferred-page-construction.md) | Lazy pages and the idle prebuild of the main window. |
| [`HLSD/keyboard-shortcuts.md`](HLSD/keyboard-shortcuts.md) | The shortcut table and how it is rendered. |
| [`HLSD/precise-seam.md`](HLSD/precise-seam.md), [`HLSD/wipe-inward.md`](HLSD/wipe-inward.md), [`HLSD/multiline-infill.md`](HLSD/multiline-infill.md), [`HLSD/prime-tower-sparse-layers.md`](HLSD/prime-tower-sparse-layers.md) | Slicing features and the constraints that shape them. |

[`design/`](design/) holds the brand and design foundation (tokens, type, colour) and
[`CAD/`](CAD/) the notes on the Design tab. [`images/`](images/) holds every figure the
README and these pages use, hand-authored SVG on the same dark ground with the same
type and signal colours as the application.

Planning and investigation notes are not kept in the repository.
