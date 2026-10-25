# 01-prerequisites-and-baseline: Validate toolchain and capture baseline

Verify .NET 10 SDK availability, confirm repository and build preconditions, and capture a baseline of current build/test behavior before project-system and target-framework changes begin. This task provides the reference point for detecting upgrade regressions and confirms the workspace can support SDK-style conversion and net10 targeting.

The baseline should include managed project build status, warning inventory, and test-discovery status where available. The native C++ rendering engine remains in scope for compatibility checks but is not part of TFM migration.

## Scope Inventory

- **Projects in scope**: `LevelEditor`, `LevelEditorCore`, `RenderingInterop`, `Atf.Core`, `Atf.Gui`, `Atf.Gui.WinForms`, `Atf.IronPython`, and native `LvEdRenderingEngine`.
- **Distinct concerns**: SDK/tooling readiness, global.json compatibility, baseline build health, test project discovery.
- **Change signals from assessment**: all managed projects are non-SDK-style .NET Framework 4.0 projects with significant API migration volume; no NuGet package inventory was detected in the assessment.
- **Build tool strategy for baseline**: use solution-level IDE/MSBuild build path for legacy project compatibility (classic csproj + vcxproj).

## Research Findings

- `validate_dotnet_sdk_installation(net10.0)` confirms compatible SDK presence.
- `validate_dotnet_sdk_in_globaljson(net10.0)` reports no global.json constraints.
- Solution project inventory confirms 7 managed `.csproj` projects and 1 native `.vcxproj` project.
- Test project discovery returned no detected test projects in the current solution composition.
- Baseline full workspace build succeeds before upgrade execution begins.

## Baseline Outcome

Prerequisites are satisfied. No blocker prevents moving to SDK-style conversion as the next task.

**Done when**: .NET 10 SDK is confirmed, baseline build/test state is recorded, and prerequisite blockers are resolved or documented.
