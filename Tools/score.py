"""Score a scan batch against the corpus's ground truth: detection, false positives, masking, times (01 section 7.3).

Usage:
    python -I Tools/score.py --batch <batch>                 every configuration of the batch, F0 as the stock baseline
    python -I Tools/score.py --selftest
    python -I Tools/score.py --batch <mutated batch> --incremental <original batch>   SC6: I0-I3 vs new F1 findings

Definitions (fixed in 01 section 7.3 before any rule existed):
  * A seeded defect is (asset, expected rule). GameplayLint DETECTS it if the report has a finding of that rule on
    that asset (rule level). An asset is FLAGGED if any check at all (engine or GameplayLint) gave it an error or a
    warning (asset level) - the only level the stock run can be scored at.
  * Recall is over category "defect" only. "heldout" (expected misses, v1 does not walk them) and "masked" (a
    class-level IsDataValid error hides the rule, 01b #2) are reported separately, never mixed in.
  * A false positive is a GameplayLint finding on a control asset (category clean or nearmiss). The false-positive
    rate is control assets with at least one finding / control assets. A finding of the wrong rule on a defective
    asset is listed separately.
  * Only assets under /Game/GLSeed/ count: the stock commandlet also lists a /Script object (09, F-3).
  * Repeated runs of one configuration must give the same finding set (criterion E2); a difference is reported.
  * Percentiles use linear interpolation between closest ranks.
"""
from __future__ import annotations

import argparse
import json
import os
import statistics
import sys
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW_DIR = os.path.join(ROOT, "Benchmarks", "Raw")
SUMMARY_DIR = os.path.join(ROOT, "Benchmarks", "Summaries")
CORPUS_PREFIX = "/Game/GLSeed/"
CONTROLS = ("clean", "nearmiss")


def percentile(values: list[float], p: float) -> float:
    """Linear interpolation between closest ranks (the numpy default), p in [0, 100]."""
    ordered = sorted(values)
    if not ordered:
        return float("nan")
    rank = (len(ordered) - 1) * p / 100.0
    low = int(rank)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (rank - low)


def load(path: str) -> dict:
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def findings_of(report: dict) -> set[tuple[str, str]]:
    return {(f["asset"], f["rule"]) for f in report.get("findings", []) if f["asset"].startswith(CORPUS_PREFIX)}


def flagged_of(report: dict) -> set[str]:
    return {a["asset"] for a in report.get("assets", [])
            if a["asset"].startswith(CORPUS_PREFIX) and (a["result"] == "Invalid" or a["errors"] or a["warnings"])}


def score(truth: dict, report: dict, baseline: dict | None = None) -> dict:
    """Pure scoring of one report (plus an optional stock baseline report). Returns plain numbers and lists."""
    entries = [e for e in truth["entries"] if e["asset"].startswith(CORPUS_PREFIX)]
    found = findings_of(report)
    flagged = flagged_of(report)
    base_flagged = flagged_of(baseline) if baseline else None
    by_asset = defaultdict(set)
    for asset, rule in found:
        by_asset[asset].add(rule)

    cells = defaultdict(lambda: [0, 0])          # (rule, kind, variant) -> [detected, seeded]
    rules = defaultdict(lambda: [0, 0])
    asset_level = defaultdict(lambda: [0, 0])     # category -> [flagged, total]
    special = defaultdict(lambda: [0, 0])         # heldout / masked -> [detected at rule level, total]
    missed, false_positives, wrong_rule = [], [], []
    gap = [0, 0]                                  # [found by this run and not flagged by the baseline, defects]
    for e in entries:
        category, asset, rule = e["category"], e["asset"], e["rule"]
        asset_level[category][1] += 1
        asset_level[category][0] += asset in flagged
        if category == "defect":
            hit = (asset, rule) in found
            cells[(rule, e["kind"], e["variant"])][0] += hit
            cells[(rule, e["kind"], e["variant"])][1] += 1
            rules[rule][0] += hit
            rules[rule][1] += 1
            if not hit:
                missed.append(e)
            extra = by_asset[asset] - {rule}
            if extra:
                wrong_rule.append({"asset": asset, "expected": rule, "reported": sorted(extra)})
            if base_flagged is not None:
                gap[1] += 1
                gap[0] += hit and asset not in base_flagged
        elif category in ("heldout", "masked"):
            special[category][0] += (asset, rule) in found
            special[category][1] += 1
        elif category in CONTROLS and by_asset[asset]:
            false_positives.append({"asset": asset, "category": category, "kind": e["kind"], "rules": sorted(by_asset[asset])})

    defects = sum(v[1] for v in rules.values())
    detected = sum(v[0] for v in rules.values())
    controls = sum(asset_level[c][1] for c in CONTROLS)
    times = {rule: {"assets": len(v), "median_ms": percentile(v, 50), "p99_ms": percentile(v, 99)}
             for rule, v in (report.get("check_times_ms") or {}).items() if v}
    return {
        "defects": defects, "detected": detected, "recall": detected / defects if defects else float("nan"),
        "rules": dict(rules), "cells": {"|".join(k): v for k, v in sorted(cells.items())},
        "controls": controls, "fp_assets": len(false_positives),
        "fp_rate": len(false_positives) / controls if controls else float("nan"),
        "false_positives": false_positives, "wrong_rule": wrong_rule, "missed": missed,
        "asset_level": dict(asset_level), "special": dict(special),
        "gap": gap if baseline else None, "check_times": times,
    }


