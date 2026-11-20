# 01 · Problem Analysis: GameplayLint

**Phase:** 1 (Problem Analysis), written 2026-11-21 together with Phase 2, whose verified findings it uses ([01b_Research_Findings.md](01b_Research_Findings.md)).
**Engine:** UE **5.8.4** (CL 58902052), launcher build.
**Status of every claim:** **[verified]** (read in engine source or a cited page), **[hypothesis]** (to be confirmed by measurement), or **[target]** (a pass/fail threshold, not a result).

---

## 1. The problem in one paragraph

Gameplay data is a web of references. An ability names gameplay tags; a loot entry points at a row in a DataTable; a cooldown is a curve that must rise with level; an enemy Blueprint promises, by the project's own rules, to run its per-frame logic through an update budget. Unreal checks a lot of things about a single asset. It checks much less about whether the **relation** between two assets still holds after one of them changes. A designer removes a row, merges a tags `.ini`, or adds a ticking component to a parent Blueprint, and the asset that is now broken is not the one that changed, so nothing re-checks it. The break shows up later as a silent `false` from a tag query, a "row not found" line in a playtest log, or a frame-time regression that someone has to profile back to its cause.

## 2. Where this happens in real development

| Evidence | What it shows | Source |
|---|---|---|
| Assassin's Creed Origins (Ubisoft Montréal, GDC 2018) | In a huge open world it was "practically impossible" to validate world data in time by traditional means, made worse by **dependencies between AI, art and design data**. Their answer: daily automated tests and reports that track errors and regressions | [research/02](Research/02_technical_challenges.md) [T8-A] (abstract) |
| Embark Studios, Unreal Fest 2022 | Build stability through alerts, tests and Data Validation | [research/02](Research/02_technical_challenges.md) [T8-D] (snippet) |
| Respawn (Apex) job postings, 2026 | "automated content validation"; a tools role that must "measure the impact" of its tools | [research/01](Research/01_job_market.md) §2 (EA2, EA4) |
| Job market tally | Tools and developer productivity in 33 of 57 postings, 9 of 10 lead roles | [research/01](Research/01_job_market.md) §4–5 |
| Epic's own engine | The Gameplay Tags editor refuses to delete a tag that the asset registry says is still referenced (`GameplayTagsEditorModule.cpp:682–690`). Epic guards the UI path; hand-edited or merged `.ini` files, disabled tag-source plugins and text imports bypass it | [verified] `01b` §2 |
| This portfolio | UE5-UpdateBudget's documented mistakes include data-level ones: a budget client left in the default tick group, a component whose tick never starts, an asset referenced only from code that the cooker left out | UE5-UpdateBudget `07_Common_Mistakes.md` (§4 below) |

**What these share:** teams already run validation. The failures that hurt are relations between assets, and changes whose victim is somewhere else.

## 3. What already exists, and what it leaves open

Phase 2 read the Data Validation plugin and the related engine code in 5.8.4. It is a mature framework, and this project builds on it rather than around it.

