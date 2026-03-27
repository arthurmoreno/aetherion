#!/usr/bin/env python3
"""Generate local SVG badges for tests and coverage."""

from __future__ import annotations

import argparse
import html
import xml.etree.ElementTree as ET
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate local test/coverage badges.")
    parser.add_argument("--coverage", default="coverage.xml", help="Path to coverage XML file.")
    parser.add_argument("--status", default=".test_status", help="Path to test status marker file.")
    parser.add_argument("--output", default="_images", help="Directory for generated SVG badges.")
    return parser.parse_args()


def read_coverage_percent(path: Path) -> float | None:
    if not path.exists():
        return None

    try:
        root = ET.parse(path).getroot()
        line_rate = root.attrib.get("line-rate")
        if line_rate is None:
            return None
        return round(float(line_rate) * 100.0, 1)
    except (ET.ParseError, ValueError):
        return None


def read_test_status(path: Path) -> str:
    if not path.exists():
        return "unknown"

    content = path.read_text(encoding="utf-8").strip().lower()
    if content in {"passing", "pass", "ok", "success"}:
        return "passing"
    if content in {"failing", "fail", "error", "failed"}:
        return "failing"
    return "unknown"


def coverage_color(percent: float | None) -> str:
    if percent is None:
        return "#9f9f9f"
    if percent >= 90:
        return "#4c1"
    if percent >= 75:
        return "#97ca00"
    if percent >= 60:
        return "#dfb317"
    return "#e05d44"


def status_color(status: str) -> str:
    if status == "passing":
        return "#4c1"
    if status == "failing":
        return "#e05d44"
    return "#9f9f9f"


def measure_text_width(value: str) -> int:
    # Approximate width used by typical badge renderers.
    return max(24, 7 * len(value) + 10)


def build_badge(label: str, value: str, value_color: str) -> str:
    left_w = measure_text_width(label)
    right_w = measure_text_width(value)
    total_w = left_w + right_w
    left_mid = left_w / 2
    right_mid = left_w + right_w / 2

    label = html.escape(label)
    value = html.escape(value)

    return f"""<svg
  xmlns="http://www.w3.org/2000/svg"
  width="{total_w}"
  height="20"
  role="img"
  aria-label="{label}: {value}"
>
  <title>{label}: {value}</title>
  <rect width="{left_w}" height="20" fill="#555"/>
  <rect x="{left_w}" width="{right_w}" height="20" fill="{value_color}"/>
  <text
    x="{left_mid}"
    y="14"
    fill="#fff"
    font-family="DejaVu Sans,Verdana,Geneva,sans-serif"
    font-size="11"
    text-anchor="middle"
  >{label}</text>
  <text
    x="{right_mid}"
    y="14"
    fill="#fff"
    font-family="DejaVu Sans,Verdana,Geneva,sans-serif"
    font-size="11"
    text-anchor="middle"
  >{value}</text>
</svg>
"""


def write_badge(path: Path, label: str, value: str, color: str) -> None:
    path.write_text(build_badge(label, value, color), encoding="utf-8")


def main() -> int:
    args = parse_args()
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    coverage_percent = read_coverage_percent(Path(args.coverage))
    coverage_value = "unknown" if coverage_percent is None else f"{coverage_percent:.1f}%"
    write_badge(
        output_dir / "coverage-badge.svg",
        "coverage",
        coverage_value,
        coverage_color(coverage_percent),
    )

    tests_value = read_test_status(Path(args.status))
    write_badge(output_dir / "tests-badge.svg", "tests", tests_value, status_color(tests_value))

    print(f"Wrote {output_dir / 'coverage-badge.svg'}")
    print(f"Wrote {output_dir / 'tests-badge.svg'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
