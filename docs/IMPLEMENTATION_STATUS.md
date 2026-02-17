# Implementation Status

## Summary

Cross-platform plan has been switched to **v2 (8-week, 2-phase)** with Windows/macOS priority.

## Phase Board

### Phase 1 (MVP, Week 1-4)

- [x] Build policy updated (`Qt 6.5.3`, `spdlog mandatory`)
- [x] `vcpkg.json` + `CMakePresets.json`
- [x] Compiler warnings module (`cmake/CompilerWarnings.cmake`)
- [x] `PathService` abstraction and platform implementations
- [x] `TokenStorage` file-based implementation
- [x] `AuthPresenter` token restore/save hookup
- [x] New tests: `test_path_service`, `test_token_storage`
- [x] CI matrix for client + server across Windows/macOS/Linux
- [x] Platform documentation baseline

### Phase 2 (Production readiness, Week 5-8)

- [ ] MSI packaging flow
- [ ] DMG packaging flow
- [ ] Signing and notarization automation
- [ ] Sentry integration + symbol upload
- [ ] Performance baseline and release checklist

## Known Gaps

1. Client `HttpBackendApi` and SSE runtime parser are still placeholders.
2. Server persistence is still in-memory for now.
3. Packaging/signing tasks are documented but not yet implemented.
