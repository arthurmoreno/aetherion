"""Self-tests for `_nanobind_leak_harness`.

These tests do NOT import aetherion. They verify the harness itself:
the parser handles the literal stderr format nanobind emits, and a
trivial subprocess (no extension modules) round-trips as `is_clean`.
"""

from __future__ import annotations

import textwrap

from _nanobind_leak_harness import (
    LeakRunResult,
    _parse_leak_counts,
    run_snippet,
)


def test_parse_leak_counts_no_leaks():
    counts = _parse_leak_counts("everything is fine\n")
    assert counts == {"instances": 0, "types": 0, "functions": 0}


def test_parse_leak_counts_all_three_kinds():
    """Mirror the literal stderr captured from a leaking `make test` run."""
    fake_stderr = textwrap.dedent(
        """\
        some unrelated noise
        nanobind: leaked 1064 instances!
         - leaked instance 0x79df366344c8 of type "aetherion._aetherion.X"
         - leaked instance 0x79df3bed8b28 of type "aetherion._aetherion.Y"
         - ... skipped remainder
        nanobind: leaked 35 types!
         - leaked type "Z"
         - ... skipped remainder
        nanobind: leaked 384 functions!
         - leaked function "foo"
         - ... skipped remainder
        nanobind: this is likely caused by a reference counting issue ...
        """
    )
    counts = _parse_leak_counts(fake_stderr)
    assert counts == {"instances": 1064, "types": 35, "functions": 384}


def test_parse_leak_counts_only_instances():
    """A run that leaks instances but no types/functions must still parse."""
    fake_stderr = "nanobind: leaked 7 instances!\n"
    counts = _parse_leak_counts(fake_stderr)
    assert counts == {"instances": 7, "types": 0, "functions": 0}


def test_leak_run_result_is_clean_only_when_all_zero():
    clean = LeakRunResult(0, "", "", 0, 0, 0)
    assert clean.is_clean

    leaked = LeakRunResult(0, "", "", 1, 0, 0)
    assert not leaked.is_clean

    aborted = LeakRunResult(134, "", "", 0, 0, 0)
    assert not aborted.is_clean
    assert aborted.aborted


def test_harness_detects_clean_run():
    """A trivial subprocess that does not load any nanobind module must
    return cleanly. If even this baseline isn't clean, either the parser
    is wrong or the subprocess setup is broken."""
    result = run_snippet("import sys; sys.exit(0)")
    assert result.is_clean, f"baseline import-only snippet was not clean: {result.summary()}\nstderr:\n{result.stderr}"
