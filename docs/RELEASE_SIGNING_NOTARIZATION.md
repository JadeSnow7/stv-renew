# Release Signing and Notarization (Phase 2)

## Scope

This document defines Phase 2 release security steps:

1. Windows executable/package signing.
2. macOS app signing + notarization + staple.
3. Symbol files upload for crash diagnostics.

## Windows

### Signing

```powershell
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a path\to\stv_app.msi
```

### Verification

```powershell
signtool verify /pa /v path\to\stv_app.msi
```

## macOS

### Sign app bundle

```bash
codesign --deep --force --verify --verbose \
  --options runtime \
  --sign "Developer ID Application: <TEAM>" stv_app.app
```

### Notarize

```bash
xcrun notarytool submit stv_app.dmg \
  --apple-id "$APPLE_ID" \
  --team-id "$TEAM_ID" \
  --password "$APP_SPECIFIC_PASSWORD" \
  --wait
```

### Staple

```bash
xcrun stapler staple stv_app.dmg
```

## Symbol Upload

1. Windows: upload PDB files.
2. macOS: upload dSYM bundles.
3. Keep symbol retention aligned with release retention policy.

## Rollback

1. Revoke affected release artifact.
2. Publish previous signed version.
3. Announce rollback window and known issue.
