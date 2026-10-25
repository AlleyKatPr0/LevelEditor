# .NET Version Upgrade

## Preferences
- **Flow Mode**: Automatic
- **Target Framework**: net10.0

## Upgrade Options

### Strategy
- Upgrade Strategy: Bottom-Up

### Project Structure
- Project Approach: In-place
- Package Management: Per-Project (defer CPM to post-migration)

### Compatibility
- Unsupported API Handling: Defer Complex Changes
- Windows Native APIs: Windows Compatibility Pack
- System.Web Adapters: Direct Migration to ASP.NET Core APIs

### Modernization
- Assembly Binding Redirects: Remove Binding Redirects
- Nullable Reference Types: Enable Nullable Reference Types

## Strategy
**Selected**: Bottom-Up (Dependency-First)
**Rationale**: Multiple .NET Framework projects with a deep dependency chain require dependency-ordered migration with validation per tier.

### Execution Constraints
- Complete SDK-style conversion before any target framework upgrades.
- Upgrade projects in strict dependency order (leaf projects first, root application last).
- Validate each tier before moving to the next tier (build, then relevant tests).
- For complex API replacements, allow temporary compilable stubs and resolve via follow-up work in the same tier.
- Keep package versions per-project during migration; defer CPM adoption to post-migration cleanup.

## User Preferences
### Technical Preferences
- **Nullable Reference Types**: Enable during this upgrade

## Source Control
- **Source Branch**: master
- **Working Branch**: upgrade-dotnet-10
- **Commit Strategy**: After Each Task
- **Branch Sync**: Auto (Merge)
