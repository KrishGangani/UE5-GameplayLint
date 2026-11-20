"""Run a batch of validation scans on the seeded corpus and keep every artefact (Benchmarks/README.md).

Usage (through the UE job queue; it starts the editor):
    python -I -u Tools/run_scan.py --configs S,F0 [--runs N] [--label NAME] [--all-assets] [--no-prime] [--dry-run]
    python -I -u Tools/run_scan.py --configs I0,I1,I2,I3 --changes GameplayLint/Saved/GLSeed/changes.txt

What one batch does:
  1. Pins this process to the performance cores (pin_self), before anything else, so every editor it starts inherits
     the mask from its first instruction. Records the environment once, including the mask Windows applied.
  2. Copies the corpus's ground truth (GameplayLint/Saved/GLSeed/manifest.json) into the batch, with its hash.
  3. Runs one PRIMING run (discarded), then every configuration --runs times, INTERLEAVED: the order rotates every
     round. --runs defaults to the smallest multiple of the number of configurations that is at least 4.
  4. Starts UnrealEditor-Cmd.exe directly with subprocess.Popen, waits with a timeout and kills the process tree in a
     `finally` block, so exit codes are real and nothing is left running (never `cmd /c start /wait`).
  5. Keeps each run's JSON report and log in Benchmarks/Raw as <batch>_<Config>_N<assets>_r<run>.<ext> and rewrites
     <batch>_manifest.json after every run. Never overwrites raw data.
  6. Writes Benchmarks/Summaries/<batch>_summary.md: validation-phase and process times per configuration (median
     across runs [min-max]) and the asset counts each configuration validated. Detection is scored by score.py.

Configurations (01 section 7.2; 02 section 9). S is the engine's stock commandlet; every other one is -run=GameplayLint
with only the rule set, the dependent mode and the change list switched.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
import time

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS_DIR)
import core_mask  # noqa: E402  (same folder)

ROOT = os.path.dirname(TOOLS_DIR)
PROJECT_DIR = os.path.join(ROOT, "GameplayLint")
UPROJECT = os.path.join(PROJECT_DIR, "GameplayLint.uproject")
EDITOR = r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
ENGINE_BUILD_VERSION = r"C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version"
GROUND_TRUTH = os.path.join(PROJECT_DIR, "Saved", "GLSeed", "manifest.json")
RAW_DIR = os.path.join(ROOT, "Benchmarks", "Raw")
SUMMARY_DIR = os.path.join(ROOT, "Benchmarks", "Summaries")
CORPUS_PATH = "/Game/GLSeed"
MIN_RUNS = 4
RUN_TIMEOUT_SECONDS = 1200   # one scan; a whole queue job stays under 30 minutes by choosing --runs and sizes

COMMON_ARGS = ["-unattended", "-nullrhi", "-nosplash", "-traceautostart=0", "-BUILDMACHINE", "-stdout",
               "-FullStdOutLogOutput"]

# Per configuration: the rule set and the dependent mode. None = the stock commandlet.
CONFIGS = {
    "S": None,
    "F0": ("None", None),
    "F1": ("All", None),
    "F2": ("GL.Data", None),
    "F3": ("GL.UB", None),
    "I0": ("None", None),
    "I1": ("All", "Off"),
    "I2": ("All", "Coarse"),
    "I3": ("All", "Precise"),
}

# Stock per-asset result lines (EditorValidatorSubsystem.cpp:1011-1033), printed when bShowIfNoFailures is set.
RESULT_LINES = {
    "valid": "contains valid data.",
    "valid_with_warnings": "contains valid data, but has warnings.",
    "invalid": "contains invalid data.",
    "not_validated": "has no data validation.",
}
COMMANDLET_TIME = re.compile(r"Execution of commandlet took:\s+(?:.*\()?([0-9.]+) seconds")
# The engine's load warning for a permanent corpus tag means the tag dictionary did not load the corpus's tag files,
# so every tag would count as unknown (it happened once: 09, S1-S4). Such a run is not ok.
PERMANENT_TAG_WARNING = "Invalid GameplayTag Fixture.Tag."


def powershell(command: str) -> str:
    result = subprocess.run(["powershell", "-NoProfile", "-Command", command], capture_output=True, text=True)
    return result.stdout.strip()


def sha256(path: str) -> str:
    with open(path, "rb") as handle:
        return hashlib.sha256(handle.read()).hexdigest()


def environment_record(applied_mask: str) -> dict:
    """Facts that must accompany every result (Benchmarks/README.md, environment record)."""
    with open(ENGINE_BUILD_VERSION, encoding="utf-8") as handle:
        build = json.load(handle)
    return {
        "date": dt.datetime.now().isoformat(timespec="seconds"),
        "engine": f"{build['MajorVersion']}.{build['MinorVersion']}.{build['PatchVersion']} (CL {build['Changelist']})",
        "build_config": "Development editor, UnrealEditor-Cmd.exe in commandlet mode",
        "compiler": "MSVC 14.44.35229, Windows SDK 10.0.22621.0",
        "cpu": powershell("(Get-CimInstance Win32_Processor).Name"),
        "ram_gb": powershell("[math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1GB,1)"),
        "os": powershell("$o=Get-CimInstance Win32_OperatingSystem; $o.Caption + ' build ' + $o.BuildNumber"),
        "power_plan": powershell("powercfg /getactivescheme"),
        "on_ac_power": powershell("(Get-CimInstance Win32_Battery | Select-Object -First 1).BatteryStatus") == "2",
        "cpu_affinity": applied_mask,
        "common_args": COMMON_ARGS,
    }


def command_for(config: str, report: str, log: str, all_assets: bool, changes: str | None) -> list[str]:
    command = [EDITOR, UPROJECT]
    spec = CONFIGS[config]
    if spec is None:
        command.append("-run=DataValidation")
    else:
        rules, dependents = spec
        command += ["-run=GameplayLint", f"-Report={report}", f"-GameplayLintRules={rules}"]
        if dependents:
            command.append(f"-GameplayLintDependents={dependents}")
        if config.startswith("I"):
            command.append(f"-Changes={changes}")   # the engine's changelist path gathers the assets: no -Paths
        elif not all_assets:
            command.append(f"-Paths={CORPUS_PATH}")
    return command + COMMON_ARGS + [f"-abslog={log}"]


def parse_log(path: str) -> dict:
    """What the stock commandlet only prints: per-asset result lines and the commandlet's own duration."""
    counts = {key: 0 for key in RESULT_LINES}
    commandlet_seconds = None
    zen_retry = False
    permanent_tag_warnings = 0
    if os.path.exists(path):
        with open(path, encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if "AssetCheck:" in line:
                    for key, text in RESULT_LINES.items():
                        if line.rstrip().endswith(text):
                            counts[key] += 1
                            break
                match = COMMANDLET_TIME.search(line)
                if match:
                    commandlet_seconds = float(match.group(1))
                if "Re-checking ZenServer readiness" in line:
                    zen_retry = True
                if PERMANENT_TAG_WARNING in line:
                    permanent_tag_warnings += 1
    return {"result_lines": counts, "commandlet_seconds": commandlet_seconds, "zen_retry": zen_retry,
            "permanent_tag_warnings": permanent_tag_warnings}


def run_one(name: str, config: str, args) -> dict:
    report = os.path.join(RAW_DIR, name + ".json")
    log = os.path.join(RAW_DIR, name + ".log")
    for path in (report, log):
        if os.path.exists(path):
            raise FileExistsError(f"refusing to overwrite raw data: {path}")
    command = command_for(config, report, log, args.all_assets, args.changes)
    record = {"name": name, "config": config, "command": command}
    if args.dry_run:
        print("DRY:", " ".join(command), flush=True)
        return record
    print(f"[run_scan] {name}: {' '.join(command)}", flush=True)
    started = time.time()
    process = subprocess.Popen(command)
    try:
        exit_code = process.wait(timeout=RUN_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        exit_code = None
    finally:
        if process.poll() is None:   # timeout, Ctrl+C or any other error: never leave an editor running into the next run
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)], capture_output=True)
    record.update(exit_code=exit_code, wall_seconds=round(time.time() - started, 3), **parse_log(log))
    # Stock: 0, or 1 when validation errors were logged. GameplayLint: 0, or 3 when assets failed (-FailOn=Error).
    record["ok"] = exit_code in ((0, 1) if CONFIGS[config] is None else (0, 3)) and record["permanent_tag_warnings"] == 0
    if os.path.exists(report):
        with open(report, encoding="utf-8") as handle:
            data = json.load(handle)
        record["times_s"] = data.get("times_s")
        record["counts"] = data.get("counts")
        record["findings"] = len(data.get("findings", []))
        record["dependents"] = data.get("dependents")
    print(f"[run_scan] {name}: exit {exit_code}, {record['wall_seconds']} s, ok={record['ok']}", flush=True)
    return record


