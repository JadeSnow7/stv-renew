# Technical Decisions

## TD-001 Qt Version

- Decision: lock client app build to `Qt 6.5.3` (exact).
- Reason: reduce cross-version drift risk in Phase 1.

## TD-002 Dependency Strategy

- Decision: Windows/macOS use `vcpkg` manifest mode.
- Decision: Linux uses apt packages for Phase 1.
- Reason: fastest path to stable matrix with explicit platform policy.

## TD-003 Logging

- Decision: `spdlog` is mandatory.
- Reason: consistent observability and no fallback behavior split.

## TD-004 Crash Diagnostics

- Decision: integrate `Sentry SDK` in Phase 2.
- Reason: lower maintenance than custom crash handler while preserving symbol workflows.

## TD-005 Delivery Milestones

- Decision: 8-week two-phase model.
- Reason: separate MVP compatibility from release-hardening tasks.
