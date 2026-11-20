# 12 · Future Scalability: GameplayLint

**Status:** not written yet. Filled in **Phase 10**; the growth paths decided in Phase 3 are in [02_System_Architecture.md](02_System_Architecture.md) §11.

**Will contain:**
- **One rule pack per portfolio project**, written from that project's documented mistakes once it exists (candidates in `02` §11).
- **The deferred cook-reachability rule** from UE5-UpdateBudget's mistakes ("an asset referenced only from C++ isn't cooked"): assets referenced by native class defaults of project modules that no cook rule reaches (not under `DirectoriesToAlwaysCook`, not reachable from the maps or primary assets that get cooked). Its ground truth is a packaged build's cooked file list, so it is validated against a real cook rather than the seeded corpus.
- **Suppressions with reasons and expiry**, **trend reports** in CI, editor-menu integration, and a registry-only fast mode if Phase 8 justifies it.
- **Running the packs on real content** (the other portfolio projects) to measure a real-content false-positive rate.
