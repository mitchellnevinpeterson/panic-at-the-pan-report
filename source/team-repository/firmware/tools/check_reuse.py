#!/usr/bin/env python3
"""Verify preserved lab/framework files against the recorded copy hashes."""
import hashlib
import json
from pathlib import Path

project = Path(__file__).resolve().parents[2]
manifest = project / "firmware/docs/reuse_manifest.json"
entries = json.loads(manifest.read_text())
failures = []
for entry in entries:
    destination = project / entry["destination"]
    if not destination.is_file():
        failures.append(f"Missing: {entry['destination']}")
    elif hashlib.sha256(destination.read_bytes()).hexdigest() != entry["sha256"]:
        failures.append(f"Changed since lab copy: {entry['destination']}")
if failures:
    raise SystemExit("\n".join(failures))
print(f"Verified {len(entries)} preserved lab/framework files.")
