#!/usr/bin/env python3
"""Architecture guard for drogon-pay (CI gate + local check).

Enforces the layering rules documented in CONTRIBUTING.md:

  1. Library never depends on a host:
     libs/drogon-pay/src/** and include/** must not #include anything
     from examples/.
  2. Public API must not leak internals:
     libs/drogon-pay/include/drogon_pay/** must not #include internal
     src/ headers (relative "../src/..." or known internal header names).
  3. Services depend on the SPI only:
     libs/drogon-pay/src/services/** must not #include concrete channel
     headers from src/channels/**. (The dynamic_pointer_cast exception for
     channel-specific capabilities is confined to the handlers layer.)
  4. Public API surface freeze:
     include/drogon_pay/ must contain exactly the whitelisted headers.
     Adding a public header requires editing PUBLIC_API_WHITELIST here,
     in the same PR (deliberate friction).

Exit code 0 = all rules pass; 1 = violations (printed one per line).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
LIB_ROOT = REPO_ROOT / "libs" / "drogon-pay"
PUBLIC_INCLUDE = LIB_ROOT / "include" / "drogon_pay"
LIB_SRC = LIB_ROOT / "src"

# Rule 4: the frozen public API surface. Keep sorted.
PUBLIC_API_WHITELIST = {
    "ChannelRegistry.h",
    "PayErrorCategory.h",
    "PaymentChannel.h",
    "PayPlugin.h",
}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+["<]([^">]+)[">]')

# Rule 3 escape hatch: a concrete-channel include is tolerated only when the
# preceding lines carry an explicit, reviewable annotation (mirrors the
# "dynamic_pointer_cast exception + whitelist comment" policy).
WHITELIST_MARKER = "SPI whitelist exception"

# Internal top-level directories under src/ whose headers must never appear
# in a public header include line (rule 2).
INTERNAL_DIRS = ("channels/", "handlers/", "services/", "models/", "utils/")


def iter_sources(root: Path):
    for ext in ("*.h", "*.hpp", "*.cc", "*.cpp"):
        yield from root.rglob(ext)


def includes_of(path: Path):
    text = path.read_text(encoding="utf-8", errors="replace")
    for lineno, line in enumerate(text.splitlines(), start=1):
        m = INCLUDE_RE.match(line)
        if m:
            yield lineno, m.group(1).replace("\\", "/")


def rel(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def main() -> int:
    violations: list[str] = []

    # --- Rule 1: library must not include host (examples/) code -------------
    for f in iter_sources(LIB_ROOT):
        for lineno, inc in includes_of(f):
            if "examples/" in inc:
                violations.append(
                    f"[rule1 lib->host] {rel(f)}:{lineno}: #include \"{inc}\""
                )

    # --- Rule 2: public headers must not leak internal src/ headers ---------
    for f in iter_sources(PUBLIC_INCLUDE):
        for lineno, inc in includes_of(f):
            leaked = "../src/" in inc or inc.startswith("src/") or any(
                inc.startswith(d) for d in INTERNAL_DIRS
            )
            if leaked:
                violations.append(
                    f"[rule2 api-leak] {rel(f)}:{lineno}: #include \"{inc}\""
                )

    # --- Rule 3: services must not include concrete channel headers ---------
    channel_headers = {p.name for p in (LIB_SRC / "channels").glob("*.h")}
    for f in iter_sources(LIB_SRC / "services"):
        lines = f.read_text(encoding="utf-8", errors="replace").splitlines()
        for lineno, inc in includes_of(f):
            if "channels/" in inc or Path(inc).name in channel_headers:
                # Tolerate only explicitly annotated exceptions: the marker
                # must appear within the 10 lines above the include.
                context = "\n".join(lines[max(0, lineno - 11):lineno - 1])
                if WHITELIST_MARKER in context:
                    continue
                violations.append(
                    f"[rule3 svc->channel] {rel(f)}:{lineno}: #include \"{inc}\" "
                    f"(annotate with '{WHITELIST_MARKER}' if this is a "
                    f"deliberate dynamic_pointer_cast exception)"
                )

    # --- Rule 4: public API surface freeze -----------------------------------
    actual = {p.name for p in PUBLIC_INCLUDE.glob("*.h")}
    for extra in sorted(actual - PUBLIC_API_WHITELIST):
        violations.append(
            f"[rule4 api-surface] unexpected public header include/drogon_pay/{extra} "
            f"(add it to PUBLIC_API_WHITELIST in scripts/check_architecture.py "
            f"if intentional)"
        )
    for missing in sorted(PUBLIC_API_WHITELIST - actual):
        violations.append(
            f"[rule4 api-surface] whitelisted public header missing: "
            f"include/drogon_pay/{missing}"
        )

    if violations:
        print(f"Architecture guard FAILED ({len(violations)} violation(s)):")
        for v in violations:
            print("  " + v)
        return 1

    print("Architecture guard passed (4 rules, "
          f"{len(PUBLIC_API_WHITELIST)} public headers).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