def reports_of(batch: str) -> dict[str, list[dict]]:
    manifest = load(os.path.join(RAW_DIR, f"{batch}_manifest.json"))
    out = defaultdict(list)
    for run in manifest["runs"]:
        path = os.path.join(RAW_DIR, run["name"] + ".json")
        if not run.get("priming") and os.path.exists(path):
            out[run["config"]].append(load(path))
    return out


def pct(value: float) -> str:
    return f"{100 * value:.1f}%"


def markdown(batch: str, config: str, result: dict, identical: bool) -> list[str]:
    lines = [f"## {config}", "",
             f"- Rule-level detection: **{result['detected']} / {result['defects']} = {pct(result['recall'])}** of seeded defects (category defect).",
             f"- False positives: **{result['fp_assets']} of {result['controls']} control assets = {pct(result['fp_rate'])}**; wrong-rule findings on defective assets: {len(result['wrong_rule'])}.",
             f"- Held out (expected misses) detected: {result['special'].get('heldout', [0, 0])[0]} / {result['special'].get('heldout', [0, 0])[1]}; "
             f"masked detected: {result['special'].get('masked', [0, 0])[0]} / {result['special'].get('masked', [0, 0])[1]}.",
             f"- Repeated runs gave identical finding sets: {'yes' if identical else '**no**'}."]
    if result["gap"]:
        lines.append(f"- Found here and not flagged at all by F0: {result['gap'][0]} / {result['gap'][1]} = {pct(result['gap'][0] / result['gap'][1]) if result['gap'][1] else 'n/a'} (SC3).")
    lines += ["", "| Category | Flagged by any check | Total |", "|---|---|---|"]
    for category, (flag, total) in sorted(result["asset_level"].items()):
        lines.append(f"| {category} | {flag} | {total} |")
    lines += ["", "| Rule | Kind | Variant | Detected | Seeded |", "|---|---|---|---|---|"]
    for key, (hit, total) in result["cells"].items():
        rule, kind, variant = key.split("|")
        lines.append(f"| {rule} | {kind} | {variant} | {hit} | {total} |")
    if result["check_times"]:
        lines += ["", "| Rule | Assets checked | Median ms | p99 ms |", "|---|---|---|---|"]
        for rule, t in sorted(result["check_times"].items()):
            lines.append(f"| {rule} | {t['assets']} | {t['median_ms']:.4f} | {t['p99_ms']:.4f} |")
    if result["false_positives"]:
        lines += ["", "False positives (every one is analysed in 09):"] + [f"- `{f['asset']}` ({f['category']}, {f['kind']}): {', '.join(f['rules'])}" for f in result["false_positives"][:50]]
    if result["missed"]:
        lines += ["", f"Missed defects (first 50 of {len(result['missed'])}):"] + [f"- `{m['asset']}` {m['rule']} {m['kind']} {m['variant']} `{m['where']}`" for m in result["missed"][:50]]
    return lines + [""]


