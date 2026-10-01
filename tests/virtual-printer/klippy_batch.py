#!/usr/bin/env python3
"""Run a G-code file through upstream Klippy in batch mode and judge the result.

Klippy runs unmodified (klippy.py <printer.cfg> -i <gcode> -o /dev/null -d <dict>).
This wrapper only adds observation around it, in the same process:

- every log record goes to a log file with its level, so errors and warnings can
  be told apart from informational output (batch mode logs without levels);
- every G-code command object records which parameters its handler read, so
  parameters Klipper silently ignores (e.g. M73 R, ACCEL_TO_DECEL once
  MINIMUM_CRUISE_RATIO is given) are reported instead of disappearing;
- the toolhead print time is sampled at the first EXCLUDE_OBJECT_START and at
  every full move flush (M400 and friends), giving Klipper's own motion time
  for the printed part of the file.

The verdict is written as JSON. Exit status: 0 accepted, 1 rejected, 2 usage.
"""

import argparse
import collections
import json
import logging
import os
import re
import sys

KLIPPY_DIR = "/opt/klipper/klippy"

# Parameters whose value Klipper hands to the handler as a whole are marked
# with this key; such a command never reports ignored parameters.
ALL_PARAMS = "*"
# Parameters Klipper still accepts but has deprecated, for the pinned release
# (Klipper docs/Config_Changes.md). Klipper logs nothing for them at runtime.
DEPRECATED_PARAMS = {
    ("SET_VELOCITY_LIMIT", "ACCEL_TO_DECEL"):
        "deprecated 2024-03-13 (Config_Changes.md); use MINIMUM_CRUISE_RATIO",
}
TRADITIONAL_RE = re.compile(r"^[A-Z]\d+(\.\d+)?$")


class RecordCollector(logging.Handler):
    """Keeps WARNING+ records and the INFO records that matter for the verdict."""

    UNKNOWN_RE = re.compile(r'^Unknown command:"(?P<cmd>[^"]*)"')

    def __init__(self):
        super().__init__(level=logging.INFO)
        self.errors = []
        self.warnings = []
        self.unknown = collections.Counter()

    def emit(self, record):
        msg = record.getMessage()
        match = self.UNKNOWN_RE.match(msg)
        if match is not None:
            self.unknown[match.group("cmd")] += 1
        elif record.levelno >= logging.ERROR:
            self.errors.append(msg)
        elif record.levelno >= logging.WARNING:
            self.warnings.append(msg)


