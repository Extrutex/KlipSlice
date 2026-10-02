# Connecting a printer

KLIPSLICE talks to a printer through Moonraker and nothing else. One address in the
printer settings gives you the Device tab, the status line, filament sync and
printing. This page is the user-facing side of the design in
[`docs/HLSD/moonraker-device.md`](../HLSD/moonraker-device.md).

![KLIPSLICE talks to the printer only through Moonraker's HTTP API](../images/device.svg)

## The address

Open the printer settings and enter the printer's Moonraker address under
**Print host**: the host name or IP, with the port when Moonraker does not answer on
80 (for example `voron.local:7125`). A scheme is optional; `http://` is assumed.

- **Test** checks that Moonraker answers at that address.
- **Browse** lists every Moonraker on the LAN. Moonraker announces itself as
  `_moonraker._tcp` through Bonjour; the instance name is what you gave it in
  `moonraker.conf`.
- **API key** is needed when the slicer's machine is not in Moonraker's
  `trusted_clients`. Create it under *Machine › API keys* in Mainsail or Fluidd.
- **Certificate file** matters only for Moonraker behind HTTPS with a self-signed
  certificate.

On Windows a `.local` name is resolved once through mDNS and the address is reused
for the whole upload; two resolves in a row fail there.

## The Device tab

The Device tab shows the printer's own web interface, Mainsail or Fluidd, exactly as
the browser would. The API key from the printer settings is injected into the
page's requests, so a printer that requires a key works without logging in twice.

Above the page sits one line:

| Shown | Meaning |
|---|---|
| **No printer host set** | The printer preset has no address yet. |
| **Connecting** | The first poll is on its way. |
| **Printer not reachable** | Moonraker did not answer; the error is shown after the address. Polled every 5 s until it does. |
| **Klipper starting**, **Klipper not ready** | Moonraker answers, Klipper does not. The message is Klipper's own (`webhooks.state_message`). |
| **Idle**, **Printing**, **Paused**, **Print finished**, **Print cancelled**, **Print error** | `print_stats.state`, with file name, progress, layer and print time while a print is active. |

Nozzle and bed show as *actual/target ℃*. The line is polled every 2 s while the
printer answers. Everything richer, from heater targets to macros, is the web
interface's job.

## Sync filaments

The sidebar's sync button reads the filament changer the printer actually has.
The changer is picked from `/printer/objects/list`, so nothing in the profile
names it:

| Changer | Read from |
|---|---|
| **AFC**, recent **Happy Hare** | Moonraker database namespace `lane_data`: lane, material, colour, vendor, temperatures |
| **Happy Hare** | The `mmu` printer object: gate status, material, colour, temperature |
| **Qidi box** | `save_variables` slot variables, `box_stepper slotN` runout sensors, and the box's `officiall_filas_list.cfg` |
| **Snapmaker** toolhead | `print_task_config`, with the NFC tag data of `filament_detect` |

Every loaded slot is matched to a filament preset: the vendor's own preset of that
material with the closest colour first, else a visible system preset of the
material. The dialog then asks before it replaces the project's filament list (or,
with the preference *Filament sync mode: Color only*, before it takes the
colours over). Empty slots stay empty
placeholders so the slot numbers (`T0`, `T1`, …) keep matching the printer.

A printer without a known changer answers *The printer reports no filament
changer*; a printer with an empty changer answers *Every slot … is empty*.

## Printing

**Print** uploads the G-code and starts the print; **Send** only uploads. Both go
through `POST /server/files/upload`, and print start through
`POST /printer/print/start`. KLIPSLICE writes layer progress as
`SET_PRINT_STATS_INFO` so Klipper's `print_stats` and the web interface show the
current layer.

## When it does not connect

- **401 or 403**: the slicer's machine is not a trusted client and no API key is
  set, or the key is wrong.
- **Could not resolve host**: the `.local` name is not announced on this network;
  use the IP address.
- **Connection refused**: Moonraker is not running or listens on another port;
  check `moonraker.conf` `[server] port`.
- **Certificate errors**: give the CA file in the printer settings, or use `http://`.