def main_batch(batch: str) -> int:
    truth = load(os.path.join(RAW_DIR, f"{batch}_groundtruth.json"))
    reports = reports_of(batch)
    baseline = reports.get("F0", [None])[0]
    lines = [f"# Score of batch {batch}", "", f"Ground truth: {truth['assets']} assets, seed {truth['seed']}.", ""]
    results = {}
    for config, runs in reports.items():
        sets = [findings_of(r) for r in runs]
        identical = all(s == sets[0] for s in sets)
        result = score(truth, runs[0], baseline if config != "F0" else None)
        results[config] = result
        lines += markdown(batch, config, result, identical)
    path = os.path.join(SUMMARY_DIR, f"{batch}_score.md")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    with open(os.path.join(SUMMARY_DIR, f"{batch}_score.json"), "w", encoding="utf-8") as handle:
        json.dump(results, handle, indent=1, default=str)
    print(f"score written to {path}")
    return 0


# --- Incremental runs (SC6, 01 section 7.3) ---------------------------------------------------------------------------
# M1-M4 are applied together. The new defects are the findings of a full F1 scan of the mutated corpus that a full F1
# scan of the original corpus did not have, keyed by (asset, rule, where). Each is attributed to the mutation that
# explains it; M4 (a harmless edit) must explain nothing, so anything unexplained is listed.


def finding_keys(report: dict) -> set[tuple[str, str, str]]:
    return {(f["asset"], f["rule"], f["where"]) for f in report.get("findings", []) if f["asset"].startswith(CORPUS_PREFIX)}


def mutation_of(finding: dict) -> str:
    message = finding.get("message", "")
    if (finding["rule"] == "GL.Data.DanglingRow" and ("'T30'" in message or "'T31'" in message)
            and ("DT_TargetsA" in message or "DT_TargetsB" in message)):
        return "M1"   # the rows M1 removed, from the tables it removed them from
    if finding["rule"] == "GL.Data.UnknownTag" and "Fixture.Removable." in message:
        return "M2"   # the tags whose file M2 deleted
    if finding["rule"] == "GL.UB.TickOutsideBudget" and finding["where"] == "Component[M3Ticker]":
        return "M3"   # the component M3 added (no seeded defect uses that name)
    return "unexplained"


def main_incremental(batch: str, original: str) -> int:
    reports = reports_of(batch)
    before = reports_of(original).get("F1", [])
    if not before or "F1" not in reports:
        print("need an F1 run in both batches (the original corpus and the mutated one)")
        return 2
    reference = reports["F1"][0]
    by_key = {(f["asset"], f["rule"], f["where"]): f for f in reference.get("findings", [])}
    new_keys = finding_keys(reference) - finding_keys(before[0])
    gone = finding_keys(before[0]) - finding_keys(reference)
    by_mutation = Counter(mutation_of(by_key[k]) for k in new_keys)
    lines = [f"# Incremental score of batch {batch}", "",
             f"Reference: the full F1 scan of the mutated corpus in this batch, minus the full F1 scan of batch `{original}`.", "",
             f"- New findings after M1-M4: **{len(new_keys)}** ({', '.join(f'{m}: {n}' for m, n in sorted(by_mutation.items())) or 'none'}).",
             f"- Findings that disappeared: {len(gone)}.", "",
             "| Config | Assets validated | New findings found | Recall of new findings | Dependents added | Gathering (ms) |",
             "|---|---|---|---|---|---|"]
    results = {"new": len(new_keys), "by_mutation": dict(by_mutation), "gone": len(gone), "configs": {}}
    for config in sorted(c for c in reports if c.startswith("I")):
        run = reports[config][0]
        found = new_keys & finding_keys(run)
        dependents = run.get("dependents") or {}
        added = sum(d.get("assets_added", 0) for d in dependents.values()) if isinstance(dependents, dict) else 0
        gather_ms = sum(d.get("ms", 0.0) for d in dependents.values()) if isinstance(dependents, dict) else 0.0
        recall = len(found) / len(new_keys) if new_keys else 1.0
        results["configs"][config] = {"validated": run.get("counts", {}).get("checked"), "found": len(found),
                                      "recall": recall, "dependents_added": added, "gather_ms": gather_ms,
                                      "missed": sorted(str(k) for k in new_keys - found)[:50]}
        lines.append(f"| {config} | {run.get('counts', {}).get('checked', 'n/a')} | {len(found)} / {len(new_keys)} | "
                     f"{pct(recall)} | {added} | {gather_ms:.1f} |")
    unexplained = sorted(str(k) for k in new_keys if mutation_of(by_key[k]) == "unexplained")
    lines += ["", "Unexplained new findings (M4 must add none): " + (", ".join(unexplained) if unexplained else "none.")]
    path = os.path.join(SUMMARY_DIR, f"{batch}_incremental.md")
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    with open(os.path.join(SUMMARY_DIR, f"{batch}_incremental.json"), "w", encoding="utf-8", newline="\n") as handle:
        json.dump(results, handle, indent=1)
    print(f"incremental score written to {path}")
    return 0


