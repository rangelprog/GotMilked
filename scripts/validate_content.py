#!/usr/bin/env python3
"""
Validate content records under assets/content/data against their schemas.

Each subdirectory in assets/content/data/ is expected to have a matching schema
file named <directory>.schema.json in assets/content/schemas/.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Dict

import jsonschema
import yaml


REPO_ROOT = Path(__file__).resolve().parents[1]
SCHEMAS_DIR = REPO_ROOT / "assets" / "content" / "schemas"
DATA_DIR = REPO_ROOT / "assets" / "content" / "data"


def load_schemas() -> Dict[str, jsonschema.validators.Validator]:
    schemas: Dict[str, jsonschema.validators.Validator] = {}
    for schema_path in sorted(SCHEMAS_DIR.glob("*.schema.json")):
        # File pattern "<name>.schema.json"
        schema_name = schema_path.name.split(".")[0]
        with schema_path.open("r", encoding="utf-8") as fh:
            schema_json = json.load(fh)
        schemas[schema_name] = jsonschema.Draft202012Validator(schema_json)
    return schemas


def load_payload(path: Path):
    if path.suffix.lower() in {".yaml", ".yml"}:
        with path.open("r", encoding="utf-8") as fh:
            return yaml.safe_load(fh)
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def main() -> int:
    if not SCHEMAS_DIR.exists() or not DATA_DIR.exists():
        print("Content directories are missing; nothing to validate", file=sys.stderr)
        return 0

    validators = load_schemas()
    if not validators:
        print("No schema files were found; aborting", file=sys.stderr)
        return 1

    failures = []
    for type_dir in sorted(DATA_DIR.iterdir()):
        if not type_dir.is_dir():
            continue
        schema_name = type_dir.name
        validator = validators.get(schema_name)
        if validator is None:
            print(f"[WARN] No schema for content bucket '{schema_name}', skipping")
            continue

        for payload_path in sorted(type_dir.rglob("*")):
            if payload_path.is_dir():
                continue
            if payload_path.suffix.lower() not in {".json", ".yaml", ".yml"}:
                continue
            try:
                payload = load_payload(payload_path)
                validator.validate(payload)
            except Exception as exc:  # pylint: disable=broad-except
                failures.append((payload_path, exc))

    if failures:
        print("Schema validation failures detected:")
        for path, error in failures:
            print(f"  - {path.relative_to(REPO_ROOT)}: {error}")
        return 2

    print("All content files satisfied their schemas.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

