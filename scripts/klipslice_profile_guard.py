#!/usr/bin/env python3
"""The Klipper-only invariants of the KLIPSLICE profile tree.

orca_profile_tool.py checks that the tree is well formed. This guard checks what makes it a
KLIPSLICE tree:

  1. every machine preset slices for Klipper: its effective `gcode_flavor`, after following
     `inherits`, is "klipper";
  2. every machine preset prints through Moonraker: `host_type`, where a preset sets it at
     all, is "moonraker" (unset means Moonraker, the only value the slicer knows);
  3. every printer model a <Vendor>.json lists has a kept entry in
     docs/klipslice/printer-firmware.json, every kept entry has a model, and every entry's
     verdict, confidence and sources are what its `kept` flag says they are.

Findings are printed one per line and the exit code is 1 when there is at least one. With
no findings the counts are printed and the exit code is 0.

Run from the repo root:  python3 scripts/klipslice_profile_guard.py
"""

import argparse
import json
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_PROFILES = os.path.join(REPO_ROOT, "resources", "profiles")
DEFAULT_EVIDENCE = os.path.join(REPO_ROOT, "docs", "klipslice", "printer-firmware.json")

KLIPPER_FLAVOR = "klipper"
MOONRAKER_HOST = "moonraker"

# A printer ships with a profile only when its evidence ends in one of these verdicts.
KEPT_VERDICTS = {
    "STOCK_KLIPPER_MOONRAKER",     # stock firmware is Klipper with Moonraker
    "STOCK_KLIPPER_NO_MOONRAKER",  # stock Klipper, Moonraker added by the owner
    "KLIPPER_VIA_ESTABLISHED_MOD", # Klipper through a community mod the evidence names
    "DIY_KLIPPER",                 # self-built or converted machine
}
DROPPED_VERDICTS = {"UNCLEAR", "NOT_KLIPPER"}
CONFIDENCES = {"high", "medium", "low"}


def _load_json(path):
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def vendor_bundles(profiles_dir):
    """{vendor name: parsed <Vendor>.json} for every bundle index in the tree."""
    bundles = {}
    for entry in sorted(os.listdir(profiles_dir)):
        if entry.endswith(".json"):
            bundles[entry[:-5]] = _load_json(os.path.join(profiles_dir, entry))
    return bundles


def machine_presets(profiles_dir, vendor, bundle):
    """{preset name: parsed machine json} for the bundle's machine_list; a missing file is
    skipped here, orca_profile_tool.py check reports it."""
    presets = {}
    for item in bundle.get("machine_list", []):
        path = os.path.join(profiles_dir, vendor, item["sub_path"])
        if os.path.isfile(path):
            presets[item["name"]] = _load_json(path)
    return presets


def effective_value(presets, name, key):
    """The value of `key` the preset `name` ends up with, following `inherits`; None when no
    preset in the chain sets it or the chain is broken."""
    seen = set()
    while name and name in presets and name not in seen:
        seen.add(name)
        preset = presets[name]
        if key in preset:
            return preset[key]
        name = preset.get("inherits", "")
    return None


def check_flavor_and_host(profiles_dir):
    """Findings for invariants 1 and 2, and the number of instantiated machine presets checked."""
    findings = []
    checked = 0
    for vendor, bundle in vendor_bundles(profiles_dir).items():
        presets = machine_presets(profiles_dir, vendor, bundle)
        for name, preset in presets.items():
            if str(preset.get("instantiation", "true")).lower() != "true":
                continue
            checked += 1
            flavor = effective_value(presets, name, "gcode_flavor")
            if flavor != KLIPPER_FLAVOR:
                findings.append(f"{vendor}/{name}: gcode_flavor is {flavor!r}, every KLIPSLICE machine slices for {KLIPPER_FLAVOR!r}")
            host = effective_value(presets, name, "host_type")
            if host not in (None, MOONRAKER_HOST):
                findings.append(f"{vendor}/{name}: host_type is {host!r}, the only print host is {MOONRAKER_HOST!r}")
    return findings, checked


def check_evidence(profiles_dir, evidence_path):
    """Findings for invariant 3, and the number of printer models checked."""
    findings = []
    models = set()
    for vendor, bundle in vendor_bundles(profiles_dir).items():
        for model in bundle.get("machine_model_list", []):
            models.add((vendor, model["name"]))

    kept = set()
    for entry in _load_json(evidence_path):
        key = (entry.get("vendor", ""), entry.get("model", ""))
        label = f"{key[0]} / {key[1]}"
        verdict = entry.get("verdict")
        if entry.get("kept"):
            kept.add(key)
            if verdict not in KEPT_VERDICTS:
                findings.append(f"evidence {label}: kept with verdict {verdict!r}, kept printers need one of {sorted(KEPT_VERDICTS)}")
            if entry.get("confidence") not in CONFIDENCES:
                findings.append(f"evidence {label}: confidence {entry.get('confidence')!r} is not one of {sorted(CONFIDENCES)}")
            if not entry.get("sources"):
                findings.append(f"evidence {label}: kept without a single source")
        elif verdict not in DROPPED_VERDICTS:
            findings.append(f"evidence {label}: not kept but verdict {verdict!r} is not one of {sorted(DROPPED_VERDICTS)}")

    for vendor, name in sorted(models - kept):
        findings.append(f"{vendor}/{name}: printer model ships without a kept entry in {os.path.relpath(evidence_path, REPO_ROOT)}")
    for vendor, name in sorted(kept - models):
        findings.append(f"evidence {vendor} / {name}: kept, but no <Vendor>.json lists that printer model")
    return findings, len(models)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Check the Klipper-only invariants of the profile tree.")
    parser.add_argument("--profiles", default=DEFAULT_PROFILES, help="resources/profiles directory (default: the repo's)")
    parser.add_argument("--evidence", default=DEFAULT_EVIDENCE, help="printer-firmware.json (default: the repo's)")
    args = parser.parse_args(argv)

    findings, presets = check_flavor_and_host(args.profiles)
    evidence_findings, models = check_evidence(args.profiles, args.evidence)
    findings += evidence_findings

    for finding in findings:
        print(finding)
    if findings:
        print(f"{len(findings)} finding(s)", file=sys.stderr)
        return 1
    print(f"ok: {presets} machine presets slice for Klipper through Moonraker, {models} printer models with evidence")
    return 0


if __name__ == "__main__":
    sys.exit(main())
