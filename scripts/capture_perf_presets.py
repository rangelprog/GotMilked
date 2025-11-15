#!/usr/bin/env python3
"""
Generate automated performance capture plans for weather/time-of-day presets.

This helper does not launch the game directly (CI runners typically lack GPUs),
but it emits structured JSON that downstream tooling can consume to drive
capture jobs on perf machines.
"""

from __future__ import annotations

import argparse
import json
import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List


SCENARIOS: Dict[str, Dict[str, object]] = {
    "sunny_midday": {
        "profilingPreset": "sunny",
        "durationSeconds": 120,
        "captureTypes": ["reflection", "light_probe"],
        "description": "Baseline clear-sky midday reference",
        "notes": [
            "Verifies outdoor shading stability with minimal fog/rain.",
            "Expected GPU frame time under 16.7ms on reference hardware."
        ]
    },
    "stormy_midday": {
        "profilingPreset": "stormy",
        "durationSeconds": 150,
        "captureTypes": ["reflection", "light_probe"],
        "description": "Heavy rain & wet materials stress scenario",
        "notes": [
            "Collects probes during maximum precipitation/wetness.",
            "Ensure weather particles and fog remain enabled."
        ]
    },
    "dusk_clear": {
        "profilingPreset": "dusk",
        "durationSeconds": 140,
        "captureTypes": ["reflection"],
        "description": "Golden-hour lighting & long shadows",
        "notes": [
            "Targets sun elevation ~15° for long shadow coverage.",
            "Useful for verifying cascaded shadow transitions."
        ]
    }
}


def build_plan(selected: List[str], output: Path | None, dry_run: bool) -> Dict[str, object]:
    timestamp = datetime.now(timezone.utc).isoformat()
    plan_entries = []
    for name in selected:
        data = SCENARIOS[name].copy()
        data["name"] = name
        data["timestamp"] = timestamp
        data["commands"] = [
            f"./GotMilked --headless --perf-capture {data['profilingPreset']} --seconds {data['durationSeconds']}"
        ]
        plan_entries.append(data)

    plan = {"generatedAt": timestamp, "scenarios": plan_entries}

    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        if dry_run:
            print(f"[dry-run] Would write plan to {output}")
        else:
            output.write_text(json.dumps(plan, indent=2))
            print(f"Wrote perf capture plan to {output}")
    return plan


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate automated perf capture plans.")
    parser.add_argument(
        "--preset",
        action="append",
        choices=sorted(SCENARIOS.keys()),
        help="Limit capture generation to the specified preset(s). Can be repeated."
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Optional path to save the capture plan JSON."
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print plan information without writing files."
    )
    args = parser.parse_args()

    selected = args.preset or sorted(SCENARIOS.keys())
    plan = build_plan(selected, args.output, args.dry_run)

    print("Perf capture plan:")
    print(json.dumps(plan, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