| Existing tool | What it does | What it doesn't do | Evidence |
|---|---|---|---|
| **Data Validation plugin** (`UEditorValidatorBase`, `UEditorValidatorSubsystem`) | Runs `IsDataValid` and every validator class on save, from menus, before submit, from a commandlet and from script. Discovers C++ validators by reflection, Blueprint ones through the registry | Ships no rule about tags, row handles or curves in user-defined data. The class-level `IsDataValid` runs first, and when it fails, validators are skipped for that asset | [verified] `01b` §1–2 |
| **Changelist (incremental) validation** | Validates assets in modified packages, plus referencers of **deleted** packages, plus whatever each validator returns from `GetAssetsToValidateFromChangelist` | Does **not** re-validate referencers of a *modified* asset or of a changed non-asset file (a tags `.ini`). The hook exists; no shipped validator uses it for this | [verified] `EditorValidatorSubsystem.cpp:1461–1516`; `01b` §2 |
| **`DataValidation` commandlet** | `-run=DataValidation` with `-AssetType=`, `-IncludeOnlyOnDiskAssets`, `-includeengine`. In 5.8 its steps are virtual, so it can be subclassed | No path filter, no changelist input, no report file. Its exit code means "something logged an error", not "these assets failed" | [verified] `DataValidationCommandlet.cpp:146–151, 227–231`; `LaunchEngineLoop.cpp:4217–4224` |
| **Fixers** (`IFixer`, `DataValidationFixers.h`) | One-click repairs attached to a validation message | Applied only from the Message Log UI; there is no apply-in-CI path | [verified] `01b` §2 |
| **Gameplay Tags load warning** | Loading an asset that holds an unknown tag logs "Invalid GameplayTag … found in property …" in editor builds. The stock commandlet captures load logs, so a freshly loaded asset reports it as a warning | Only when the asset is *loaded* during validation. Validate-on-save uses already-loaded assets and doesn't reload them. No rule name and no severity control | [verified] `GameplayTagsManager.cpp:1076, 1123`; `EditorValidatorSubsystem.cpp:906–937, 1167` |
| **Class-specific checks** | GAS validates `FScalableFloat` curve-table rows; Smart Object definitions and StateTrees validate themselves; `UDataTable` asks each row struct to validate itself | Only inside those classes. A `FDataTableRowHandle` or a tag inside a project struct has no owner that checks it | [verified] `ScalableFloat.cpp:22–38`; `DataTable.cpp:418–462` |
| **Asset Referencing Restrictions** (off by default) | Folder and plugin reference domains, with a fixer | Structure (who may reference whom), not meaning | [verified] `01b` §5 |
| **Linter v2** (Gamemakin) | Rule sets as data assets, chosen by asset class; naming and Marketplace rules; its own commandlet with JSON/HTML reports and exit codes 0/1/2 | Style and structure, not gameplay relations. A parallel framework with its own runner | docs opened, `01b` §5 |
| **CommonValidators** (community) | Blocking loads, empty Event Tick (before 5.6), heavy references, pure-node reuse, texture sampler source | Blueprint hygiene, not data relations; runs on save | README opened, `01b` §5 |
| **Lyra's validators** (Epic sample) | A project validator base, Blueprint/load/source-control checks, "Check Content" before submit | No gameplay-meaning rules in the walkthrough reviewed | `01b` §5 |

**The gap, after Phase 2 (medium, and narrower than the roadmap said):**
- The framework gap is **none**: the engine has discovery, an incremental hook, fixers, per-validator timing and an extensible commandlet.
- Part of the semantic gap is **already covered**: unknown tags show up as load warnings when assets are freshly loaded, and GAS, Smart Objects and StateTree check their own references.
- What remains:
  1. **Generic** relation rules across user-defined data types: tags, row handles and curves inside any struct, array, map, instanced object, DataTable row or Blueprint default.
  2. **Incremental re-validation of the victims** of a change, using the registry's searchable-name dependencies (the hook exists, nothing uses it for this).
  3. **Project-contract rules** taken from a team's documented mistakes (here: UE5-UpdateBudget's).
  4. **Measurement:** no UE validation tool reviewed publishes a detection rate, a false-positive rate or scan costs.

We present GameplayLint as rule packs on Epic's framework, never as a new validation system. A web search cannot prove that no studio has this internally, so the gap statement is about what was reviewed.

## 4. The first rule pack comes from UE5-UpdateBudget's documented mistakes

The roadmap says the rules encode the lessons of Projects 1–9. Only UE5-UpdateBudget exists today. Its `07_Common_Mistakes.md` lists 40 mistakes. Most are **code** mistakes, which a data rule cannot see: they belong to code review and to the mutation tests of a later portfolio project (UE5-TestTrust). The ones that live in **data** become rules:

| UE5-UpdateBudget mistake or plan | Lives in | Becomes |
|---|---|---|
| "Leaving the default `TG_DuringPhysics`": baselines and scheduler run at different frame positions | Data: a component template's Tick Group can be changed in any Blueprint (`EngineBaseTypes.h:195–196`, `EditDefaultsOnly`) | **`GL.UB.ClientTickGroup`**: a budget client whose tick group differs from the scheduler's |
| "Forgetting `bCanEverTick = true`": the native path never updates | Data, partly: `bCanEverTick` isn't editable, but "Start with Tick Enabled" is (`EngineBaseTypes.h:212–217`) | **`GL.UB.ClientNeverTicks`**: a budget client whose tick can never start |
| The project's own plan: "P10 adds a validator for 'ticking actor not registered with the budget'" (UE5-UpdateBudget `02` §11), and the problem it solves: per-frame ticks cost the same whether they matter or not | Data: actor and component tick settings on Blueprint templates and class defaults | **`GL.UB.TickOutsideBudget`**: an actor in the budget's scope that still ticks every frame through another path |
| "An asset referenced only from C++ isn't cooked": invisible agents in packaged builds | Cook rules plus native class defaults: a project-level relation, and its ground truth is the cooker's output | **Deferred** to a later step (`12`): it needs a packaged build as ground truth, which the seeded per-asset corpus can't provide |
| Two clocks, swap-remove null entries, `Deinitialize` cleanup, Significance Manager unregistering, seeded random streams, optimised-away workloads, `SetComponentTickInterval` vs `…AndCooldown` | C++ code | **Out of scope** for a data linter (stated honestly) |

