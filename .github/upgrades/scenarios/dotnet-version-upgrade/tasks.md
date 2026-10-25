# .NET Version Upgrade Progress

## Overview

Upgrade LevelEditor managed projects from .NET Framework 4.0 to .NET 10 using a bottom-up dependency-first strategy. The plan converts all managed projects to SDK-style first, then upgrades each dependency tier from foundation libraries to the final application. Validation and cleanup close the migration with warning-free builds and modernization follow-through.

**Progress**: 1/8 tasks complete <progress value="12" max="100"></progress> 12%

## Tasks

- ✅ 01-prerequisites-and-baseline: Validate toolchain and capture baseline ([Content](tasks/01-prerequisites-and-baseline/task.md), [Progress](tasks/01-prerequisites-and-baseline/progress-details.md))
- 🔲 02-sdk-style-conversion: Convert managed projects to SDK-style
- 🔲 03-tier-1-foundation: Upgrade Atf.Core to net10 and resolve foundational API changes
- 🔲 04-tier-2-and-3-ui-libraries: Upgrade Atf.Gui and Atf.Gui.WinForms
- 🔲 05-tier-4-core-and-scripting: Upgrade LevelEditorCore and Atf.IronPython
- 🔲 06-tier-5-rendering-interop: Upgrade RenderingInterop and verify native boundary compatibility
- 🔲 07-tier-6-application: Upgrade LevelEditor application to net10
- 🔲 08-final-validation-and-cleanup: Validate full solution and complete modernization cleanup