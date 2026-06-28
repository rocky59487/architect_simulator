#!/usr/bin/env python3
"""
test_iwyu_validator.py — Unit tests for check_iwyu_first_header.py.

Run with:
  python Tools/test_iwyu_validator.py
  python -m pytest Tools/test_iwyu_validator.py -v

Fixtures:
  1 (positive)  — v0.4.0.1 post-fix order → PASS
  2 (negative)  — v0.4.0.1 pre-fix order → FAIL with correct file/line in message
  3 (edge)      — empty file → PASS
  4 (edge)      — comment-only file → PASS
  5 (edge)      — #pragma once but no #include → PASS
  6 (skip)      — Tests/ directory file with Misc/AutomationTest.h first → PASS
  7 (skip)      — FROZEN FrameCore path → PASS (not checked)
"""

import sys
import os
import tempfile
import unittest
from pathlib import Path

# ---------------------------------------------------------------------------
# Import the module under test.
# ---------------------------------------------------------------------------

# Add Tools/ to sys.path so we can import the validator directly.
_TOOLS_DIR = Path(__file__).resolve().parent
if str(_TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(_TOOLS_DIR))

import check_iwyu_first_header as validator


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

class _TempCpp:
    """Context manager: writes content to a named temp .cpp file and returns its Path."""

    def __init__(self, content: str, filename: str = "FooWidget.cpp"):
        self._content = content
        self._filename = filename
        self._dir = None
        self._path = None

    def __enter__(self) -> Path:
        self._dir = tempfile.mkdtemp()
        self._path = Path(self._dir) / self._filename
        self._path.write_text(self._content, encoding="utf-8")
        return self._path

    def __exit__(self, *_):
        import shutil
        shutil.rmtree(self._dir, ignore_errors=True)


def _make_nested(parent_dir: str, relative_path: str, content: str) -> Path:
    """Create a file at parent_dir/relative_path with the given content."""
    p = Path(parent_dir) / relative_path
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(content, encoding="utf-8")
    return p


# ---------------------------------------------------------------------------
# Fixture content strings
# ---------------------------------------------------------------------------

# Fixture 1 — v0.4.0.1 post-fix (correct IWYU order):
#   ArchSimScenarioWidget.cpp starts with #include "Editor/ArchSimScenarioWidget.h"
_FIXTURE_1_CONTENT = """\
// ArchSim — UArchSimScenarioWidget implementation.
// v0.4.0.1 (AS-28): own header MUST be the first #include (UE5.5+ IWYU rule).
#include "Editor/ArchSimScenarioWidget.h"

#include "FrameCoreUE/FrameUtilizationHeatmapActor.h"
#include "Components/ArchSimMemberData.h"
"""

# Fixture 2 — v0.4.0.1 pre-fix (wrong IWYU order):
#   FrameCoreUE header appears before the matching widget header.
_FIXTURE_2_CONTENT = """\
// ArchSim — UArchSimScenarioWidget implementation (BUG: wrong include order).
#include "FrameCoreUE/FrameUtilizationHeatmapActor.h"

#include "Editor/ArchSimScenarioWidget.h"
#include "Components/ArchSimMemberData.h"
"""

# Fixture 3 — empty file.
_FIXTURE_3_CONTENT = ""

# Fixture 4 — comment-only file.
_FIXTURE_4_CONTENT = """\
// This file is intentionally blank.
/* Another comment block. */
// No #include directives at all.
"""

# Fixture 5 — #pragma once but no #include.
_FIXTURE_5_CONTENT = """\
#pragma once

// Forward declarations only — no includes needed.
class SomeThing;
"""

# Fixture 6 — test file: first include is Misc/AutomationTest.h (UE convention).
# This should be SKIPPED (returned as PASS) because it lives in a Tests/ directory.
_FIXTURE_6_CONTENT = """\
#include "Misc/AutomationTest.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMyTest, "FrameCore.UE.MyTest", ...)
"""

# Fixture 7 — file under FROZEN FrameCore path.
# This should be SKIPPED regardless of include order.
_FIXTURE_7_CONTENT = """\
#include "SomeOtherHeader.h"
#include "FrameCore/FrameSolver.h"
"""


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------

