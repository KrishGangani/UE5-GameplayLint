# P10 · GameplayLint: meaning-level checks for gameplay data

**Author:** Krish Gangani · [github.com/KrishGangani](https://github.com/KrishGangani)

**Domain:** Editor tools & automation · developer productivity · **Difficulty:** ★★★☆☆ · **Engine:** UE 5.8.4

> **Status: in progress. There are no results yet.** Phases 1–4 are done, and Phase 5 steps S1–S4 (fixtures, the seeded corpus, the measuring harness, the rule base) are built and tested. The stock engine's baseline was measured before any rule exists: on a seeded 1,000-asset corpus it flags 28 of 200 defects, all of them unknown gameplay tags, as warnings ([09](Explanation/09_Testing_And_Benchmarks.md)). The three data rules (S5–S6) find all 125 seeded unknown-tag, dangling-row and bad-curve defects with no false alarms; the UpdateBudget rules and incremental runs come next. The ten portfolio projects are developed in parallel, and every Unreal Engine build and benchmark runs one at a time on the same machine, so measurements stay clean. Sections marked *(pending)* are filled in only from real measurements, in the phase named. The success criteria are **targets fixed before any rule code exists, not results**.

| Phase | 1 Problem | 2 Research | 3 Architecture | 4 Foundation | 5 Implementation | 6 Explanation | 7 Testing | 8 Profiling | 9 Review | 10 Portfolio |
|---|---|---|---|---|---|---|---|---|---|---|
| State | ✓ | ✓ | ✓ final | ✓ | S1–S6 of 10 ✓, next S7 | ongoing | — | — | — | — |

---

## Problem statement

Gameplay data is a web of references: abilities name tags, loot points at DataTable rows, cooldowns are curves that must rise with level, and an enemy Blueprint promises, by the project's own rules, to do its per-frame work through an update budget. Unreal checks single assets well. It checks much less about whether a **relation** still holds after the *referenced* asset changes. When a row is removed or a tags file is edited, the asset that is now broken is not the one that changed, so incremental validation never looks at it. Details: [Explanation/01_Problem_Analysis.md](Explanation/01_Problem_Analysis.md).

## What already exists (and the honest gap)

Epic's Data Validation plugin is a complete framework:
- validators run on save, from menus, before submit and in CI;
- C++ validators are found by reflection;
- there is an incremental (changelist) path with a hook for extra assets, fixers, per-validator timing and an extensible commandlet.

Unknown gameplay tags already raise a load warning, and GAS, Smart Objects and StateTree check their own references.

What the reviewed tools don't do:
- generic relation rules across user-defined data;
- re-validating the unchanged **victims** of a change (the engine has the hook and the registry data, and nothing uses them for this);
- rules taken from a team's documented mistakes;
- published detection or false-positive rates.

The tag part of the gap is weaker than first planned, and the write-up says so. Details: [Explanation/01b_Research_Findings.md](Explanation/01b_Research_Findings.md).

## Engineering approach

- **Every rule is a Data Validation validator.** It derives from one small abstract base class that gives it a stable ID, a lesson, a timer, a report and the engine's pass/fail contract.
- **A shared walker** hands each rule every value of one struct type inside an asset (structs, arrays, maps, instanced objects, DataTable rows, Blueprint defaults and component templates), with a readable path.
- **Incremental runs re-check the victims.** Each rule asks the asset registry which unchanged assets a change can break. Tag and row references are recorded as searchable names when assets are saved, so no asset is loaded to find the victims.
- **Rules ship as plugins (packs):**
  - a generic data pack: unknown tags, dangling DataTable/CurveTable rows, curve contracts;
  - a pack built from UE5-UpdateBudget's documented mistakes: per-frame ticking outside the update budget, budget clients in the wrong tick group or never ticking.
- **A child of the engine's commandlet** adds a path filter, a changelist read from a file, a JSON report and a meaningful exit code.

Details: [Explanation/02_System_Architecture.md](Explanation/02_System_Architecture.md).

## Architecture diagram

Final (v1.0): [Explanation/02_System_Architecture.md](Explanation/02_System_Architecture.md) §5–6 (mermaid).

## Before / after comparison

*(pending, Phase 8)*. Eight configurations on one seeded corpus:
- the stock engine run (F0);
- GameplayLint with all rules, and with each pack alone (F1–F3);
- four incremental variants after controlled changes (I0 stock, I1 no dependents, I2 coarse dependents, I3 precise dependents).

## Benchmark results

*(pending, Phase 8)*. Detection rate and false-positive rate per rule, defect kind and structural variant; full-scan and incremental validation times at N = 500 / 1,000 / 5,000 assets (the owner's cap: 5,000 assets, about 1.5 hours of queue time; the trend is reported, not extrapolated). Raw data in [Benchmarks/](Benchmarks/README.md). The pass/fail criteria were fixed in advance: [01_Problem_Analysis.md §7.4](Explanation/01_Problem_Analysis.md).

## Recorded demonstration

*(pending, Phase 10)*. Saving an asset and seeing a rule's finding in the Message Log; a DataTable losing a row and the incremental run finding the unchanged assets that point at it; the score table for the stock run vs GameplayLint.

## Source code

The UE project is in [GameplayLint/](GameplayLint/) (UE 5.8.4, C++):
- the reusable framework is the plugin `GameplayLintCore`;
- the rule packs are the plugins `GameplayLintDataRules` and `GameplayLintUpdateBudget`;
- the fixture data types are in the game module `GameplayLint`, and the corpus generator is in the editor module `GameplayLintBench`;
- run scripts and tools are in [Tools/](Tools/).

## Documentation

| File | Contents |
|---|---|
| [00_Project_Overview](Explanation/00_Project_Overview.md) | Identity, demonstration, presentation, industry relevance |
| [01_Problem_Analysis](Explanation/01_Problem_Analysis.md) | Problem, existing work, the first rule pack's sources, root-cause hypotheses, success criteria, measurement plan |
| [01b_Research_Findings](Explanation/01b_Research_Findings.md) | Phase 2: verified engine behaviour (UE 5.8.4) and what it changed in the design |
| [02_System_Architecture](Explanation/02_System_Architecture.md) | Final v1.0: modules, classes, rules, flows, defaults, safety rules, growth, limits, rejected alternatives |
| [03_Implementation_Roadmap](Explanation/03_Implementation_Roadmap.md) | The small, ordered build steps; the stock baseline is captured before any rule code |
| [04](Explanation/04_CPP_Fundamentals.md) · [05](Explanation/05_Line_By_Line_Code_Explanation.md) · [06](Explanation/06_Unreal_APIs_And_Macros.md) · [07](Explanation/07_Common_Mistakes.md) | C++ fundamentals, line-by-line code, APIs and macros, common mistakes. Written step by step with the code (current to S4) |
| [09](Explanation/09_Testing_And_Benchmarks.md) | Every check and benchmark batch so far |
| [08](Explanation/08_Debugging_And_Profiling.md) · [10](Explanation/10_Interview_Questions.md) · [11](Explanation/11_Optimization_Decisions.md) · [12](Explanation/12_Future_Scalability.md) | Debugging and profiling, interview questions, optimisation decisions, future scalability. Filled in during Phases 5–10 |
| [Phase2_Raw_Notes/](Explanation/Phase2_Raw_Notes/README.md) | The research audit trail |
| [Research/](Explanation/Research/) | The portfolio-wide research this project draws on: job market, technical challenges, existing solutions, local engine verification |

## Why studios care

Tools and developer productivity appear in 33 of 57 job postings reviewed, and in 9 of 10 lead roles. Respawn asks for "automated content validation" and for tools whose impact is measured. Ubisoft's Assassin's Creed Origins team described daily automated validation of world data whose parts depend on each other. Sources: [research/01](Explanation/Research/01_job_market.md), [research/02](Explanation/Research/02_technical_challenges.md).

**Main risk:** keeping the rules general enough to be useful outside this portfolio. Mitigations: rules are configured by class paths and property annotations, not hard-coded types, and each project's lessons are a separate pack.

## Folder layout

```text
P10_GameplayLint\
├── README.md        ← this page (the project's portfolio page)
├── Explanation\     ← 00–12, research findings, raw notes, research copies
├── Benchmarks\      ← Raw\ + Summaries\, run protocol
├── Media\           ← diagrams, screenshots, demo video
├── Tools\           ← build, package, smoke and validate scripts; CPU mask (run and scoring scripts from S3)
└── GameplayLint\    ← the Unreal Engine project (UE 5.8.4)
```

## License

MIT, see [LICENSE](LICENSE). Unreal Engine itself is not included and is covered by Epic Games' own license.