**General rules** come from the roadmap's problem statement and from Phase 2: **`GL.Data.UnknownTag`**, **`GL.Data.DanglingRow`** (DataTable and CurveTable handles) and **`GL.Data.Curve`** (non-finite keys always, monotonicity and "required" only where the property is annotated). "Smart Object slots without matching animations" is left to the rule pack of the AI project (UE5-SquadQuery), whose data it is.

## 5. Root-cause hypotheses

Each hypothesis has a test, and the baseline runs come before any rule code.

| # | Hypothesis | Why we suspect it | How we test it |
|---|---|---|---|
| H1 | **The victim of a change is not re-validated.** Incremental validation checks the changed asset, not the unchanged assets that depend on it | [verified] in code: referencers are added only for *deleted* packages (`EditorValidatorSubsystem.cpp:1492–1516`); a tags `.ini` is not an asset | Mutation scenarios M1–M3 (§7.1): stock incremental run I0 vs a full scan after the same change |
| H2 | **Generic data has no owner who checks it.** Engine checks live in the classes that own the data (GAS, Smart Objects, StateTree); a tag or handle inside a project struct is nobody's job | [verified] in code: no engine `IsDataValid` or validator checks handles or tags generically (`01b` §1) | Stock full scan F0: asset-level detection per defect class and per structural variant |
| H3 | **The engine's tag warning depends on how the asset was loaded.** It fires during a fresh load, so a full commandlet run sees it, but validate-on-save does not reload | [verified] in code: on-save uses `bLoadAssetsForValidation = false` (`EditorValidatorSubsystem.cpp:1167`) | Validate already-loaded assets with the script entry point (usecase Save) vs a fresh-load commandlet run, on the same unknown-tag defects |
| H4 | **Project contracts are invisible to generic validation.** "Per-frame work goes through the budget" exists only in one project's docs | Nothing in the engine knows the contract | Stock F0 detection on the `GL.UB.*` defects (expected: none) |
| H5 | **The stock CI signal is too coarse to gate on.** Warnings don't change the exit code, messages have no stable IDs, and any unrelated logged error fails the run | [verified] in code: `ProcessValidationResults` always succeeds; the exit code is "errors were logged" (`01b` §2) | Exit code of F0 on a corpus whose only defects are warning-level; whether F0's output can be mapped to defect classes without guessing |
| H6 | **Loading dominates validation time**, so meaning-level rules are cheap on top of it, but full scans stay too slow to run on every change | [verified] in code: every asset is loaded before validators run (`EditorValidatorSubsystem.cpp:903–938`) | Per-validator totals and per-asset rule time vs validation-phase wall time, F0 vs F1 at three sizes |

**Not a cause we accept without checking:** "people just don't run validation". The tooling runs on save by default (`DataValidationSettings.h:24–26`). The question is what it checks.

## 6. The cost of leaving it unsolved

- **Silent wrong behaviour.** An unknown tag is still "valid" by `FGameplayTag::IsValid()`, which only tests for `None` (`GameplayTagContainer.h:138–141`). Tag queries simply return false: an ability that never triggers, an immunity that never applies. Nothing crashes, so it survives until someone plays that exact case.
- **Late, noisy discovery.** A dangling row handle logs a warning only when `GetRow` runs at run time (`DataTable.h:457`). It is found in a playtest log, by whoever happens to read it.
- **Performance contracts erode.** In UE5-UpdateBudget, every per-frame tick outside the budget is cost the budget can't control. One added component in a parent Blueprint puts it on every child.
- **Teams lose trust in validation** when it is noisy or vague. A tool with false positives gets switched off. That is why false positives are a headline metric here, not a footnote.

## 7. What "solved" means: success criteria, defined now

### 7.1 The workload: a seeded corpus, generated by script

