# IWYU First-Header Validator

**Tool:** `Tools/check_iwyu_first_header.py`  
**Installed hook:** `.git/hooks/pre-commit`  
**Sprint:** S-06 (U-IWYU)

---

## 1. What it does

UE5.5+ enforces the **IWYU first-header rule**: every `.cpp` file must have its
own matching header as the very first `#include`.  For example:

```cpp
// ArchSimScenarioWidget.cpp — CORRECT
#include "Editor/ArchSimScenarioWidget.h"   // ← must be first
#include "FrameCoreUE/FrameUtilizationHeatmapActor.h"
```

When this rule is violated UBT emits an error:

```
error: Expected "Editor/ArchSimScenarioWidget.h" to be first header included.
```

However, **the target build can still return `Result: Succeeded`** because the
stale `.obj` is silently linked into the new DLL.  This caused the v0.4.0.1
regression (AS-28): the cross-world fix in `ArchSimScenarioWidget.cpp` was
placed after a `FrameCoreUE` header, so UBT rejected the rebuild and the fix
was invisible for two rebuild rounds.

This validator catches the violation **before** any commit reaches the repo,
keeping the stale-obj window at zero.

---

## 2. Usage

### Ad-hoc (single file)

```bash
python Tools/check_iwyu_first_header.py Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp
# Exit 0 → PASS (silent)
# Exit 1 → FAIL (prints ERROR lines)
# Exit 2 → tool error (file unreadable, etc.)
```

### Full scan (Source/ + FrameCoreUE/)

```bash
python Tools/check_iwyu_first_header.py
# IWYU check: 74 files scanned, 0 violations — PASS
# Wall time: ~0.2 s on dev host
```

### Test suite

```bash
python Tools/test_iwyu_validator.py
# OR
python -m pytest Tools/test_iwyu_validator.py -v
# 7 fixtures: positive / negative / 5 edge/skip cases
```

---

## 3. Pre-commit hook

The hook is installed at `.git/hooks/pre-commit` (created by U-IWYU, Sprint S-06).
It runs automatically on every `git commit` and checks only the staged `.cpp`
files in that commit — unrelated files are never scanned.

### Manual installation (if the hook file is missing)

Copy-paste into Git Bash or WSL:

```bash
# From the repo root:
cp Tools/check_iwyu_first_header.py Tools/check_iwyu_first_header.py  # already there
cat > .git/hooks/pre-commit << 'EOF'
#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VALIDATOR="$REPO_ROOT/Tools/check_iwyu_first_header.py"
STAGED_CPP=$(git diff --cached --name-only --diff-filter=ACM | grep '\.cpp$' || true)
if [ -z "$STAGED_CPP" ]; then exit 0; fi
python "$VALIDATOR" $STAGED_CPP
EXIT_CODE=$?
if [ "$EXIT_CODE" -ne 0 ]; then
    echo "Pre-commit IWYU check FAILED. Fix the include order and re-stage."
    echo "See docs/IWYU_VALIDATOR.md for details."
    exit 1
fi
exit 0
EOF
chmod +x .git/hooks/pre-commit
```

### If a pre-commit hook already exists

Do **not** overwrite the existing hook.  Instead, append the IWYU call to it:

```bash
echo '' >> .git/hooks/pre-commit
echo '# IWYU first-header check (U-IWYU, S-06)' >> .git/hooks/pre-commit
echo 'python "$(git rev-parse --show-toplevel)/Tools/check_iwyu_first_header.py" \' >> .git/hooks/pre-commit
echo '  $(git diff --cached --name-only --diff-filter=ACM | grep "\.cpp$" || true)' >> .git/hooks/pre-commit
```

### Emergency bypass

```bash
git commit --no-verify -m "..."   # skips ALL hooks — use sparingly
```

---

## 4. Exceptions (paths always skipped)

| Pattern | Reason |
|---|---|
| `Source/**/Tests/**/*.cpp` | Test files conventionally start with `"Misc/AutomationTest.h"` — UBT does not enforce IWYU on automation TUs |
| `Plugins/FrameSolver/Source/FrameCore/**` | FROZEN since v4.0.0 — not our jurisdiction |
| `Plugins/LevelSim/Source/LevelCore/**` | FROZEN since v2.2+1 — not our jurisdiction |
| Any `.cpp` with no same-stem `.h` in Source/+Plugins/ | No header to require first — trivially compliant (e.g. `FrameCoreUEAnalysisMarshal.cpp`) |

### Adding a new exception

Edit the `_FROZEN_PREFIXES` list or the `_TESTS_DIR_RE` regex at the top of
`Tools/check_iwyu_first_header.py` and add a comment explaining why.

---

## 5. CI integration

Add to your GitHub Actions workflow (e.g. `.github/workflows/release-gate.yml`):

```yaml
- name: IWYU first-header check
  run: python Tools/check_iwyu_first_header.py
```

Place this step **before** the UE build step so violations are caught at the
cheapest possible gate (Python, ~0.2 s) rather than after a full UBT run.
