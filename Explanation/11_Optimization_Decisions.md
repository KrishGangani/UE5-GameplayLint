# 11 · Optimization Decisions: GameplayLint

**Status:** not written yet. Filled in **Phase 8**.

**Will contain:** every optimisation considered, with the measurement that motivated it, the change, the before/after numbers, and what was rejected. Candidates already known from Phase 2, none to be done without data: a registry-only "no load" scan mode (`01b` #6), the engine's own preloading switch (`EditorValidator.EnablePreloadingAssets`), and precise vs coarse dependent gathering (configurations I2 and I3). Added in S5 (code review): the data rules apply to every asset that is not a redirector, so each one walks every reflected property, including those of meshes and textures, which can never hold a tag or a handle. The candidate fix is a per-struct cache in the walker: skip any struct whose property tree cannot contain the asked-for type. It waits for SC4/SC5 data on a corpus with large non-data assets.
