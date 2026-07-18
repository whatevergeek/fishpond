#!/usr/bin/env python3
"""Export CTest registration/disabled state and fail a required-evidence gate."""
import argparse, json, pathlib, subprocess, sys

parser = argparse.ArgumentParser()
parser.add_argument("--build-dir", required=True)
parser.add_argument("--manifest", required=True)
parser.add_argument("--output", required=True)
parser.add_argument("--results", help="optional JUnit result file from CTest")
parser.add_argument("--configuration", help="CTest configuration for multi-config generators")
args = parser.parse_args()
manifest = json.loads(pathlib.Path(args.manifest).read_text())
ctest_list_command = ["ctest", "--test-dir", args.build_dir]
if args.configuration:
    ctest_list_command.extend(["-C", args.configuration])
ctest_list_command.append("--show-only=json-v1")
listed = json.loads(subprocess.check_output(ctest_list_command, text=True))
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
for record in records:
    record["result"] = "not-run"
if args.results:
    import xml.etree.ElementTree as ET
    for case in ET.parse(args.results).iter("testcase"):
        if case.attrib.get("name") in tests:
            next(r for r in records if r["id"] == case.attrib["name"])["result"] = "failed" if case.find("failure") is not None else "passed"
for record in records:
    if record["required"] and record["result"] != "passed": failures.append(record["id"])
report = {"schema_version": 1, "fixture_version": manifest["fixture_version"], "tests": records, "gate_passed": not failures, "failures": sorted(set(failures)), "pending": pending, "required_external_evidence": manifest.get("external_evidence", {"juce_vst3_smoke": {"status": "not-run"}, "juce_au_smoke_macos": {"status": "not-run"}, "windows_runner": {"status": "not-run"}, "linux_runner": {"status": "not-run"}})}
pathlib.Path(args.output).write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({"gate_passed": report["gate_passed"], "failures": failures, "pending": pending}))
sys.exit(0 if report["gate_passed"] else 2)
