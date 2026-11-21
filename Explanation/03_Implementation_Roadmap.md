# 03 · Implementation Roadmap: GameplayLint

**Written:** end of Phase 3, 2026-11-21 · **Engine:** UE 5.8.4

**Rules for every step:**
- Each step is small enough to read in one sitting: a few hundred lines at most (Hard rule 6).
- A step is finished only when its code **compiles with 0 warnings through the UE job queue**, its check **passes**, and its `Explanation/` entries (`05`, `06`, and `07` where relevant) are **written in the same step** (Hard rule 13).
- After each step that adds C++: run `simplify`, then `code-review`.
- **The baseline is captured before any rule exists.** Steps S1–S3 build only the corpus, the harness and the report path, then measure the stock engine on the corpus. The first rule arrives in S5.
- Every Unreal job (build, commandlet, package, timed run) goes through the queue. Timed runs are pinned to the P-cores (`01` §8).

---

**Progress (2026-11-21):** Phases 1–3 ✅. **Phase 4 ✅** (F1–F3; checks F-1 to F-6 and T-1 in `09`). **S1–S4 ✅** (one commit: they were built, seeded, measured and tested as one queue chain; `09` P5-1 to P5-3). The stock baseline was captured before any rule code: S = F0, and the stock run flags 28 of 200 seeded defects, all of them unknown tags. **S5 ✅** (`09` P5-4): UnknownTag 28/28, DanglingRow 64/64, 0 false positives. **S6 ✅** (`09` P5-5): Curve 33/33; the data rules find 125/125 with 0 false positives. **S7 ✅** (`09` P5-6, P5-7, E1): all six rules find 200/200 with 0 false positives; the pack changed 0 core lines. Next: **S8**.

## Phase 4: Foundation (no rule code yet) ✅

| Step | What gets built | How it is checked |
|---|---|---|
| **F1** | `GameplayLint\GameplayLint.uproject` (EngineAssociation 5.8; the default-on Android File Server plugin disabled, `05` A1), `Source\GameplayLint.Target.cs`, `GameplayLintEditor.Target.cs`, the runtime game module `GameplayLint` and the editor module `GameplayLintBench` (`Build.cs`, module `.cpp`) | `Tools\build.bat` builds the editor target from the command line through the queue: 0 errors, 0 warnings |
| **F2** | The three plugin skeletons: `GameplayLintCore` (lists `DataValidation`), `GameplayLintDataRules` and `GameplayLintUpdateBudget` (each lists `GameplayLintCore`), each with `.uplugin`, `Build.cs` and module `.cpp` | Same build: 0 warnings proves the plugin-dependency wiring (UBT warns otherwise, `01b` §4). The editor log mounts all three plugins |
| **F3** | `Config\DefaultEngine.ini` (default maps, renderer), `DefaultGame.ini`, `DefaultInput.ini` (Enhanced Input classes); `Tools\build.bat`, `package.bat`, `smoke_game.bat`, `validate.bat`, `core_mask.py` | Game target builds with 0/0 and compiles **no** lint module; the stock `-run=DataValidation` commandlet runs on the project (records the default validator set); a packaged Development build cooks with 0/0 and its smoke run exits 0 with 0 warnings |

## Phase 5: Implementation

