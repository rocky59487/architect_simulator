#!/usr/bin/env python3
"""
check_iwyu_first_header.py — IWYU first-header rule validator for ArchSim.

WHY THIS RULE EXISTS:
  UE5.5+ enforces "IWYU first-header rule": each .cpp file's first #include
  must be the matching header for that translation unit (e.g. Foo.cpp first
  include must be "Foo.h" or "<subdir>/Foo.h").

  When UBT detects a violation it emits:
      error: Expected ... to be first header included.
  but the *target build can still return "Result: Succeeded"* because the stale
  .obj is linked silently. This caused the v0.4.0.1 regression: cross-world fix
  in ArchSimScenarioWidget.cpp was invisible for two rebuild rounds because the
  IWYU violation kept the obj stale.

USAGE:
  # Check specific files:
  python Tools/check_iwyu_first_header.py Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp

  # Scan all Source/**/*.cpp and Plugins/FrameSolver/Source/FrameCoreUE/**/*.cpp:
  python Tools/check_iwyu_first_header.py

EXIT CODES:
  0 — all files PASS (no IWYU violations)
  1 — one or more violations found (each printed as ERROR line)
  2 — internal error (file not readable, etc.)

EXCLUDED PATHS (always skipped):
  - Source/**/Tests/**/*.cpp   — test files; first include is often Misc/AutomationTest.h
  - Plugins/FrameSolver/Source/FrameCore/**  — FROZEN engine; not our jurisdiction
  - Plugins/LevelSim/Source/LevelCore/**    — FROZEN engine; not our jurisdiction
"""

import sys
import re
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Repository root: one level above this script's directory (Tools/).
_REPO_ROOT = Path(__file__).resolve().parent.parent

# Scan roots when no explicit files are given.
_DEFAULT_SCAN_ROOTS = [
    _REPO_ROOT / "Source",
    _REPO_ROOT / "Plugins" / "FrameSolver" / "Source" / "FrameCoreUE",
]

# Paths whose content is permanently frozen — validator has no jurisdiction.
# These are resolved to absolute paths for reliable prefix matching.
_FROZEN_PREFIXES = [
    _REPO_ROOT / "Plugins" / "FrameSolver" / "Source" / "FrameCore",
    _REPO_ROOT / "Plugins" / "LevelSim" / "Source" / "LevelCore",
]

# Pattern for a Tests/ directory anywhere in the path.
# WHY: test .cpp files conventionally start with #include "Misc/AutomationTest.h"
# and UBT does not enforce IWYU first-header on automation test translation units.
_TESTS_DIR_RE = re.compile(r"[/\\][Tt]ests[/\\]")

# Matches a C-style or C++-style comment line (to skip when looking for first include).
_COMMENT_LINE_RE = re.compile(r"^\s*(//|/\*|\*)")

# Matches a preprocessor line that is NOT an include (e.g. #pragma, #define, #if, #endif).
_NONINC_PREPROCESSOR_RE = re.compile(r"^\s*#\s*(?!include)")

# Matches a blank line.
_BLANK_LINE_RE = re.compile(r"^\s*$")

# Matches an #include line; group(1) = the quoted/angled path.
_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')


# ---------------------------------------------------------------------------
# Core logic
# ---------------------------------------------------------------------------

def _is_frozen(path: Path) -> bool:
    """Return True if *path* lives under a FROZEN engine directory.

    WHY absolute-prefix check rather than glob:
      Frozen paths are stable directories, not wildcard patterns.
      A single startswith() per file is O(1) and avoids false positives
      from similarly-named directories (e.g. FrameCoreFoo/).
    """
    resolved = path.resolve()
    return any(resolved.is_relative_to(fp) for fp in _FROZEN_PREFIXES)


def _is_test_file(path: Path) -> bool:
    """Return True if *path* is inside a Tests/ subdirectory.

    WHY: UBT skips the IWYU first-header check for automation test TUs.
    We mirror that exception so legitimate test files (which start with
    "Misc/AutomationTest.h") are not flagged as violations.
    """
    return bool(_TESTS_DIR_RE.search(str(path)))


# ---------------------------------------------------------------------------
# Header stem cache — built once per process run to avoid per-file glob cost.
# Maps stem (e.g. "ArchSimScenarioWidget") → True if any matching .h exists
# under Source/ or Plugins/ in the repo tree.
#
# WHY a module-level cache:
#   Without caching, each call to _stem_header_exists() runs two recursive
#   globs.  On a tree with ~1600 header files across Source/ + Plugins/, this
#   pushes full-scan time past 1 s on dev hosts.  Building the set once at
#   startup brings repeated lookups to O(1) and keeps total scan wall-time
#   well under 1 s.
_KNOWN_HEADER_STEMS: set[str] | None = None


def _build_header_stem_cache() -> set[str]:
    """Walk Source/ and Plugins/ once and collect all .h stem names."""
    stems: set[str] = set()
    for root in [_REPO_ROOT / "Source", _REPO_ROOT / "Plugins"]:
        if root.is_dir():
            for h in root.rglob("*.h"):
                stems.add(h.stem)
    return stems


