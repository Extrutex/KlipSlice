#!/usr/bin/env python3
"""Tests for scripts/klipslice_profile_guard.py: the Klipper-only invariants of the profile
tree (stdlib unittest, no external deps).

Run from the repo root:  python -m unittest discover -s scripts/tests -v
"""

import contextlib
import io
import json
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import klipslice_profile_guard as guard  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


class Tree:
    """A throwaway profile tree with one vendor and an evidence file beside it."""

    def __init__(self):
        self.dir = tempfile.mkdtemp(prefix="profile_guard_test_")
        self.profiles = os.path.join(self.dir, "profiles")
        self.evidence = os.path.join(self.dir, "printer-firmware.json")
        os.makedirs(os.path.join(self.profiles, "Acme", "machine"))
        self.index = {"name": "Acme", "version": "01.00.00.00", "machine_model_list": [], "machine_list": []}
        self.entries = []
        self.flush()

    def machine(self, name, **keys):
        preset = {"type": "machine", "name": name, **keys}
        with open(os.path.join(self.profiles, "Acme", "machine", name + ".json"), "w", encoding="utf-8") as fh:
            json.dump(preset, fh)
        self.index["machine_list"].append({"name": name, "sub_path": "machine/" + name + ".json"})
        self.flush()

    def model(self, name):
        self.index["machine_model_list"].append({"name": name, "sub_path": "machine/" + name + ".json"})
        self.flush()

    def evidence_entry(self, model, kept=True, verdict="STOCK_KLIPPER_MOONRAKER", confidence="high", sources=("https://example.test",)):
        self.entries.append({"vendor": "Acme", "model": model, "kept": kept, "verdict": verdict,
                             "confidence": confidence, "sources": list(sources)})
        self.flush()

    def flush(self):
        with open(os.path.join(self.profiles, "Acme.json"), "w", encoding="utf-8") as fh:
            json.dump(self.index, fh)
        with open(self.evidence, "w", encoding="utf-8") as fh:
            json.dump(self.entries, fh)

    def run(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            code = guard.main(["--profiles", self.profiles, "--evidence", self.evidence])
        return code, out.getvalue()

    def cleanup(self):
        shutil.rmtree(self.dir, ignore_errors=True)


class GuardTest(unittest.TestCase):
    def setUp(self):
        self.tree = Tree()
        self.addCleanup(self.tree.cleanup)
        self.tree.machine("fdm_common", instantiation="false", gcode_flavor="klipper")
        self.tree.machine("Acme One 0.4 nozzle", inherits="fdm_common", printer_model="Acme One")
        self.tree.model("Acme One")
        self.tree.evidence_entry("Acme One")

    def test_a_conforming_tree_passes_with_counts(self):
        code, out = self.tree.run()
        self.assertEqual(code, 0, out)
        self.assertIn("1 machine presets", out)
        self.assertIn("1 printer models", out)

    def test_flavor_is_read_through_inherits(self):
        self.tree.machine("fdm_marlin", instantiation="false", gcode_flavor="marlin2")
        self.tree.machine("Acme Two 0.4 nozzle", inherits="fdm_marlin")
        code, out = self.tree.run()
        self.assertEqual(code, 1)
        self.assertIn("Acme/Acme Two 0.4 nozzle: gcode_flavor is 'marlin2'", out)

    def test_a_preset_without_any_flavor_is_a_finding(self):
        self.tree.machine("Acme Bare 0.4 nozzle")
        code, out = self.tree.run()
        self.assertEqual(code, 1)
        self.assertIn("Acme Bare 0.4 nozzle: gcode_flavor is None", out)

    def test_only_moonraker_may_be_named_as_host(self):
        self.tree.machine("Acme Octo 0.4 nozzle", inherits="fdm_common", host_type="octoprint")
        code, out = self.tree.run()
        self.assertEqual(code, 1)
        self.assertIn("host_type is 'octoprint'", out)

    def test_an_unset_host_is_moonraker(self):
        self.tree.machine("Acme Quiet 0.4 nozzle", inherits="fdm_common")
        code, out = self.tree.run()
        self.assertEqual(code, 0, out)

    def test_non_instantiated_presets_are_not_checked(self):
        self.tree.machine("fdm_old", instantiation="false", gcode_flavor="reprap")
        code, out = self.tree.run()
        self.assertEqual(code, 0, out)

    def test_a_model_without_kept_evidence_is_a_finding(self):
        self.tree.model("Acme Mystery")
        code, out = self.tree.run()
        self.assertEqual(code, 1)
        self.assertIn("Acme/Acme Mystery: printer model ships without a kept entry", out)

    def test_dropped_evidence_does_not_count_as_kept(self):
        self.tree.model("Acme Dropped")
        self.tree.evidence_entry("Acme Dropped", kept=False, verdict="NOT_KLIPPER")
        code, out = self.tree.run()
        self.assertEqual(code, 1)
        self.assertIn("Acme/Acme Dropped: printer model ships without a kept entry", out)

    def test_kept_evidence_needs_a_listed_model(self):
        self.tree.evidence_entry("Acme Ghost")
        code, out = self.tree.run()
        self.assertEqual(code, 1)
        self.assertIn("evidence Acme / Acme Ghost: kept, but no <Vendor>.json lists that printer model", out)

    def test_kept_evidence_needs_a_kept_verdict_a_confidence_and_a_source(self):
        self.tree.model("Acme Weak")
        self.tree.evidence_entry("Acme Weak", verdict="UNCLEAR", confidence="guess", sources=())
        code, out = self.tree.run()
        self.assertEqual(code, 1)
        self.assertIn("kept with verdict 'UNCLEAR'", out)
        self.assertIn("confidence 'guess'", out)
        self.assertIn("kept without a single source", out)

    def test_dropped_evidence_needs_a_dropped_verdict(self):
        self.tree.evidence_entry("Acme Odd", kept=False, verdict="DIY_KLIPPER")
        code, out = self.tree.run()
        self.assertEqual(code, 1)
        self.assertIn("not kept but verdict 'DIY_KLIPPER'", out)

    def test_an_inherits_cycle_does_not_hang(self):
        self.tree.machine("loop_a", instantiation="false", inherits="loop_b")
        self.tree.machine("loop_b", instantiation="false", inherits="loop_a")
        self.tree.machine("Acme Loop 0.4 nozzle", inherits="loop_a")
        code, out = self.tree.run()
        self.assertEqual(code, 1)
        self.assertIn("Acme Loop 0.4 nozzle: gcode_flavor is None", out)


class RealTreeTest(unittest.TestCase):
    def test_the_shipped_tree_holds_the_invariants(self):
        profiles = os.path.join(REPO_ROOT, "resources", "profiles")
        evidence = os.path.join(REPO_ROOT, "docs", "klipslice", "printer-firmware.json")
        if not os.path.isdir(profiles) or not os.path.isfile(evidence):
            self.skipTest("not run from a KLIPSLICE checkout")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = guard.main(["--profiles", profiles, "--evidence", evidence])
        self.assertEqual(code, 0, out.getvalue())


if __name__ == "__main__":
    unittest.main()
