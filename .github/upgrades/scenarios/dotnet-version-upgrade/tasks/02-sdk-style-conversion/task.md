# 02-sdk-style-conversion: Convert managed projects to SDK-style

Convert the seven managed `.csproj` files from classic format to SDK-style while keeping behavior intact and preserving project relationships. This is a structural migration task only; it should not simultaneously introduce target framework changes to net10.

Assessment signals show all managed projects are currently non-SDK-style and include binding redirect patterns. This task should absorb `packages.config`/legacy project-system normalization and prepare the solution for consistent modern build semantics in subsequent tier upgrades.

**Done when**: all seven managed projects are SDK-style, solution references remain valid, and the solution builds on existing target frameworks without introducing new warning debt.
