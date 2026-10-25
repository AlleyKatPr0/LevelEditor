# .NET Version Upgrade Plan

## Overview

**Target**: Upgrade LevelEditor managed projects from .NET Framework 4.0 to .NET 10 (`net10.0`).
**Scope**: Large mixed solution with 8 projects total (7 managed projects requiring migration, 1 native C++ project unaffected by TFM changes), classic project format, and high API incompatibility volume.

### Selected Strategy
**Bottom-Up (Dependency-First)** — Upgrade from leaf nodes to root applications, tier by tier.
**Rationale**: 7 managed projects with a 6-tier dependency graph on .NET Framework require strict dependency-ordered migration.

Dependency tiers:
- Tier 1: `Atf.Core`
- Tier 2: `Atf.Gui`
- Tier 3: `Atf.Gui.WinForms`
- Tier 4: `Atf.IronPython`, `LevelEditorCore`
- Tier 5: `RenderingInterop`
- Tier 6: `LevelEditor`

## Upgrade Options

| Option | Selected | Why |
|--------|----------|-----|
| Upgrade Strategy | Bottom-Up | Multiple .NET Framework projects with deep internal dependencies require tier-by-tier migration. |
| Project Approach | In-place | The solution is desktop/library focused with no ASP.NET Framework web host to migrate side-by-side. |
| Package Management | Per-Project (defer CPM to post-migration) | Framework-to-modern migration plus non-SDK projects makes CPM best deferred until stabilization. |
| Unsupported API Handling | Defer Complex Changes | The assessment reported a very high API break count, so deferring complex replacements preserves progress per tier. |
| Windows Native APIs | Windows Compatibility Pack | Extensive WinForms/System.Drawing usage indicates a Windows-first migration path. |
| System.Web Adapters | Direct Migration to ASP.NET Core APIs | No active System.Web web-host migration path was identified for this desktop-oriented solution. |
| Assembly Binding Redirects | Remove Binding Redirects | Binding redirects were flagged and are obsolete in modern .NET runtime resolution. |
| Nullable Reference Types | Enable Nullable Reference Types | User explicitly requested enabling nullable as part of this upgrade. |

## Tasks

### 01-prerequisites-and-baseline: Validate toolchain and capture baseline

Verify .NET 10 SDK availability, confirm repository and build preconditions, and capture a baseline of current build/test behavior before project-system and target-framework changes begin. This task provides the reference point for detecting upgrade regressions and confirms the workspace can support SDK-style conversion and net10 targeting.

The baseline should include managed project build status, warning inventory, and test-discovery status where available. The native C++ rendering engine remains in scope for compatibility checks but is not part of TFM migration.

**Done when**: .NET 10 SDK is confirmed, baseline build/test state is recorded, and prerequisite blockers are resolved or documented.

---

### 02-sdk-style-conversion: Convert managed projects to SDK-style

Convert the seven managed `.csproj` files from classic format to SDK-style while keeping behavior intact and preserving project relationships. This is a structural migration task only; it should not simultaneously introduce target framework changes to net10.

Assessment signals show all managed projects are currently non-SDK-style and include binding redirect patterns. This task should absorb `packages.config`/legacy project-system normalization and prepare the solution for consistent modern build semantics in subsequent tier upgrades.

**Done when**: all seven managed projects are SDK-style, solution references remain valid, and the solution builds on existing target frameworks without introducing new warning debt.

---

### 03-tier-1-foundation: Upgrade Atf.Core to net10 and resolve foundational API changes

Upgrade `Atf.Core` as the leaf foundation project to `net10.0`, update compatibility settings, and apply required API migrations for this tier. Because multiple higher tiers depend on this library, it must be stabilized first and validated before any downstream tier is upgraded.

Assessment identified API incompatibilities in this project, so simple replacements are applied inline while complex replacements may use temporary compilable stubs when needed to keep the tier moving. This task establishes the first modern-runtime foundation for all dependent projects.

**Done when**: `Atf.Core` targets `net10.0`, tier build is green, and dependent projects still compile on their existing framework until their own upgrade tasks.

---

### 04-tier-2-and-3-ui-libraries: Upgrade Atf.Gui and Atf.Gui.WinForms

Upgrade the UI framework tiers (`Atf.Gui`, then `Atf.Gui.WinForms`) to `net10.0` with Windows desktop compatibility enabled and required API adjustments for WinForms/System.Drawing-heavy usage. These projects carry the largest API migration burden in the solution and are prerequisites for most downstream projects.

Given the assessment’s high incompatibility counts in `Atf.Gui.WinForms`, this task should prioritize maintaining compilability at each sub-tier boundary, using deferred complex-change stubs only when necessary and tracking required follow-up in-task.

**Done when**: both projects target `net10.0`, tier-local builds and tests pass, and Tier 4 projects remain buildable on their current target until upgraded.

---

### 05-tier-4-core-and-scripting: Upgrade LevelEditorCore and Atf.IronPython

Upgrade `LevelEditorCore` and `Atf.IronPython` after their dependency tiers are stable on net10. This tier bridges foundational libraries to application-level behavior, so compatibility fixes here should focus on integration contracts and runtime behavior changes.

The assessment indicates medium migration complexity for `LevelEditorCore` and lower volume for `Atf.IronPython`, but both depend on previous UI tiers. Validation should confirm that project references, interop boundaries, and scripting-related functionality still compile and execute as expected.

**Done when**: both projects target `net10.0`, compile successfully against upgraded dependencies, and no unresolved tier-blocking stubs remain.

---

### 06-tier-5-rendering-interop: Upgrade RenderingInterop and verify native boundary compatibility

Upgrade `RenderingInterop` to `net10.0` and validate the managed/native boundary with `LvEdRenderingEngine` remains correct. This tier depends on upgraded core/UI libraries and `LevelEditorCore`, and it is a critical path dependency for the final application tier.

Because native rendering remains in a separate C++ project, this task must ensure the managed interop layer still binds correctly after project-system and runtime updates, with build validation spanning both managed interop assembly and dependent native artifacts.

**Done when**: `RenderingInterop` targets `net10.0`, interop build succeeds, and downstream application references compile without new boundary errors.

---

### 07-tier-6-application: Upgrade LevelEditor application to net10

Upgrade `LevelEditor` as the root application after all dependency tiers are modernized and stable. This task consolidates runtime, UI, and interop updates into the final executable project and addresses remaining application-specific API and behavior changes.

The assessment reports significant API impact in this project, so this task includes final replacement cleanup, configuration updates, and user-path validation for the editor startup and core workflows.

**Done when**: `LevelEditor` targets `net10.0`, application build succeeds against all upgraded dependencies, and critical startup/runtime paths validate successfully.

---

### 08-final-validation-and-cleanup: Validate full solution and complete modernization cleanup

Run full solution validation across all upgraded managed projects and ensure warning-free builds in modified projects. Remove obsolete assembly binding redirects, verify nullable reference types are enabled as requested, and reconcile any deferred complex API stubs introduced during tier execution.

This task also records post-migration recommendations, including adopting Central Package Management as a follow-up now that projects are SDK-style and unified on modern .NET.

**Done when**: complete solution build succeeds, relevant tests pass, managed projects are warning-free, binding redirects are removed, nullable is enabled where planned, and remaining follow-up items are documented.