def selftest() -> int:
    """Hand-checked numbers on a tiny ground truth and report (09: S3 tool check)."""
    assert abs(percentile([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 50) - 5.5) < 1e-9
    assert abs(percentile([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 95) - 9.55) < 1e-9
    g = "/Game/GLSeed/Assets/"
    truth = {"entries": [
        {"asset": g + "A.A", "category": "defect", "rule": "R1", "kind": "k", "variant": "v", "where": ""},
        {"asset": g + "B.B", "category": "defect", "rule": "R2", "kind": "k", "variant": "v", "where": ""},
        {"asset": g + "C.C", "category": "clean", "rule": "", "kind": "", "variant": "", "where": ""},
        {"asset": g + "D.D", "category": "nearmiss", "rule": "", "kind": "n", "variant": "", "where": ""},
        {"asset": g + "E.E", "category": "masked", "rule": "R1", "kind": "k", "variant": "Row", "where": ""},
        {"asset": "/Script/X.Y", "category": "clean", "rule": "", "kind": "", "variant": "", "where": ""},
    ]}
    report = {"findings": [{"asset": g + "A.A", "rule": "R1"}, {"asset": g + "A.A", "rule": "R9"}, {"asset": g + "D.D", "rule": "R2"},
                           {"asset": "/Script/X.Y", "rule": "R1"}],
              "assets": [{"asset": g + "A.A", "result": "Invalid", "errors": ["x"], "warnings": []},
                         {"asset": g + "E.E", "result": "Invalid", "errors": ["y"], "warnings": []}],
              "check_times_ms": {"R1": [1.0, 3.0]}}
    baseline = {"assets": [{"asset": g + "B.B", "result": "Valid", "errors": [], "warnings": ["w"]}]}
    r = score(truth, report, baseline)
    assert (r["detected"], r["defects"]) == (1, 2), r
    assert (r["fp_assets"], r["controls"]) == (1, 2), r                     # D is a near-miss with a finding; /Script ignored
    assert r["wrong_rule"] == [{"asset": g + "A.A", "expected": "R1", "reported": ["R9"]}], r
    assert r["special"]["masked"] == [0, 1], r
    assert r["gap"] == [1, 2], r                                             # A found here, not flagged by the baseline
    assert r["asset_level"]["defect"] == [1, 2], r
    assert abs(r["check_times"]["R1"]["median_ms"] - 2.0) < 1e-9
    print("selftest passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--batch")
    parser.add_argument("--incremental", metavar="ORIGINAL_BATCH",
                        help="score the I configurations of --batch (mutated corpus) against the F1 run of ORIGINAL_BATCH")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    if not args.batch:
        parser.error("--batch or --selftest")
    if args.incremental:
        return main_incremental(args.batch, args.incremental)
    return main_batch(args.batch)


if __name__ == "__main__":
    sys.exit(main())
