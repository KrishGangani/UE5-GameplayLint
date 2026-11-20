# P10 · GameplayLint: meaning-level checks for gameplay data

**Domain:** Editor tools & automation · developer productivity · **Difficulty:** ★★★☆☆ · **Engine:** UE 5.8.4

> **Status: planned; Phases 1–3 (problem analysis, research, architecture) are starting.** The ten projects are developed in parallel, and every Unreal Engine build and benchmark runs one at a time on the same machine, so measurements stay clean. Everything below is the plan. The success criteria are **targets fixed before any code exists, not results**. Real numbers will only come from controlled before/after runs, each traceable to a raw benchmark batch.

## The problem

Broken cross-asset gameplay data (missing tags, dangling DataTable rows, invalid curves, Smart Object slots without matching animations) reaches playtests. The validation *framework* exists, but meaning-level gameplay rules don't

## Proposed solution

Cross-asset validators plus a CI commandlet. The rules encode the lessons of Projects 1–9 (e.g. "ticking actor not registered with the budget"). Measured by detection rate on seeded defects

## Main Unreal Engine technologies

Data Validation (`UEditorValidatorBase`, commandlet, fixers), Asset Registry, Gameplay Tags, DataTables, editor module

## Success criteria (targets, defined before implementation)

Detection rate on seeded defects; false-positive rate; incremental (changelist) and full-scan time on N assets

## Existing work it must be compared against

Data Validation plugin (validators, commandlet, fixers), Asset Referencing Restrictions, Linter v2, CommonValidators

**The gap it targets:** Cross-asset gameplay rules with a measured detection rate (gap strength: Medium).

**Main risk:** Keeping rules general enough to be useful outside this portfolio

## Why studios care

Tools in 33/57 postings (9/10 leads); Respawn "automated content validation"; Respawn Tools "measure the impact"; AC Origins data validation

## Method

Every project in the portfolio follows the same ten phases: problem analysis → research → architecture → foundation → implementation → explanation → testing → profiling → review → portfolio. Each phase is documented in `Explanation/` (files 00–12, including a line-by-line code walkthrough), and every claimed improvement must be measured against the existing solutions above on the same workload. Engine: Unreal Engine 5.8.4, C++.

## Folder layout

```text
P10_GameplayLint\
├── README.md        ← this page (becomes the project's portfolio page)
├── Explanation\     ← 00–12, created when the project starts
├── Benchmarks\      ← Raw\ + Summaries\
├── Media\           ← diagrams, screenshots, demo video
├── Tools\           ← analysis and run scripts
└── GameplayLint\  ← the Unreal Engine project (created in its Phase 4)
```
