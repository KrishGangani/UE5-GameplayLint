# 10 · Interview Questions: GameplayLint

Answers come from this project's code and measurements (N = 1,000 seeded corpus, `09`).

**1. Why build rules as Data Validation validators instead of a separate tool?**
Validators run everywhere the engine validates: on save, from menus, before submit, in CI. And they are found by reflection: a concrete subclass is enough (`EditorValidatorSubsystem.cpp:381–395`). The project adds one abstract base (`UGameplayLintRule`) with a stable rule ID, a timer and the pass/fail contract, so a rule only implements `AppliesTo` and `CheckAsset`.

**2. The engine already warns about unknown gameplay tags. What is the real gap?**
The baseline answered it before any rule existed. The stock run flagged 28 of 200 seeded defects, exactly the unknown tags, and only as load warnings: the asset still "contains valid data", and the message names the struct, not the field. Dangling row handles, broken curves and tick-budget mistakes were 0 of 172. GameplayLint found 200/200, each with a rule ID and a path such as `Map[K1].Tag`.

**3. How do you know the measurements aren't flattering?**
- The corpus is generated from a seed, with ground truth, near-misses that must pass, held-out kinds that v1 is expected to miss, and a masked set.
- The criteria were fixed before the rules.
- The first baseline was thrown away and kept as INVALID: the seeded tag files had loaded no tags. The run script now refuses such a run.
- What wasn't measured (full-scan overhead under the protocol, incremental runs, other sizes) is marked "not measured".

**4. How does one rule see values in arrays, maps, instanced objects and Blueprint templates?**
A shared walker uses `FPropertyValueIterator` with a match-all property class. It skips transient properties, and it follows instanced sub-objects, DataTable rows, the Blueprint class default object and every component template (`AActor::ForEachComponentOfActorClassDefault`). It hands each rule the memory, the declaring property (for metadata such as `LintRequired`) and a readable path.

**5. What was the hardest bug?**
A baseline in which every tag was unknown. The seeder wrote `+GameplayTagList=` lines, but a loose `Config/Tags/*.ini` file is read without ini symbol commands (`ConfigCacheIni.cpp:2216–2219`), so the key literally was `+GameplayTagList`. It was found by grouping 7,661 warnings and noticing that even tables holding only declared tags failed (`08` §2).

**6. And a crash?**
A test held its rule objects in raw pointers. Compiling a test Blueprint runs a garbage collection (`BlueprintCompilationManager.cpp:423–427`), so the next `ValidateLoadedAsset` read freed memory. The fix was `TStrongObjectPtr` for every object that must survive more than one call.

**7. How do incremental runs find the victims of a change?**
The engine re-checks the referencers only of *deleted* packages. Each rule adds its own dependents through `GetAssetsToValidateFromChangelist`, from the asset registry: searchable names for tags and rows, package referencers, and the Blueprint class hierarchy. Precise mode adds only referencers that name a row or tag that is now gone. The code is built and unit-tested, but its completeness and speed (SC6, SC7) were **not measured**.

**8. What did code review catch that tests didn't?**
- Ignoring `bAutoActivate`: an auto-activating component's tick is enabled by `Activate`.
- NaN tangents and default values in curves.
- Counting dependents per path instead of per asset.
- A test whose only clean case would pass a rule that flags everything.
Each became a test case or a line in `07`.

**9. How would a new team add rules?**
Write a plugin that depends on the core, subclass the rule base, and name project classes by soft path in a settings class. The UpdateBudget pack was added this way with 0 changes to the core (E1).

**10. What would you measure next?**
The interleaved F0/F1 batch at N = 1,000 and 5,000. One indicative run suggests about 1.33 × validation-phase cost, above the 1.10 × target, and the candidate optimisation is in `11`. Then I0–I3 after M1–M4.