| Step | What gets built | How it is checked | Explanation files touched |
|---|---|---|---|
| **S1. Fixture types** | Game module: `UGLFixtureDataAsset`, `FGLFixturePayload`, `FGLFixtureRow`, `UGLFixtureInstanced`, `AGLFixtureAgent`, `AGLFixtureTickingAgent`, `UGLFixtureBudgetClient`, `UGLFixtureTicker` (`02` §4.4), with the `LintRequired` / `LintMonotonic` annotations; `GameplayTags` dependency | Editor and game targets build 0/0 (confirms UHT accepts the custom metadata keys, `01b` §3); one object of each type is created in a smoke commandlet run without errors | 04, 05, 06 |
| **S2. Seeded corpus** | `UGameplayLintSeedCommandlet` (`-Seed`, `-Assets`, `-Out`, `-Manifest`): the defect kinds, variants, near-misses, held-out kinds and masking set of `01` §7.1; `Config\Tags\` with a permanent and a removable tag file | **E2:** seeding twice with the same seed gives byte-identical manifests (hash), a different seed gives a different one. The editor opens a seeded asset of each type. The corpus folder is git-ignored | 05, 06, 07, 09 |
| **S3. Harness + the baseline** | Core: `FGameplayLintFinding`, `UGameplayLintReportSubsystem`, `UGameplayLintSettings` (rule switch, command-line overrides), `UGameplayLintCommandlet` (`-Paths`, `-Report`, `-FailOn`; per-asset details, per-validator totals, phase timings). `Tools\run_scan.py` (pinned, interleaved, priming run, batch manifest, environment record), `Tools\score.py` (recall and false positives vs ground truth, per rule, kind and variant) | `score.py --selftest` on a hand-made report. **Baseline capture:** the stock `-run=DataValidation` and F0 on the N = 1,000 corpus, 4 interleaved runs each (two rotations) plus priming. The full F0 timing at all three sizes is part of the Phase 8 matrix, interleaved with F1–F3. F0 must report the same "Files Checked / Failed" as stock (`02` §9). **H2, H3, H5 and the stock side of SC3 are answered with data before any rule code** | 05, 06, 08, 09 |
| **S4. Rule base + walker** | `UGameplayLintRule` (the engine overrides, pass/fail contract, timer, report), `FGameplayLintContext`, the walker (`02` §4.1) | Automation tests on in-memory objects: the walker finds a value in every structural variant with the right path; a rule that reports nothing passes, one that reports an error fails, and no `ensure` fires in either case | 04, 05, 06, 07 |
| **S5. Data rules I** | `GL.Data.UnknownTag`, `GL.Data.DanglingRow` (no dependents yet) | First F1 runs at N = 1,000: SC1/SC2 checked for these two rules before going on | 05, 06, 07 |
| **S6. Data rules II** | `GL.Data.Curve` (non-finite always; `LintMonotonic`, `LintRequired`) | Same, for the curve kinds; near-miss "flat curve" must pass | 05, 06, 07 |
| **S7. UpdateBudget pack** | `UGameplayLintUpdateBudgetSettings` and the three `GL.UB.*` rules; the project config points them at the fixture classes | **E1:** the commit adding the pack changes 0 lines under `Plugins/GameplayLintCore/`. SC1/SC2 for the `GL.UB.*` kinds | 05, 06, 07 |
| **S8. Incremental** | `GatherDependents` in all rules (precise and coarse), the commandlet's `-Changes=` pseudo-changelist, `-GameplayLintDependents=`, the bench commandlet's `-Mutate=M1..M4` | M1–M4 at N = 1,000: I3 finds what a full F1 scan finds after the same change (SC6); I0 and I1 recorded; M4 adds nothing | 05, 06, 07, 09 |
| **S9. (Optional) Fixer** | A fixer on `GL.UB.ClientTickGroup` that sets the scheduler's tick group, built with the engine's fixer helpers | Clicking it in the editor fixes the template and the asset then validates clean (checked with the `run` skill) | 05, 06 |
| **S10. (Optional, last) Cook-reachability rule** | UE5-UpdateBudget's "asset referenced only from C++ isn't cooked" (`12`), only after S1–S8 work. Ground truth: a packaged build's cooked file list | If time runs short, it moves to `12` with its design (owner's decision, 2026-11-21) | 05, 06, 07, 12 |

## Phases 6–10 (after the code exists)

| Phase | Work |
|---|---|
| 6 Explanation | Consolidate 04–07 into one consistent line-by-line pass. Check every API against 5.8.4 again |
| 7 Testing | Edge cases: an empty project; an asset whose `IsDataValid` fails (masking); a settings class path that doesn't resolve; a Blueprint that fails to compile; a 10,000-row DataTable; redirectors and renamed tables; a deleted table (engine path) vs a modified one (ours). Stability: the N = 5,000 corpus validated three times in one process |
| 8 Profiling | The matrix: F0–F3 × N = 500 / 1,000 / 5,000 × 4 interleaved runs + priming (the N = 5,000 rounds split over two queue jobs, two rounds each, so no job exceeds 30 minutes); I0–I3 × 4 runs at N = 5,000 on one mutated corpus (M1–M4 applied together; validation never modifies it, so it needs no restore between runs); one F1 correctness run on the mutated corpus (the SC6 reference). Where the time goes (load vs rule vs dependents). Optimisations only with data (`11`) |
| 9 Code review | The `code-review` and `simplify` skills, plus a written self-review: weaknesses, not praise |
| 10 Portfolio | Charts (`dataviz`), an architecture page, the demo video, README results, interview Q&A |

**Budget (owner's cap, 2026-11-21):** every scan job ≤ 30 minutes, the whole Phase 8 measurement ≤ about 1.5 hours of queue time. **Rough cost [estimate, re-measured in S3]:** 48 full-scan runs at 0.5–2.5 minutes (≈ 50 minutes), 16 incremental runs (≈ 10 minutes), three seedings and the reference run (≈ 15 minutes). **S3 measured** about 9.3 s per scan process at N = 1,000, of which the validation phase is under 1 s, and about 28 s per seeding of 1,000 assets. The Phase 8 matrix is therefore far inside the budget; N = 5,000 is not measured yet. If S3 shows the estimate is wrong, the plan is cut (fewer sizes for F2/F3 first), never the run rule.
