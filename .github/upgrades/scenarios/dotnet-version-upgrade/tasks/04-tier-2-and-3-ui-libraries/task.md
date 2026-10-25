# 04-tier-2-and-3-ui-libraries: Upgrade Atf.Gui and Atf.Gui.WinForms

Upgrade the UI framework tiers (`Atf.Gui`, then `Atf.Gui.WinForms`) to `net10.0` with Windows desktop compatibility enabled and required API adjustments for WinForms/System.Drawing-heavy usage. These projects carry the largest API migration burden in the solution and are prerequisites for most downstream projects.

Given the assessment’s high incompatibility counts in `Atf.Gui.WinForms`, this task should prioritize maintaining compilability at each sub-tier boundary, using deferred complex-change stubs only when necessary and tracking required follow-up in-task.

**Done when**: both projects target `net10.0`, tier-local builds and tests pass, and Tier 4 projects remain buildable on their current target until upgraded.
