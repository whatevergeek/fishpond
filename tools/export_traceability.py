#!/usr/bin/env python3
"""Export CTest registration/disabled state and fail a required-evidence gate."""
import argparse, json, pathlib, subprocess, sys

parser = argparse.ArgumentParser()
parser.add_argument("--build-dir", required=True)
parser.add_argument("--manifest", required=True)
parser.add_argument("--output", required=True)
args = parser.parse_args()
manifest = json.loads(pathlib.Path(args.manifest).read_text())
listed = json.loads(subprocess.check_output(["ctest", "--test-dir", args.build_dir, "--show-only=json-v1"], text=True))
tests = {item["name"]: item for item in listed["tests"]}
records, failures, pending = [], [], []
for item in manifest["tests"]:
    observed = tests.get(item["id"])
    disabled = bool(observed and any(p["name"] == "DISABLED" and p["value"] in (True, "TRUE", "true", "1") for p in observed.get("properties", [])))
    state = "missing" if not observed else "disabled" if disabled else "registered"
    record = {**item, "state": state}
    records.append(record)
    if item["required"] and state != "registered": failures.append(item["id"])
    if not item["required"] and state != "registered": pending.append(item["id"])
report = {"schema_version": 1, "fixture_version": manifest["fixture_version"], "tests": records, "gate_passed": not failures, "failures": failures, "pending": pending}
pathlib.Path(args.output).write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({"gate_passed": report["gate_passed"], "failures": failures, "pending": pending}))
sys.exit(0 if report["gate_passed"] else 2)