class TestIwyuValidator(unittest.TestCase):

    def setUp(self):
        # Reset the header-stem cache before each test so that our per-test
        # temp directory stubs are discovered fresh rather than relying on the
        # real repo's cached stems.
        validator._KNOWN_HEADER_STEMS = None

    def tearDown(self):
        # Restore to None so the next test starts from a clean slate.
        validator._KNOWN_HEADER_STEMS = None

    # ------------------------------------------------------------------
    # Fixture 1 — positive: v0.4.0.1 post-fix → PASS
    # ------------------------------------------------------------------
    def test_fixture_1_postfix_correct_order_pass(self):
        """v0.4.0.1 post-fix: first include is matching header → PASS."""
        with _TempCpp(_FIXTURE_1_CONTENT, "ArchSimScenarioWidget.cpp") as cpp:
            # Create a fake same-stem .h so stem_header_exists() returns True.
            _h = cpp.parent / "ArchSimScenarioWidget.h"
            _h.write_text("// stub", encoding="utf-8")

            # Temporarily redirect _REPO_ROOT so the stem-search glob finds our stub.
            orig_root = validator._REPO_ROOT
            try:
                # Point REPO_ROOT at the temp dir so "Source/**/<stem>.h" is found.
                # We inject a fake Source subdir to satisfy the glob.
                fake_source = cpp.parent / "Source"
                fake_source.mkdir(exist_ok=True)
                fake_h_in_source = fake_source / "ArchSimScenarioWidget.h"
                fake_h_in_source.write_text("// stub", encoding="utf-8")
                validator._REPO_ROOT = cpp.parent

                # Also adjust FROZEN prefixes so this temp path is not excluded.
                orig_frozen = validator._FROZEN_PREFIXES
                validator._FROZEN_PREFIXES = []  # no frozen paths for this test

                result = validator.check_file(cpp)
                self.assertIsNone(result, f"Expected PASS, got: {result}")
            finally:
                validator._REPO_ROOT = orig_root
                validator._FROZEN_PREFIXES = orig_frozen if 'orig_frozen' in dir() else []

    # ------------------------------------------------------------------
    # Fixture 2 — negative: v0.4.0.1 pre-fix → FAIL
    # ------------------------------------------------------------------
    def test_fixture_2_prefix_wrong_order_fail(self):
        """v0.4.0.1 pre-fix: first include is FrameCoreUE header → FAIL.

        The violation message must contain:
          - The filename stem "ArchSimScenarioWidget"
          - The line number (line 2 in this fixture — comment is line 1,
            first #include is line 2)
          - The offending header path
        """
        with _TempCpp(_FIXTURE_2_CONTENT, "ArchSimScenarioWidget.cpp") as cpp:
            orig_root = validator._REPO_ROOT
            orig_frozen = validator._FROZEN_PREFIXES
            try:
                fake_source = cpp.parent / "Source"
                fake_source.mkdir(exist_ok=True)
                (fake_source / "ArchSimScenarioWidget.h").write_text("// stub", encoding="utf-8")
                validator._REPO_ROOT = cpp.parent
                validator._FROZEN_PREFIXES = []

                result = validator.check_file(cpp)
                self.assertIsNotNone(result, "Expected FAIL (violation), but got PASS")
                # Must mention the filename stem.
                self.assertIn("ArchSimScenarioWidget", result)
                # Must mention the actual offending include.
                self.assertIn("FrameCoreUE/FrameUtilizationHeatmapActor.h", result)
                # Must include a line number (the format is "line <N>").
                import re
                m = re.search(r"line (\d+)", result)
                self.assertIsNotNone(m, f"No 'line N' in message: {result}")
                lineno = int(m.group(1))
                # Line 1 is the comment; line 2 is the first #include.
                self.assertEqual(lineno, 2,
                    f"Expected violation on line 2, got line {lineno}. Message: {result}")
            finally:
                validator._REPO_ROOT = orig_root
                validator._FROZEN_PREFIXES = orig_frozen

    # ------------------------------------------------------------------
    # Fixture 3 — edge: empty file → PASS
    # ------------------------------------------------------------------
    def test_fixture_3_empty_file_pass(self):
        """Empty file has no #include — trivially compliant, PASS."""
        with _TempCpp(_FIXTURE_3_CONTENT, "FooWidget.cpp") as cpp:
            orig_root = validator._REPO_ROOT
            orig_frozen = validator._FROZEN_PREFIXES
            try:
                fake_source = cpp.parent / "Source"
                fake_source.mkdir(exist_ok=True)
                # Create a matching stub .h so the file isn't skipped for missing header.
                (fake_source / "FooWidget.h").write_text("// stub", encoding="utf-8")
                validator._REPO_ROOT = cpp.parent
                validator._FROZEN_PREFIXES = []

                result = validator.check_file(cpp)
                self.assertIsNone(result, f"Expected PASS for empty file, got: {result}")
            finally:
                validator._REPO_ROOT = orig_root
                validator._FROZEN_PREFIXES = orig_frozen

    # ------------------------------------------------------------------
    # Fixture 4 — edge: comment-only → PASS
    # ------------------------------------------------------------------
    def test_fixture_4_comment_only_pass(self):
        """File with only comments, no #include — trivially compliant, PASS."""
        with _TempCpp(_FIXTURE_4_CONTENT, "FooWidget.cpp") as cpp:
            orig_root = validator._REPO_ROOT
            orig_frozen = validator._FROZEN_PREFIXES
            try:
                fake_source = cpp.parent / "Source"
                fake_source.mkdir(exist_ok=True)
                (fake_source / "FooWidget.h").write_text("// stub", encoding="utf-8")
                validator._REPO_ROOT = cpp.parent
                validator._FROZEN_PREFIXES = []

                result = validator.check_file(cpp)
                self.assertIsNone(result, f"Expected PASS for comment-only, got: {result}")
            finally:
                validator._REPO_ROOT = orig_root
                validator._FROZEN_PREFIXES = orig_frozen

    # ------------------------------------------------------------------
    # Fixture 5 — edge: #pragma once but no #include → PASS
    # ------------------------------------------------------------------
    def test_fixture_5_pragma_no_include_pass(self):
        """File with #pragma once but no #include — trivially compliant, PASS."""
        with _TempCpp(_FIXTURE_5_CONTENT, "FooWidget.cpp") as cpp:
            orig_root = validator._REPO_ROOT
            orig_frozen = validator._FROZEN_PREFIXES
            try:
                fake_source = cpp.parent / "Source"
                fake_source.mkdir(exist_ok=True)
                (fake_source / "FooWidget.h").write_text("// stub", encoding="utf-8")
                validator._REPO_ROOT = cpp.parent
                validator._FROZEN_PREFIXES = []

                result = validator.check_file(cpp)
                self.assertIsNone(result, f"Expected PASS for pragma-only, got: {result}")
            finally:
                validator._REPO_ROOT = orig_root
                validator._FROZEN_PREFIXES = orig_frozen

    # ------------------------------------------------------------------
    # Fixture 6 — skip: test file → PASS regardless of include order
    # ------------------------------------------------------------------
    def test_fixture_6_test_file_skipped(self):
        """File under Tests/ directory is skipped — PASS regardless of first include."""
        import tempfile, shutil
        tmp = tempfile.mkdtemp()
        try:
            # Place the file under a Tests/ subdirectory.
            cpp = _make_nested(tmp, "Source/Private/Tests/FooWidget.cpp",
                               _FIXTURE_6_CONTENT)
            # Also add a same-stem .h so the stem-existence check would fire if not skipped.
            _make_nested(tmp, "Source/FooWidget.h", "// stub")

            orig_root = validator._REPO_ROOT
            orig_frozen = validator._FROZEN_PREFIXES
            try:
                validator._REPO_ROOT = Path(tmp)
                validator._FROZEN_PREFIXES = []

                result = validator.check_file(cpp)
                self.assertIsNone(result,
                    f"Expected Tests/ file to be skipped (PASS), got: {result}")
            finally:
                validator._REPO_ROOT = orig_root
                validator._FROZEN_PREFIXES = orig_frozen
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    # ------------------------------------------------------------------
    # Fixture 7 — skip: FROZEN FrameCore path → PASS regardless
    # ------------------------------------------------------------------
    def test_fixture_7_frozen_path_skipped(self):
        """File under a FROZEN path is skipped — PASS regardless of include order."""
        import tempfile, shutil
        tmp = tempfile.mkdtemp()
        try:
            frozen_dir = Path(tmp) / "Plugins" / "FrameSolver" / "Source" / "FrameCore"
            cpp = _make_nested(
                tmp,
                "Plugins/FrameSolver/Source/FrameCore/Private/SomeEngine.cpp",
                _FIXTURE_7_CONTENT,
            )
            # Add matching .h so stem check would fire if not frozen.
            _make_nested(tmp, "Source/SomeEngine.h", "// stub")

            orig_root = validator._REPO_ROOT
            orig_frozen = validator._FROZEN_PREFIXES
            try:
                validator._REPO_ROOT = Path(tmp)
                # Restore real frozen prefixes relative to our tmp root.
                validator._FROZEN_PREFIXES = [
                    Path(tmp) / "Plugins" / "FrameSolver" / "Source" / "FrameCore",
                    Path(tmp) / "Plugins" / "LevelSim" / "Source" / "LevelCore",
                ]

                result = validator.check_file(cpp)
                self.assertIsNone(result,
                    f"Expected FROZEN path to be skipped (PASS), got: {result}")
            finally:
                validator._REPO_ROOT = orig_root
                validator._FROZEN_PREFIXES = orig_frozen
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    # Run tests with verbose output so each fixture label is visible.
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(TestIwyuValidator)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)