class Observer:
    """Instruments gcode.GCodeCommand and toolhead.ToolHead without changing results."""

    def __init__(self):
        self.commands = []
        self.gcode_errors = []
        self.dispatch = None
        # Toolhead print time at the first two EXCLUDE_OBJECT_START commands,
        # i.e. the start of the first and of the second layer of a
        # single-object print.
        self.object_starts = []
        self.print_time_last_flush = None

    def install(self):
        import gcode
        import toolhead

        observer = self
        orig_cmd_init = gcode.GCodeCommand.__init__
        orig_get = gcode.GCodeCommand.get
        orig_get_params = gcode.GCodeCommand.get_command_parameters
        orig_get_raw = gcode.GCodeCommand.get_raw_command_parameters
        orig_dispatch_init = gcode.GCodeDispatch.__init__
        orig_extended = gcode.GCodeDispatch._get_extended_params
        orig_respond_error = gcode.GCodeDispatch._respond_error
        orig_wait_moves = toolhead.ToolHead.wait_moves

        def cmd_init(self, gcode_obj, command, commandline, params, need_ack):
            orig_cmd_init(self, gcode_obj, command, commandline, params, need_ack)
            self._vp_read = set()
            observer.commands.append(self)
            if command == "EXCLUDE_OBJECT_START" and len(observer.object_starts) < 2:
                th = observer.lookup_toolhead()
                if th is not None:
                    observer.object_starts.append(th.print_time)

        def get(self, name, *args, **kwargs):
            self._vp_read.add(name)
            return orig_get(self, name, *args, **kwargs)

        def get_command_parameters(self):
            self._vp_read.add(ALL_PARAMS)
            return orig_get_params(self)

        def get_raw_command_parameters(self):
            self._vp_read.add(ALL_PARAMS)
            return orig_get_raw(self)

        def dispatch_init(self, printer):
            orig_dispatch_init(self, printer)
            observer.dispatch = self

        def get_extended_params(self, gcmd):
            # Parsing NAME=VALUE parameters reads the raw line; that is not the
            # handler consuming them.
            result = orig_extended(self, gcmd)
            gcmd._vp_read.discard(ALL_PARAMS)
            return result

        def respond_error(self, msg):
            observer.gcode_errors.append(msg.strip())
            orig_respond_error(self, msg)

        def wait_moves(self):
            orig_wait_moves(self)
            observer.print_time_last_flush = self.print_time

        gcode.GCodeCommand.__init__ = cmd_init
        gcode.GCodeCommand.get = get
        gcode.GCodeCommand.get_command_parameters = get_command_parameters
        gcode.GCodeCommand.get_raw_command_parameters = get_raw_command_parameters
        gcode.GCodeDispatch.__init__ = dispatch_init
        gcode.GCodeDispatch._get_extended_params = get_extended_params
        gcode.GCodeDispatch._respond_error = respond_error
        toolhead.ToolHead.wait_moves = wait_moves

    def lookup_toolhead(self):
        if self.dispatch is None:
            return None
        return self.dispatch.printer.lookup_object("toolhead", None)

    def command_census(self, unknown):
        """Per command: how often it ran and which given parameters were never read."""
        census = {}
        for gcmd in self.commands:
            name = gcmd.get_command()
            if not name:
                continue
            entry = census.setdefault(name, {"count": 0, "params": collections.Counter(),
                                             "ignored": collections.Counter()})
            entry["count"] += 1
            given = set(gcmd._params.keys())
            if TRADITIONAL_RE.match(name):
                # Traditional commands carry their own letter as a parameter.
                given.discard(name[0])
            entry["params"].update(given)
            if ALL_PARAMS not in gcmd._vp_read and name not in unknown:
                entry["ignored"].update(given - gcmd._vp_read)
        return {
            name: {"count": e["count"], "params": dict(sorted(e["params"].items())),
                   "ignored": dict(sorted(e["ignored"].items()))}
            for name, e in sorted(census.items())
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--config", required=True)
    parser.add_argument("--gcode", required=True)
    parser.add_argument("--dict", required=True)
    parser.add_argument("--log", required=True)
    parser.add_argument("--summary", required=True)
    args = parser.parse_args()
    for path in (args.config, args.gcode, args.dict):
        if not os.path.isfile(path):
            print("klippy_batch: not a file: %s" % path, file=sys.stderr)
            return 2

    sys.path.insert(0, KLIPPY_DIR)
    import klippy

    root = logging.getLogger()
    root.setLevel(logging.INFO)
    file_handler = logging.FileHandler(args.log, mode="w")
    file_handler.setFormatter(logging.Formatter("%(levelname)s %(message)s"))
    root.addHandler(file_handler)
    collector = RecordCollector()
    root.addHandler(collector)

    observer = Observer()
    observer.install()

    sys.argv = ["klippy.py", args.config, "-i", args.gcode, "-o", "/dev/null",
                "-d", args.dict]
    exit_code = 0
    try:
        klippy.main()
    except SystemExit as exc:
        exit_code = exc.code if isinstance(exc.code, int) else 1
    file_handler.flush()

    census = observer.command_census(collector.unknown)
    motion_time = first_layer_time = None
    starts = observer.object_starts
    if starts and observer.print_time_last_flush is not None:
        motion_time = round(observer.print_time_last_flush - starts[0], 1)
    if len(starts) == 2:
        first_layer_time = round(starts[1] - starts[0], 1)
    gcode_errors = observer.gcode_errors
    # G-code errors are logged at WARNING level too; keep them out of warnings.
    warnings = [w for w in collector.warnings if w.strip() not in gcode_errors]
    accepted = (exit_code == 0 and not collector.errors and not gcode_errors
                and not collector.unknown)
    summary = {
        "accepted": accepted,
        "klippy_exit_code": exit_code,
        "gcode_errors": gcode_errors,
        "errors": collector.errors,
        "unknown_commands": dict(collector.unknown),
        "warnings": warnings,
        "deprecated_parameters": [
            {"command": cmd, "param": param, "count": census[cmd]["params"][param], "note": note}
            for (cmd, param), note in sorted(DEPRECATED_PARAMS.items())
            if param in census.get(cmd, {}).get("params", {})
        ],
        "ignored_parameters": {name: e["ignored"] for name, e in census.items() if e["ignored"]},
        "command_census": census,
        "commands_processed": len(observer.commands),
        "klipper_motion_time_s": motion_time,
        "klipper_first_layer_time_s": first_layer_time,
    }
    with open(args.summary, "w") as f:
        json.dump(summary, f, indent=2)
    return 0 if accepted else 1


if __name__ == "__main__":
    sys.exit(main())