def write_summary(batch: str, manifest: dict) -> str:
    lines = [f"# Scan batch {batch}", "", f"Corpus: {manifest['corpus']['assets']} assets, seed {manifest['corpus']['seed']}, "
             f"ground truth sha256 `{manifest['corpus']['sha256'][:16]}`. CPU affinity: {manifest['environment']['cpu_affinity']}.", "",
             "Medians across measured runs, [min-max]. The priming run is excluded.", "",
             "| Config | Runs | Validation phase (s) | Process wall (s) | Commandlet (s) | Checked | Invalid | Valid with warnings | GL findings |",
             "|---|---|---|---|---|---|---|---|---|"]

    def stat(values):
        values = [v for v in values if v is not None]
        if not values:
            return "n/a"
        return f"{statistics.median(values):.2f} [{min(values):.2f}-{max(values):.2f}]"

    for config in manifest["configs"]:
        runs = [r for r in manifest["runs"] if r["config"] == config and not r.get("priming")]
        validate = [(r.get("times_s") or {}).get("validate") for r in runs]
        wall = [r.get("wall_seconds") for r in runs]
        commandlet = [r.get("commandlet_seconds") for r in runs]
        first = runs[0] if runs else {}
        # Both commandlets print one result line per asset, so every row counts the same way. (The report's own
        # "warnings" total also counts invalid assets that have warnings: not comparable with the stock run.)
        rl = first.get("result_lines") or {}
        counts = {"checked": sum(rl.values()), "invalid": rl["invalid"], "warnings": rl["valid_with_warnings"]} if rl else {}
        lines.append(f"| {config} | {len(runs)} | {stat(validate)} | {stat(wall)} | {stat(commandlet)} | "
                     f"{counts.get('checked', 'n/a')} | {counts.get('invalid', 'n/a')} | {counts.get('warnings', 'n/a')} | "
                     f"{first.get('findings', 'n/a')} |")
    failed = [r["name"] for r in manifest["runs"] if not r.get("ok", True)]
    lines += ["", f"Runs not ok (unexpected exit code, or a permanent corpus tag reported invalid): {', '.join(failed) if failed else 'none'}.",
              "Detection and false positives: `python -I Tools/score.py --batch " + batch + "`."]
    path = os.path.join(SUMMARY_DIR, batch + "_summary.md")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--configs", required=True, help="comma-separated, e.g. S,F0 or F0,F1,F2,F3")
    parser.add_argument("--runs", type=int, default=0, help="runs per configuration; 0 = smallest multiple of the "
                        f"number of configurations that is at least {MIN_RUNS}")
    parser.add_argument("--label", default="")
    parser.add_argument("--all-assets", action="store_true", help="no -Paths filter (needed to compare with S)")
    parser.add_argument("--changes", help="pseudo-changelist for the I configurations")
    parser.add_argument("--no-prime", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    configs = args.configs.split(",")
    unknown = [c for c in configs if c not in CONFIGS]
    if unknown:
        parser.error(f"unknown configurations: {unknown}")
    if any(c.startswith("I") for c in configs) and not args.changes:
        parser.error("I configurations need --changes")
    if args.changes:
        args.changes = os.path.abspath(args.changes)
    if args.runs == 0:
        args.runs = math.ceil(MIN_RUNS / len(configs)) * len(configs)
    if args.runs % len(configs):
        print(f"WARNING: --runs {args.runs} is not a multiple of {len(configs)} configs; the rotation is unbalanced")

    # Pin first: every editor started from here on inherits the performance-core mask (core_mask.py).
    mask = core_mask.performance_core_mask()
    if mask and not args.dry_run:
        core_mask.pin_self(mask)
        core_mask.KERNEL32.SetThreadExecutionState(0x80000000 | 0x00000001)   # ES_CONTINUOUS | ES_SYSTEM_REQUIRED: no sleep
    applied = core_mask.applied_mask()

    if not os.path.exists(GROUND_TRUTH):
        print(f"no corpus: {GROUND_TRUTH} is missing (Tools/seed.py seed)", file=sys.stderr)
        return 2
    with open(GROUND_TRUTH, encoding="utf-8") as handle:
        truth = json.load(handle)
    os.makedirs(RAW_DIR, exist_ok=True)
    os.makedirs(SUMMARY_DIR, exist_ok=True)
    batch = dt.datetime.now().strftime("%Y-%m-%d_%H%M%S") + (f"_{args.label}" if args.label else "")
    assets = truth["assets"]
    truth_copy = os.path.join(RAW_DIR, f"{batch}_groundtruth.json")
    if not args.dry_run:
        shutil.copyfile(GROUND_TRUTH, truth_copy)
    manifest = {
        "batch": batch, "configs": configs, "runs_per_config": args.runs, "all_assets": args.all_assets,
        "changes": args.changes, "environment": environment_record(applied) if not args.dry_run else {"cpu_affinity": applied},
        "corpus": {"assets": assets, "seed": truth["seed"], "sha256": sha256(GROUND_TRUTH), "ground_truth": os.path.basename(truth_copy)},
        "runs": [],
    }
    manifest_path = os.path.join(RAW_DIR, f"{batch}_manifest.json")

    def save():
        if not args.dry_run:
            with open(manifest_path, "w", encoding="utf-8") as handle:
                json.dump(manifest, handle, indent=1)

    if not args.no_prime:
        record = run_one(f"{batch}_{configs[0]}_N{assets}_r0_prime", configs[0], args)
        record["priming"] = True
        manifest["runs"].append(record)
        save()
    for round_index in range(args.runs):
        rotation = round_index % len(configs)
        for config in configs[rotation:] + configs[:rotation]:   # interleave: rotate the order every round
            manifest["runs"].append(run_one(f"{batch}_{config}_N{assets}_r{round_index + 1}", config, args))
            save()

    if args.dry_run:
        return 0
    summary = write_summary(batch, manifest)
    print(f"[run_scan] batch {batch}: manifest {manifest_path}, summary {summary}", flush=True)
    return 0 if all(r.get("ok") for r in manifest["runs"]) else 1


if __name__ == "__main__":
    sys.exit(main())
