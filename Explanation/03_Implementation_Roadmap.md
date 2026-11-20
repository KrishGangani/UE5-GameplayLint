# 03 · Implementation Roadmap: GameplayLint

**Written:** end of Phase 3, 2026-11-21 · **Engine:** UE 5.8.4

**Rules for every step:**
- Each step is small enough to read in one sitting: a few hundred lines at most (Hard rule 6).
- A step is finished only when its code **compiles with 0 warnings through the UE job queue**, its check **passes**, and its `Explanation/` entries (`05`, `06`, and `07` where relevant) are **written in the same step** (Hard rule 13).
- After each step that adds C++: run `simplify`, then `code-review`.
- **The baseline is captured before any rule exists.** Steps S1–S3 build only the corpus, the harness and the report path, then measure the stock engine on the corpus. The first rule arrives in S5.
- Every Unreal job (build, commandlet, package, timed run) goes through the queue. Timed runs are pinned to the P-cores (`01` §8).

---

**Progress (2026-11-21):** Phases 1–3 ✅. **Phase 4 ✅** (F1–F3; checks F-1 to F-6 and T-1 in `09`, all passing). Next: **S1** (fixture types).

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
| **S3. Harness + the baseline** | Core: `FGameplayLintFinding`, `UGameplayLintReportSubsystem`, `UGameplayLintSettings` (rule switch, command-line overrides), `UGameplayLintCommandlet` (`-Paths`, `-Report`, `-FailOn`; per-asset details, per-validator totals, phase timings). `Tools\run_scan.py` (pinned, interleaved, priming run, batch manifest, environment record), `Tools\score.py` (recall and false positives vs ground truth, per rule, kind and variant) | `score.py --selftest` on a hand-made report. **Baseline capture:** the stock `-run=DataValidation` and F0 on the corpus at N = 1,000 / 4,000 / 10,000, 4 interleaved runs each (two rotations) plus priming. F0 must report the same "Files Checked / Failed" as stock (`02` §9). **H2, H3, H5 and the stock side of SC3 are answered with data before any rule code** | 05, 06, 08, 09 |
| **S4. Rule base + walker** | `UGameplayLintRule` (the engine overrides, pass/fail contract, timer, report), `FGameplayLintContext`, the walker (`02` §4.1) | Automation tests on in-memory objects: the walker finds a value in every structural variant with the right path; a rule that reports nothing passes, one that reports an error fails, and no `ensure` fires in either case | 04, 05, 06, 07 |
| **S5. Data rules I** | `GL.Data.UnknownTag`, `GL.Data.DanglingRow` (no dependents yet) | First F1 runs at N = 1,000: SC1/SC2 checked for these two rules before going on | 05, 06, 07 |
| **S6. Data rules II** | `GL.Data.Curve` (non-finite always; `LintMonotonic`, `LintRequired`) | Same, for the curve kinds; near-miss "flat curve" must pass | 05, 06, 07 |
| **S7. UpdateBudget pack** | `UGameplayLintUpdateBudgetSettings` and the three `GL.UB.*` rules; the project config points them at the fixture classes | **E1:** the commit adding the pack changes 0 lines under `Plugins/GameplayLintCore/`. SC1/SC2 for the `GL.UB.*` kinds | 05, 06, 07 |
| **S8. Incremental** | `GatherDependents` in all rules (precise and coarse), the commandlet's `-Changes=` pseudo-changelist, `-GameplayLintDependents=`, the bench commandlet's `-Mutate=M1..M4` | M1–M4 at N = 1,000: I3 finds what a full F1 scan finds after the same change (SC6); I0 and I1 recorded; M4 adds nothing | 05, 06, 07, 09 |
| **S9. (Optional) Fixer** | A fixer on `GL.UB.ClientTickGroup` that sets the scheduler's tick group, built with the engine's fixer helpers | Clicking it in the editor fixes the template and the asset then validates clean (checked with the `run` skill) | 05, 06 |

## Phases 6–10 (after the code exists)

| Phase | Work |
|---|---|
| 6 Explanation | Consolidate 04–07 into one consistent line-by-line pass. Check every API against 5.8.4 again |
| 7 Testing | Edge cases: an empty project; an asset whose `IsDataValid` fails (masking); a settings class path that doesn't resolve; a Blueprint that fails to compile; a 10,000-row DataTable; redirectors and renamed tables; a deleted table (engine path) vs a modified one (ours). Stability: the full N = 10,000 corpus validated three times in one process |
| 8 Profiling | The matrix: F0–F3 × N = 1,000 / 4,000 / 10,000 × 4 interleaved runs + priming; I0–I3 × M1–M4 × N = 4,000 / 10,000 × 4 runs, each on a freshly restored corpus; one F1 correctness run per mutated corpus (the SC6 reference). Where the time goes (load vs rule vs dependents). Optimisations only with data (`11`) |
| 9 Code review | The `code-review` and `simplify` skills, plus a written self-review: weaknesses, not praise |
| 10 Portfolio | Charts (`dataviz`), an architecture page, the demo video, README results, interview Q&A |

**Rough matrix cost [estimate, measured in S3]:** about 48 full-scan runs at 0.5–4 minutes and about 128 incremental runs at under a minute each: 3–4 hours, scripted and unattended, in the queue.
