# AGENTS.md — seafloor_echoboat_project11

Instructions for AI agents working in this repository — including **GitHub
Copilot code review**, which reads this file when reviewing PRs. Coding
agents: the deep guide (packages, layout, pitfalls) is
[`.agents/README.md`](.agents/README.md); read it before making changes.

## Workspace Rules

This repo is developed inside a [ROS 2 Agent Workspace](https://github.com/rolker/ros2_agent_workspace).
The workspace root `AGENTS.md` carries the full shared rules (worktree
isolation, issue-first policy, commit conventions, AI signatures). This file
**references** those rules and adds repo-specific context only — it must
never restate or fork them.

## Quality Standard

<!-- Standalone excerpt of the workspace AGENTS.md § Quality Standard. It is
     intentionally condensed for repos reviewed without workspace context; when
     the workspace § Quality Standard changes materially, re-sync this excerpt
     (the drift ADR-0017 acknowledges). -->

This is software for autonomous robot boats operating on open water.
Robustness is not optional.

- Fix bugs completely: add the test, handle the edge case, check the
  lifecycle transition.
- Concerns about error handling, silent failures, stale data, or missing
  validation are not nits — flag them unless the failure mode genuinely
  cannot occur. "Config is under our control" and "pathological input" are
  not blanket dismissals; field configs change under pressure.
- A change includes its consequences: tests, documentation, and dependent
  references update in the same PR.

## Reviewing PRs

- If the PR carries a work plan (`.agent/work-plans/issue-<N>/plan.md` or a
  plan in the PR body), the plan is kept **in sync with the implementation
  as it evolves** — an implementation that matches the current plan text is
  not "plan drift", even if the plan changed after the PR opened.
- Verify claims against source: parameters, topics, services, and message
  types in docs must match the code.

## Review Context — seafloor_echoboat_project11

- **Safety-critical**: `echo_helm` arms the FCU and commands the live boat
  (velocity setpoints / RC override). Standby-transition arming logic and
  cmd_vel gating deserve close review.
- **nav2 overlay comments are the tuning record** — provenance annotations
  in `config/nav2_params.{160,240}.yaml` (URDF-derived footprints,
  field-tune dates, snap-roll watch items) are load-bearing; treat their
  removal as a defect.
- **Param compose semantics**: lists/footprint strings replace wholesale in
  `launch/param_compose.py`; partial-array overlay edits are bugs.
- **Generic vs instance**: per-boat values belong in
  `unh_echoboats_project11`, not here — flag boat-instance leakage into
  this generic repo.
- `launch/nav2_bringup_launch.py` is adapted upstream nav2 code
  (Apache-2.0 header) inside otherwise-BSD packages; keep its header.
