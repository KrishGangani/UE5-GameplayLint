"""Create, check or mutate the seeded test corpus (01 section 7.1) through the bench commandlet.

Run it through the UE job queue (it starts the editor):
  python -I -u Tools/seed.py seed --seed 1 --assets 1000      delete /Game/GLSeed and its tag files, then seed afresh
  python -I -u Tools/seed.py determinism --assets 500          E2: seed 2 once, seed 1 twice; compare manifest hashes
  python -I -u Tools/seed.py mutate                            apply M1-M4 to the current corpus; write changes.txt

Outputs (git-ignored): GameplayLint/Content/GLSeed/, GameplayLint/Config/Tags/GLSeed*.ini,
GameplayLint/Saved/GLSeed/manifest.json (ground truth) and changes.txt (the pseudo-changelist after a mutation).
Exit code: 0 on success, the editor's exit code otherwise, 3 if the determinism check fails.
"""
import argparse
import glob
import hashlib
import os
import shutil
import subprocess
import sys
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.normpath(os.path.join(TOOLS, "..", "GameplayLint"))
UPROJECT = os.path.join(PROJECT_DIR, "GameplayLint.uproject")
EDITOR = r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
CORPUS_DIR = os.path.join(PROJECT_DIR, "Content", "GLSeed")
TAG_FILES = os.path.join(PROJECT_DIR, "Config", "Tags", "GLSeed*.ini")
OUT_DIR = os.path.join(PROJECT_DIR, "Saved", "GLSeed")
MANIFEST = os.path.join(OUT_DIR, "manifest.json")
CHANGES = os.path.join(OUT_DIR, "changes.txt")
TIMEOUT_SECONDS = 1500   # well inside the queue's 30-minute rule for one job


def run_editor(args: list[str], log_name: str) -> int:
    """Runs the bench commandlet. Output goes to our stdout, so the queue's idle watchdog sees progress."""
    command = [EDITOR, UPROJECT, "-run=GameplayLintSeed", *args, "-unattended", "-nullrhi", "-nosplash",
               "-traceautostart=0", "-stdout", "-FullStdOutLogOutput",
               f"-abslog={os.path.join(OUT_DIR, log_name)}"]
    print("RUN:", " ".join(command), flush=True)
    started = time.time()
    process = subprocess.Popen(command)
    try:
        code = process.wait(timeout=TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        code = 124
    finally:
        if process.poll() is None:   # timeout or interruption: never leave an editor running
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)], capture_output=True)
    print(f"EXIT {code} after {time.time() - started:.1f} s", flush=True)
    return code


def seed(seed_value: int, assets: int) -> int:
    """Deletes the old corpus (the commandlet refuses to seed into a non-empty folder) and seeds a new one."""
    os.makedirs(OUT_DIR, exist_ok=True)
    shutil.rmtree(CORPUS_DIR, ignore_errors=True)
    for path in glob.glob(TAG_FILES):
        os.remove(path)
    if os.path.exists(MANIFEST):
        os.remove(MANIFEST)
    code = run_editor([f"-Seed={seed_value}", f"-Assets={assets}", f"-Manifest={MANIFEST}"], "Seed.log")
    if code == 0:
        print(f"MANIFEST {MANIFEST} sha256={sha256(MANIFEST)}", flush=True)
    return code


def sha256(path: str) -> str:
    with open(path, "rb") as handle:
        return hashlib.sha256(handle.read()).hexdigest()


def determinism(assets: int) -> int:
    """E2 (01 section 7.4): the same seed must give a byte-identical manifest, a different seed a different one.
    Seed 2 runs first, so the corpus left behind is the seed-1 corpus that the next steps use."""
    hashes = {}
    for label, seed_value in (("seed2", 2), ("seed1_a", 1), ("seed1_b", 1)):
        code = seed(seed_value, assets)
        if code != 0:
            return code
        hashes[label] = sha256(MANIFEST)
    same = hashes["seed1_a"] == hashes["seed1_b"]
    different = hashes["seed1_a"] != hashes["seed2"]
    print(f"E2 seed 1 twice identical: {same}; seed 2 differs: {different}; {hashes}", flush=True)
    return 0 if same and different else 3


def mutate() -> int:
    if os.path.exists(CHANGES):
        os.remove(CHANGES)
    code = run_editor(["-Mutate", f"-Changes={CHANGES}"], "Mutate.log")
    if code == 0:
        with open(CHANGES, encoding="utf-8-sig") as handle:
            print("CHANGES:\n" + handle.read(), flush=True)
    return code


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    seed_parser = sub.add_parser("seed")
    seed_parser.add_argument("--seed", type=int, default=1)
    seed_parser.add_argument("--assets", type=int, default=1000)
    determinism_parser = sub.add_parser("determinism")
    determinism_parser.add_argument("--assets", type=int, default=500)
    sub.add_parser("mutate")
    args = parser.parse_args()
    if args.command == "seed":
        return seed(args.seed, args.assets)
    if args.command == "determinism":
        return determinism(args.assets)
    return mutate()


if __name__ == "__main__":
    sys.exit(main())
