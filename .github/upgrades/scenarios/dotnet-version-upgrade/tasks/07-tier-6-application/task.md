# 07-tier-6-application: Upgrade LevelEditor application to net10

Upgrade `LevelEditor` as the root application after all dependency tiers are modernized and stable. This task consolidates runtime, UI, and interop updates into the final executable project and addresses remaining application-specific API and behavior changes.

The assessment reports significant API impact in this project, so this task includes final replacement cleanup, configuration updates, and user-path validation for the editor startup and core workflows.

**Done when**: `LevelEditor` targets `net10.0`, application build succeeds against all upgraded dependencies, and critical startup/runtime paths validate successfully.
