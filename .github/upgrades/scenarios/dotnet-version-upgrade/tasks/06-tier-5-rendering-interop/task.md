# 06-tier-5-rendering-interop: Upgrade RenderingInterop and verify native boundary compatibility

Upgrade `RenderingInterop` to `net10.0` and validate the managed/native boundary with `LvEdRenderingEngine` remains correct. This tier depends on upgraded core/UI libraries and `LevelEditorCore`, and it is a critical path dependency for the final application tier.

Because native rendering remains in a separate C++ project, this task must ensure the managed interop layer still binds correctly after project-system and runtime updates, with build validation spanning both managed interop assembly and dependent native artifacts.

**Done when**: `RenderingInterop` targets `net10.0`, interop build succeeds, and downstream application references compile without new boundary errors.
