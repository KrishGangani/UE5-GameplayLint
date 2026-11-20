# 00 · Project Overview: GameplayLint

> *"This was the problem. This was my investigation. This was my solution. These were my technical decisions. This was how I measured the results."*

**Status (2026-11-21):** Phases 1–4 are done: problem analysis (`01`), verified research (`01b`), final architecture v1.0 (`02`), build steps (`03`), and the foundation (the UE project, five modules and three plugins building with 0 errors and 0 warnings, a packaged smoke run; checks in `09`). No rule code exists yet, and no results exist yet.

---

## A. Identity

| | |
|---|---|
| **Name** | GameplayLint: meaning-level checks for gameplay data |
| **Technical category** | Editor tools and automation · developer productivity · content validation in CI |
| **Main objective** | Catch broken *relations* in gameplay data before a playtest: a tag that no longer exists, a DataTable row that was renamed, a curve that violates what its user expects, an actor that does per-frame work outside the project's update budget. Catch them when the *referenced* thing changes, not only when the broken asset itself is saved. Measure how many seeded defects it finds, how many false alarms it raises, and what a full and an incremental scan cost |
| **Difficulty** | ★★★☆☆ Intermediate–Advanced |
| **Core UE technologies** | Data Validation plugin (`UEditorValidatorBase`, `UEditorValidatorSubsystem`, changelist validation, the `DataValidation` commandlet and its 5.8 extension points, fixers), Asset Registry (searchable-name dependencies, referencers), Gameplay Tags, DataTables and CurveTables, reflection (`FPropertyValueIterator`), Blueprint component templates, editor subsystems, developer settings, commandlets |
| **Relation to existing work** | It is **not** a validation framework: Epic's Data Validation plugin already is one, and a good one. GameplayLint is a set of rule packs built on it, plus a thin rule base class, a shared data walker, a report and a child commandlet. The engine provides the hook for cross-asset incremental validation (`GetAssetsToValidateFromChangelist`) but ships no rule that uses it for tags or rows. See `01` §3 and `01b` §5 |

## B–D. Problem, solution, architecture

- Problem, root-cause hypotheses, success criteria and the measurement plan: **[01_Problem_Analysis.md](01_Problem_Analysis.md)**
- What the engine actually does (verified in 5.8.4 source): **[01b_Research_Findings.md](01b_Research_Findings.md)**
- Solution and architecture (final v1.0): **[02_System_Architecture.md](02_System_Architecture.md)**

## E. Technical demonstration

**What the viewer sees**
- A generated test corpus: N assets (data assets, DataTables, actor and component Blueprints) built by a commandlet from a fixed seed. About a fifth carry exactly one known defect; the rest are clean, and some of those are deliberate near-misses that a careless rule would flag.
- A terminal and the editor's Message Log side by side.
- A score table produced by a script that compares each run's findings with the corpus's ground-truth manifest.

**Before** (the engine's stock Data Validation run, same corpus)
- Some defect classes surface as load warnings with no rule name and no property path. Others, such as a row handle pointing at a deleted row or an actor that ticks every frame next to its budget component, pass silently.
- Then one DataTable loses two rows and one tag file is removed. The stock *incremental* validation re-checks only the changed table. The assets that now point at missing rows are never looked at.

**After** (the same corpus with GameplayLint enabled)
- Every finding has a rule ID (for example `GL.Data.DanglingRow`), the asset, the property path (`Payloads[3].Loot`) and the lesson it encodes.
- The incremental run asks the asset registry which unchanged assets reference the removed rows and tags, without loading anything, and validates exactly those.
- The score table shows detection rate, false positives and scan times for the stock run and for GameplayLint, at three corpus sizes.

**Recorded material:**
- a short video of the editor flow (save an asset → Message Log entry with a clickable asset link);
- the CI flow (commandlet → JSON report → score table);
- before/after tables and charts of detection, false positives, full and incremental scan time.

## G. Portfolio presentation (produced in Phase 10)

1. Problem statement, one paragraph (from `01` §1).
2. The investigation: what the engine already does (it does a lot), and the precise places where relations between assets go unchecked (`01b`).
3. The architecture diagram and the per-asset and incremental flows (`02` §5–6).
4. The key decisions and rejected alternatives: plug into Data Validation instead of a parallel framework; one validator class per rule; registry queries for incremental dependents.
5. Detection, false-positive and scan-time results against the stock run, with the corpus and method.
6. What didn't work or wasn't needed, stated plainly (for example, if the stock run already catches a defect class).
7. Video, source repository and the complete `Explanation/` folder.

## H. Industry relevance

**Skills demonstrated:**
- extending an engine framework the way it was designed to be extended, instead of building a parallel one;
- reading engine source to find what really happens (the order of class-level and validator checks, what incremental validation re-checks, what the asset registry already knows);
- cross-asset reasoning with the asset registry (references, searchable names, derived classes);
- reflection-driven tooling: walking arbitrary structs, arrays, maps, instanced objects and Blueprint component templates;
- turning a team's documented mistakes into automated checks;
- measuring a tool: recall and false positives on a seeded corpus, and scan cost.

**Studios and roles that ask for this:**
- Respawn (Apex): "automated content validation" (senior generalist) and "measure the impact" (tools developer);
- Ubisoft Montréal, Assassin's Creed Origins: "daily automated tests and reports" for world data whose AI, art and design parts depend on each other (GDC 2018);
- tools and developer productivity appear in 33 of 57 postings reviewed, and in 9 of 10 lead roles;
- Riot's Unreal Ecosystem staff roles: shared tools for many teams, automated testing, CI/CD.

Sources: [research/01](Research/01_job_market.md) (EA2, EA4, R3; skill tally §4–5) and [research/02](Research/02_technical_challenges.md) ([T8-A], [T8-D]).

**Interview questions it prepares you for:**
- "How would you stop broken data from reaching a playtest? What runs on save, what runs in CI, and why both?"
- "`IsDataValid` or an editor validator: when would you use each?"
- "A designer deletes a DataTable row. Which assets are now broken, and how do you find them without loading the whole project?"
- "How do you measure whether a validation tool is any good? What's its false-positive rate, and why does that number decide whether people keep it enabled?"
- "Your validator is slow. Where does the time actually go?"
- "How do you design a rule system so other teams can add rules without touching your code?"
- "What can a data linter never catch, and what catches those bugs instead?"