def _stem_header_exists(cpp_path: Path) -> bool:
    """Return True if a same-stem .h header exists anywhere in the repo's Source tree.

    WHY we check existence:
      Some .cpp files are pure internal implementation helpers with no public
      header (e.g. FrameCoreUEAnalysisMarshal.cpp has no matching
      FrameCoreUEAnalysisMarshal.h).  UBT only enforces IWYU first-header when
      a matching header *does* exist in the module's include paths. When no
      same-stem .h can be found anywhere, the file is trivially compliant.

    WHY search under Source/ and Plugins/ only:
      We don't want to accidentally match a generated .h in Intermediate/.
      Limiting the glob to the tracked source tree is safe and fast.

    WHY module-level cache:
      Amortises repeated rglob() cost across the full file list.  First call
      builds the set; subsequent calls are O(1) dict lookup.
    """
    global _KNOWN_HEADER_STEMS
    if _KNOWN_HEADER_STEMS is None:
        _KNOWN_HEADER_STEMS = _build_header_stem_cache()
    return cpp_path.stem in _KNOWN_HEADER_STEMS


def _first_include_line(cpp_path: Path):
    """Return (line_number, include_path_str) for the first #include in *cpp_path*.

    Skips blank lines, comment lines, and non-include preprocessor directives
    (e.g. #pragma once, #define, #if).

    Returns (None, None) if no #include directive is found in the file.
    Raises IOError if the file cannot be read.
    """
    with cpp_path.open(encoding="utf-8", errors="replace") as fh:
        for lineno, raw in enumerate(fh, start=1):
            line = raw.rstrip("\n")
            # Skip blank lines.
            if _BLANK_LINE_RE.match(line):
                continue
            # Skip comment lines (// or /* or continuation *).
            if _COMMENT_LINE_RE.match(line):
                continue
            # Skip non-include preprocessor lines (#pragma, #define, #if, etc.).
            if _NONINC_PREPROCESSOR_RE.match(line):
                continue
            # If we hit an #include, return it.
            m = _INCLUDE_RE.match(line)
            if m:
                return lineno, m.group(1)
            # Any other non-blank, non-comment, non-preprocessor line means the
            # file has no leading #include — stop searching.
            break
    return None, None


def check_file(cpp_path: Path):
    """Validate IWYU first-header rule for a single .cpp file.

    Returns:
      None   — file is compliant (PASS)
      str    — human-readable violation message (FAIL)

    Raises:
      IOError if the file cannot be opened.
    """
    # --- Exclusion gates (in order of cheapness) ---

    # Gate 1: Frozen paths — not our jurisdiction.
    if _is_frozen(cpp_path):
        return None

    # Gate 2: Test files — UBT doesn't enforce IWYU on automation TUs.
    if _is_test_file(cpp_path):
        return None

    # Gate 3: No same-stem .h exists — trivially compliant (no rule to enforce).
    if not _stem_header_exists(cpp_path):
        return None

    # --- Actual IWYU check ---

    lineno, first_inc = _first_include_line(cpp_path)

    # No #include at all — trivially compliant.
    if first_inc is None:
        return None

    stem = cpp_path.stem  # e.g. "ArchSimScenarioWidget"

    # Accept if the include path ends with /<stem>.h or is exactly <stem>.h.
    # WHY endswith check instead of exact match:
    #   UE includes are often qualified with a module subdir prefix.
    #   "Editor/ArchSimScenarioWidget.h" and "ArchSimScenarioWidget.h" are both
    #   valid first includes for ArchSimScenarioWidget.cpp.  Using endswith()
    #   captures both flat and subdir-qualified forms without over-specifying
    #   the exact prefix.
    expected_suffix_plain = f"{stem}.h"
    expected_suffix_slash = f"/{stem}.h"

    if first_inc == expected_suffix_plain or first_inc.endswith(expected_suffix_slash):
        return None

    # Violation detected.
    rel = cpp_path.relative_to(_REPO_ROOT)
    return (
        f"ERROR: {rel}: line {lineno}: first include is \"{first_inc}\", "
        f"expected \"{stem}.h\" or \"<subdir>/{stem}.h\""
    )


def collect_files(paths):
    """Collect .cpp files from the given list of Path objects.

    Each path can be a .cpp file or a directory to scan recursively.
    """
    result = []
    for p in paths:
        if p.is_file():
            if p.suffix == ".cpp":
                result.append(p)
        elif p.is_dir():
            result.extend(sorted(p.rglob("*.cpp")))
        else:
            print(f"WARNING: {p} does not exist or is not a file/directory", file=sys.stderr)
    return result


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]

    # Build the list of .cpp files to check.
    if argv:
        # Explicit file arguments — resolve relative to cwd.
        paths = [Path(a).resolve() for a in argv]
    else:
        # Default: scan both Source/ and FrameCoreUE/.
        paths = _DEFAULT_SCAN_ROOTS

    try:
        files = collect_files(paths)
    except Exception as exc:
        print(f"FATAL: could not enumerate files: {exc}", file=sys.stderr)
        return 2

    violations = []
    errors = []

    for cpp in files:
        try:
            msg = check_file(cpp)
            if msg is not None:
                violations.append(msg)
        except IOError as exc:
            errors.append(f"ERROR reading {cpp}: {exc}")

    for v in violations:
        print(v)
    for e in errors:
        print(e, file=sys.stderr)

    if errors:
        # File-read errors are a tooling problem, not an IWYU violation.
        return 2

    if violations:
        return 1

    # All files passed — print a brief summary only when scanning by default
    # (not when the caller passes explicit files, to stay script-composable).
    if not argv:
        print(f"IWYU check: {len(files)} files scanned, 0 violations — PASS")

    return 0


if __name__ == "__main__":
    sys.exit(main())
