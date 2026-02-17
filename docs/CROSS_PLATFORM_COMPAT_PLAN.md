# Cross-Platform Compatibility Plan v2

## Goal

Implement Windows/macOS/Linux compatibility with **Windows/macOS priority** and an 8-week two-phase delivery.

## Locked Decisions

1. Qt version: `6.5.3` fixed.
2. Dependencies: Windows/macOS via `vcpkg manifest`; Linux via `apt` pinned packages.
3. Logging: `spdlog` mandatory.
4. Crash diagnostics: `Sentry SDK` (Phase 2), no custom crash handler.
5. Milestones: Phase 1 (MVP) + Phase 2 (production readiness).

## Phase 1 (Week 1-4)

1. Build and dependency unification (`vcpkg.json`, `CMakePresets.json`, warnings policy).
2. Platform abstraction (`PathService` for Windows/macOS/Linux).
3. Token file storage and login-session restore (`TokenStorage` + `AuthPresenter` hookup).
4. CI matrix for three platforms (build + tests).
5. Documentation baseline for each platform.

## Phase 2 (Week 5-8)

1. Windows MSI packaging (`windeployqt + CPack/WiX`).
2. macOS app/DMG packaging and notarization (`codesign/notarytool/stapler`).
3. Sentry integration + symbol upload (`PDB/dSYM`).
4. Performance baseline and release runbook.

## Scope

- In: client compatibility, build/test matrix, packaging/signing flow docs.
- In: backend developer runbook for Windows/macOS.
- Out (Phase 1): native secure credential storage, advanced perf tuning.
- Out (overall): backend production operations on Windows/macOS.

## Acceptance Gates

### Phase 1

1. Build succeeds on Windows/macOS/Linux for core+infra+tests.
2. Unit tests pass including `PathService` and `TokenStorage`.
3. MVP user flow works: login -> create project -> edit storyboard -> start job -> SSE updates -> cancel.
4. CI matrix green on three platforms.

### Phase 2

1. MSI/DMG/AppImage artifacts generated.
2. Signing/notarization flows reproducible.
3. Sentry crash capture and symbolication validated.
4. Install/uninstall/upgrade checks pass.