- **Generator:** an editor commandlet (`-run=GameplayLintSeed`) run through the UE job queue. From a fixed seed it writes N assets under `/Game/GLSeed/` and a **ground-truth manifest** (JSON) that lists every asset with its seeded defect, or none. The rules never read the manifest. Same seed, same corpus (criterion E2).
- **Sizes:** N ∈ {1,000, 4,000, 10,000} assets.
- **Asset mix (per 100):** 40 data assets, 10 DataTables (20 rows each), 35 actor Blueprints, 15 component Blueprints.
- **Composition (per 100):** 20 carry **exactly one** seeded defect, 15 are **near-miss controls**, 65 are plain clean. A small fixed set has two defects (to measure masking, §5 H2 and `01b` #2), reported separately.
- **Defect kinds (in scope):**

| Rule | Kinds |
|---|---|
| `GL.Data.UnknownTag` | single tag; tag inside a container |
| `GL.Data.DanglingRow` | DataTable row missing; table null but row set; required row is None (`LintRequired`); CurveTable row missing |
| `GL.Data.Curve` | NaN or infinite key; annotated monotonic curve that reverses; required curve with no keys |
| `GL.UB.TickOutsideBudget` | per-frame ticking component added beside a budget client; per-frame actor tick inherited from a ticking native parent; per-frame component inherited from a parent Blueprint |
| `GL.UB.ClientTickGroup` | client template moved to another group in an actor Blueprint; Blueprint subclass of the client in another group |
| `GL.UB.ClientNeverTicks` | "Start with Tick Enabled" cleared on a client template |

- **Structural variants** for the data rules: top-level property, nested struct, array element, map value, instanced sub-object, DataTable row, Blueprint class default.
- **Near-miss controls** (must produce no finding): a parent tag that exists; an empty container; a fully empty row handle; a flat (non-decreasing) annotated curve; an empty curve that isn't required; a ticking component with a 0.2 s interval inside the budget scope; a ticker whose start is disabled; an actor outside the scope that ticks every frame; allow-listed components (skeletal mesh, movement) inside the scope; a client in the right group.
- **Held-out kinds** (expected misses, fixed now so they can't be quietly dropped later): a defect inside an `FInstancedStruct` payload. Reported, but not counted in SC1.
- **Mutation scenarios** (incremental runs, applied by the same commandlet after a clean baseline scan):
  - **M1**: two rows removed from each of two shared DataTables;
  - **M2**: the corpus's "removable" tag file (`Config/Tags/`) deleted;
  - **M3**: a per-frame ticking component added to one parent actor Blueprint that has in-scope children;
  - **M4** (control): a row *value* edited, with no row removed. Expected: no new findings.

### 7.2 Compared configurations (same binary, same corpus, switched on the command line)

| ID | Configuration | Why it is included |
|---|---|---|
| **F0** | Full scan, GameplayLint rules **off**: the engine path only (`IsDataValid`, the engine's validators, load-log capture) | The existing solution. Run through our commandlet with every rule disabled, so all configurations share the timing points. Checked once against the stock `-run=DataValidation` for identical counts |
| **F1** | Full scan, all GameplayLint rules | The system |
| **F2** | Full scan, data rules only | Cost of one pack (SC4) |
| **F3** | Full scan, UpdateBudget rules only | Cost of the other pack (SC4) |
| **I0** | Incremental (pseudo-changelist from the mutation), rules off | The stock incremental behaviour |
| **I1** | Incremental, rules on, **dependent gathering off** | Separates "having rules" from "re-validating the victims" |
| **I2** | Incremental, rules on, **coarse** dependents (every referencer of a changed asset) | The simple way to gather dependents |
| **I3** | Incremental, rules on, **precise** dependents (registry searchable-name and derived-class queries) | The design (`02` §6) |

### 7.3 Metrics

| Metric | Unit | Source |
|---|---|---|
| Detection rate (recall), rule level: seeded defects found with the right rule ID on the right asset ÷ seeded defects | % | GameplayLint JSON report vs ground truth (`Tools\score.py`) |
| Detection rate, asset level: defective assets with any warning or error | % | Per-asset details in the report (all configurations, including F0) |
| False-positive rate: control assets with any GameplayLint finding ÷ control assets | % | Report vs ground truth |
| Validation-phase time (the engine's `ValidateAssets` call) and process wall time | s | Timestamps written by the commandlet; the runner |
| Per-validator total and per-asset rule time (median, p99) | ms | `FValidatorStatistics` and the rule base class's own timer |
| Assets added by dependent gathering, and its time | count, ms | Report |
| Exit code | code | Runner |

### 7.4 Pass/fail criteria [target]

| # | Criterion | Threshold |
|---|---|---|
| SC1 | **Detection** | F1 finds ≥ **95%** of in-scope seeded defects at rule level, at every N. Every rule × variant cell with ≥ 10 seeded defects reaches ≥ **90%** |
| SC2 | **Few false alarms** | ≤ **0.5%** of control assets carry any GameplayLint finding, at every N. Every false positive is listed and explained, never silently suppressed |
| SC3 | **A real gap over the stock run** | ≥ **50%** of in-scope seeded defects are found by F1 and not flagged at all by F0. The stock run's asset-level recall is reported per defect class. Where the stock run already finds a class (expected for unknown tags, `01b` #4), GameplayLint's value for that class is stated as attribution and incremental coverage, not detection |
| SC4 | **Full-scan overhead** | Validation-phase median: F1 ≤ **1.10 ×** F0, and F2, F3 each ≤ **1.05 ×** F0, at N = 4,000 and 10,000 |
| SC5 | **On-save responsiveness** | Per-asset GameplayLint rule time: median ≤ **0.5 ms**, p99 ≤ **5 ms** |
| SC6 | **Incremental completeness** | After each of M1–M3, I3 finds **100%** of the new defects that a full F1 scan finds after the same change. M4 adds **0** findings. I0 and I1 recall are reported |
| SC7 | **Incremental speed** | At N = 10,000, I3 validation-phase time ≤ **10%** of F1's; dependent gathering ≤ **1.0 s** |
| E1 | **Packs plug in** | Adding each rule pack changes **0** lines under `Plugins/GameplayLintCore/` (checked with `git diff` on the commit that adds the pack) |
| E2 | **Reproducible corpus and results** | Two seedings with the same seed give byte-identical manifests; repeated scans of one corpus give identical finding sets |

If a criterion fails, the result is reported and analysed. Thresholds are not moved afterwards.

## 8. Measurement plan (summary; the full protocol goes in `09_Testing_And_Benchmarks.md`)

- **What runs:** the Development editor binary `UnrealEditor-Cmd.exe` in commandlet mode, `-unattended -nullrhi -nosplash -traceautostart=0 -stdout`, through the UE job queue. This is editor tooling, so there is no packaged or fixed-step run for the headline numbers (`01b` §4). The packaged game exists only to prove that no lint code reaches a runtime build.
- **CPU:** every timed run is pinned to the performance cores (mask `FFFF` = logical CPUs 0–15 of the hybrid i7-14650HX, from `Tools\core_mask.py`). The runner starts `UnrealEditor-Cmd.exe` itself with `subprocess.Popen`, pins it at once with `pin_process`, waits with a timeout and kills the process tree in a `finally` block; the mask actually applied is recorded per run. It never launches through `cmd /c start /wait`, which returns 0 whatever the program returned.
- **Protocol for times:**
  - one discarded priming run per batch (warms the file cache and the DDC);
  - configurations interleaved, with the order rotated each round;
  - **runs per combination = number of configurations**: 4 runs for F0–F3, 4 runs for I0–I3, so each configuration sits in each position once;
  - median across runs with the [min–max] range;
  - an improvement or overhead is claimed only when the ranges support it.
- **Correctness numbers** come from the same runs. They are deterministic; E2 checks that every run of one configuration gives the same findings.
- **Environment recorded with every batch:**
  - CPU i7-14650HX, GPU RTX 5070 Laptop, 32 GB RAM, Windows 11 build 26200;
  - project on D: (NVMe), engine and DDC on C: (NVMe);
  - AC power, power plan "Turbo";
  - UE 5.8.4, Development editor, affinity mask, N, corpus seed and manifest hash.
- **Raw data:** reports, logs and a manifest per batch in `Benchmarks\Raw\` (only manifests in git), summaries in `Benchmarks\Summaries\`. Naming in [Benchmarks/README.md](../Benchmarks/README.md).

## 9. Limits we accept up front

- **The same person writes the corpus and the rules.** Detection on one's own seeded defects is an upper bound. Mitigations: the defect kinds, variants, near-misses and held-out kinds are fixed now, before any rule code; the corpus generator is a separate module that the rules never see; and the stock run is measured on the same corpus.
- **A synthetic corpus is not a real project.** The false-positive rate is measured on synthetic controls only. A real-content false-positive rate needs real content; the plan is to run the packs on the other portfolio projects' content as they gain it (`12`).
- **Rules for Projects 2–9 can't be written yet.** Their lessons don't exist. The framework must make adding them a pack, not a core change (E1).
- **The UpdateBudget rules encode one project's policy.** Another project must configure which classes are budget clients and which actors are in scope. Wrong configuration gives wrong results, and the settings say so.
- **Registry queries see saved data only.** Unsaved edits are invisible to dependent gathering; pre-submit and CI validate saved files, so this matches their use.
- **Asset loading dominates scan time** [hypothesis H6]. GameplayLint can't make the stock path faster; a "registry-only, no load" mode is a possible Phase 8 optimisation, only if the data justify it.
