# LevelEditor Wiki

## Table of Contents

- [Overview](#overview)
- [Feature Summary](#feature-summary)
- [Architecture](#architecture)
- [Repository Structure](#repository-structure)
- [System Requirements](#system-requirements)
- [Build and First Run](#build-and-first-run)
- [Resource Root (AssetRoot) Setup](#resource-root-assetroot-setup)
- [Sample Level Walkthrough](#sample-level-walkthrough)
- [Extending and Customizing](#extending-and-customizing)
- [Documentation](#documentation)
- [Troubleshooting and Performance](#troubleshooting-and-performance)
- [Documentation Roadmap](#documentation-roadmap)

## Overview

LevelEditor is a Windows desktop tool for designing game levels (maps, campaigns, and virtual worlds). It is a fork of [SonyWWS/LevelEditor](https://github.com/SonyWWS/LevelEditor). The user interface and data management are written in C# on Sony's Authoring Tools Framework (ATF), using WinForms and the Managed Extensibility Framework (MEF). 3D rendering is performed by a native C++ DirectX 11 engine. The editor provides a WYSIWYG interface and can be customized and extended through plug-ins. See the [README](../../README.md) for the original project description.

## Feature Summary

Based on the repository README and source layout:

- WYSIWYG design view for laying out game worlds
- Placement of static game objects and association of assets with them
- Positioning, rotating, scaling, and snapping of objects
- Property editing driven by the XML schema
- Show/hide of object groups, and layers and bookmarks (see `LevelEditor/Listers`)
- Linears (lines and curves) and lights
- Terrain editing (see `LevelEditor/Terrain` and `AssetRoot/HeightMaps`)
- Native DirectX rendering through `LevelEditorNativeRendering`
- Optional MEF plug-ins loaded at startup

Consult the [User's Guide](../LevelEditor-UsersGuide_e.pdf) for the authoritative description of editor operation.

## Architecture

| Component | Role |
| --- | --- |
| [`LevelEditor/LevelEditorApplication.cs`](../../LevelEditor/LevelEditorApplication.cs) | Application entry point (`Main`); builds the MEF composition catalog, including ATF and editor components, and loads plug-ins. |
| [`LevelEditor/GameEditor.cs`](../../LevelEditor/GameEditor.cs) | Game document handling and resource root integration. |
| [`LevelEditor/SchemaLoader.cs`](../../LevelEditor/SchemaLoader.cs) | Loads the XML schema and processes `scea.dom.*` annotations into property editors and palette metadata. |
| [`LevelEditor/GameAdapters.cs`](../../LevelEditor/GameAdapters.cs) | DOM node adapters that attach editor behavior to ATF DOM nodes. |
| [`LevelEditorCore/`](../../LevelEditorCore) | Shared editor interfaces, design views, property editing, commands, listers, rendering contracts, game-engine proxy, and vector math. |
| [`LevelEditorNativeRendering/`](../../LevelEditorNativeRendering) | C#/C++ interop (`NativeGameEditor.cs`, `NativeDesignView.cs`) and the native `LvEdRenderingEngine`. |
| [`CodeGenDom/`](../../CodeGenDom) | Command-line tool that generates native C++ code from the schema. |

Runtime flow: `LevelEditorApplication` composes the MEF catalog; `SchemaLoader` loads the schema and annotations; `GameEditor` manages documents; `GameAdapters` supplies behavior for DOM nodes; and the native rendering layer draws the world in the design view.

## Repository Structure

| Path | Contents |
| --- | --- |
| [`ATF/`](../../ATF) | Included subset of the Authoring Tools Framework (see [`ATF/ReadMe.txt`](../../ATF/ReadMe.txt)). |
| [`LevelEditor/`](../../LevelEditor) | The editor application: commands, documents, DOM adapters, serializers, listers, terrain, services, `schemas/`, `OnlineHelp/`. |
| [`LevelEditorCore/`](../../LevelEditorCore) | Reusable editor core library. |
| [`LevelEditorNativeRendering/`](../../LevelEditorNativeRendering) | Native rendering bridge and engine. |
| [`CodeGenDom/`](../../CodeGenDom) | Schema-to-C++ code generator. |
| [`AssetRoot/`](../../AssetRoot) | Sample assets: `HeightMaps`, `Rocks`, `Textures`, `Trees`. |
| [`SampleLevels/`](../../SampleLevels) | Example `.lvl` levels. |
| [`Docs/`](..) | PDF guides and `DocsSource`. |
| [`build/`](../../build) | Visual Studio solutions. |

## System Requirements

From the [README](../../README.md):

- Microsoft Windows 7 64-bit SP1 with platform update, or newer
- DirectX 10 compatible graphics card or newer
- Visual Studio 2010 Pro or Visual Studio 2013 Pro, with Windows SDK 8.1 installed at its default location

Solutions provided:

- [`build/LevelEditor.vs2010.sln`](../../build/LevelEditor.vs2010.sln)
- [`build/LevelEditor.vs2013.sln`](../../build/LevelEditor.vs2013.sln)
- [`build/LevelEditor.vs2015.sln`](../../build/LevelEditor.vs2015.sln)

## Build and First Run

1. Open one of the solutions above in a compatible Visual Studio.
2. Set the configuration to **Release** and build LevelEditor.
3. Run `LevelEditor.exe` from the build output directory.
4. Select **Edit -> Preferences -> Resources** and set **ResourceRoot** to [`AssetRoot`](../../AssetRoot) or another asset directory; restart LevelEditor.
5. Drag and drop assets from the Resources window to the design view or the Project lister.

## Resource Root (AssetRoot) Setup

`AssetRoot` is the root folder for assets; point LevelEditor to it so assets resolve (see [`AssetRoot/ReadMe.txt`](../../AssetRoot/ReadMe.txt)). Use **Edit -> Preferences**, click **Resources**, and set **ResourceRoot**. The README suggests a directory containing ATGI or Collada files. `GameEditor` tracks the resource root and updates the resource lister when it changes.

## Sample Level Walkthrough

1. Complete the [first-run steps](#build-and-first-run), pointing ResourceRoot at `AssetRoot`.
2. Open one of the sample levels from [`SampleLevels`](../../SampleLevels):
   - [`BasicLevel.lvl`](../../SampleLevels/BasicLevel.lvl)
   - [`LightTest.lvl`](../../SampleLevels/LightTest.lvl)
   - [`Terrain.lvl`](../../SampleLevels/Terrain.lvl)
3. Inspect the objects in the design view and the listers, and review their properties.

## Extending and Customizing

- **MEFPlugin:** `LevelEditorApplication` loads all DLLs in a `MEFPlugin` directory next to the executable, and `StateMachinePlugin.dll` if present, adding them to the MEF catalog.
- **XML schema annotations:** the schema in `LevelEditor/schemas` defines types; `SchemaLoader` processes annotations such as `scea.dom.editors` and `scea.dom.editors.attribute`. Native annotations are defined in [`NativeAnnotations.cs`](../../LevelEditorNativeRendering/NativeAnnotations.cs).
- **CodeGenDom:** generates native C++ code from a schema:

  ```text
  CodeDomGen schemaFile outputFile codeNamespace
  ```

  Source: [`CodeGenDom/Program.cs`](../../CodeGenDom/Program.cs).

See the [Programming Guide](../LevelEditor-ProgrammingGuide_e.pdf) for details.

## Documentation

- [User's Guide (PDF)](../LevelEditor-UsersGuide_e.pdf)
- [Programming Guide (PDF)](../LevelEditor-ProgrammingGuide_e.pdf)
- [README](../../README.md)
- Upstream wiki: <https://github.com/SonyWWS/LevelEditor/wiki>

## Troubleshooting and Performance

- Slow level loading or rendering: use a **Release** build; the release `LvEdRenderingEngine.dll` is much faster than the debug build.
- Assets do not appear: verify **ResourceRoot** in Edit -> Preferences and restart LevelEditor.
- Build problems: confirm Visual Studio and Windows SDK 8.1 match the [requirements](#system-requirements).
- Graphics problems: confirm a DirectX 10 compatible GPU.

## Documentation Roadmap

Suggested additional wiki pages:

1. Installation and Requirements
2. Features
3. Terrain Editing
4. Schema and Plug-in Development
5. Architecture Deep Dive
