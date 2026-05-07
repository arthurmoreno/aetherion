"""Subprocess-based harness for detecting nanobind reference leaks.

Each `run_snippet(...)` call executes the given Python source in a fresh
interpreter so we can observe `Py_Finalize` cleanly — pytest's own
interpreter is shared across tests and cannot be torn down between them.

The harness captures both stderr (to parse nanobind's `leaked N instances!`
diagnostic) and the subprocess exit code (to catch the SIGABRT that follows
when nanobind tears down its type registry with leaked refs still alive).

A run is `is_clean` only when:
  - exit code is 0 (no abort, no segfault, no exception)
  - all three leak counts (instances / types / functions) are 0
"""

from __future__ import annotations

import re
import subprocess
import sys
import textwrap
from dataclasses import dataclass

_LEAK_LINE_RE = re.compile(r"^nanobind: leaked (\d+) (instances|types|functions)!", re.MULTILINE)


def _parse_leak_counts(stderr: str) -> dict[str, int]:
    counts = {"instances": 0, "types": 0, "functions": 0}
    for match in _LEAK_LINE_RE.finditer(stderr):
        counts[match.group(2)] = int(match.group(1))
    return counts


@dataclass
class LeakRunResult:
    exit_code: int
    stdout: str
    stderr: str
    leaked_instances: int
    leaked_types: int
    leaked_functions: int

    @property
    def is_clean(self) -> bool:
        return (
            self.exit_code == 0 and self.leaked_instances == 0 and self.leaked_types == 0 and self.leaked_functions == 0
        )

    @property
    def aborted(self) -> bool:
        # 134 = 128 + SIGABRT(6); 139 = 128 + SIGSEGV(11); negative = killed by signal.
        return self.exit_code in (134, 139) or self.exit_code < 0

    def summary(self) -> str:
        return (
            f"exit={self.exit_code} "
            f"leaked(instances={self.leaked_instances}, "
            f"types={self.leaked_types}, "
            f"functions={self.leaked_functions})"
        )


def run_snippet(snippet: str, *, timeout: int = 30) -> LeakRunResult:
    """Run `snippet` in a fresh Python interpreter and capture leak diagnostics.

    The snippet is run via `python -c <code>`, so it inherits the active
    interpreter and `sys.path` (i.e. when pytest runs under the conda env,
    the subprocess uses the same env and can import `aetherion`).
    """
    proc = subprocess.run(
        [sys.executable, "-c", textwrap.dedent(snippet)],
        capture_output=True,
        text=True,
        timeout=timeout,
    )
    counts = _parse_leak_counts(proc.stderr)
    return LeakRunResult(
        exit_code=proc.returncode,
        stdout=proc.stdout,
        stderr=proc.stderr,
        leaked_instances=counts["instances"],
        leaked_types=counts["types"],
        leaked_functions=counts["functions"],
    )
