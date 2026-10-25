# 05-tier-4-core-and-scripting: Upgrade LevelEditorCore and Atf.IronPython

Upgrade `LevelEditorCore` and `Atf.IronPython` after their dependency tiers are stable on net10. This tier bridges foundational libraries to application-level behavior, so compatibility fixes here should focus on integration contracts and runtime behavior changes.

The assessment indicates medium migration complexity for `LevelEditorCore` and lower volume for `Atf.IronPython`, but both depend on previous UI tiers. Validation should confirm that project references, interop boundaries, and scripting-related functionality still compile and execute as expected.

**Done when**: both projects target `net10.0`, compile successfully against upgraded dependencies, and no unresolved tier-blocking stubs remain.
