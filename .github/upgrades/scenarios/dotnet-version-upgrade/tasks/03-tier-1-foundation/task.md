# 03-tier-1-foundation: Upgrade Atf.Core to net10 and resolve foundational API changes

Upgrade `Atf.Core` as the leaf foundation project to `net10.0`, update compatibility settings, and apply required API migrations for this tier. Because multiple higher tiers depend on this library, it must be stabilized first and validated before any downstream tier is upgraded.

Assessment identified API incompatibilities in this project, so simple replacements are applied inline while complex replacements may use temporary compilable stubs when needed to keep the tier moving. This task establishes the first modern-runtime foundation for all dependent projects.

**Done when**: `Atf.Core` targets `net10.0`, tier build is green, and dependent projects still compile on their existing framework until their own upgrade tasks.
