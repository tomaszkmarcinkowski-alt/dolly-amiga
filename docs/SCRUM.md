# Dolly development process — lightweight public Scrum

**Product goal:** A trustworthy, free Amiga disk-imaging/recovery application with 68000 baseline.

**Cadence:** Proposed 2-week iterations, adjustable to volunteer availability.

**Roles:** Maintainer owns final product decisions. Contributors select implementation stories through Issues and PRs.

**Workflow:** Backlog → Ready → In Progress → Review → Done. Track blockers separately.

**Planning:** Prioritize safety and I/O foundations before UI polish or destructive repair. Use story estimates as relative complexity, not calendar promises.

**Daily coordination:** Asynchronous updates in issue threads. No mandatory meeting schedule for volunteer work.

**Sprint review:** Show reproducible host tests, Amiga build logs and (when available) emulator screenshots.

**Retrospective:** Record what caused regressions, ambiguous requirements or excess memory use.

**Definition of Ready:** Unambiguous behavior, acceptance criteria, dependencies, failure modes and test strategy.

**Definition of Done:** Reviewed code, successful automated tests, measured memory/speed where relevant, documentation updated, safety constraints covered.

**Release policy:** No unsafe on-device writes before explicit opt-in and recovery tests; pre-release builds clearly marked experimental.
