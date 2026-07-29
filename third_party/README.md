# Vendored third-party dependencies

These dependencies are included directly in the DroidStar source tree so the
Android ARM64 build does not depend on nested Git repositories or submodules.

## Boost headers

- Source: https://github.com/yuzu-mirror/ext-boost.git
- Commit: `717900f94b5e9acbc69377cdeee9d7f952ff725c`
- Local path: `third_party/ext-boost`

This is used as a header-only Boost source tree by the Android build.

## md380_vocoder_dynarmic

- Source: https://github.com/nostar/md380_vocoder_dynarmic.git
- Commit: `55c9a2c6d502567380c7dfaf4a2e3cc4dd6d1dd5`
- Local path: `third_party/md380_vocoder_dynarmic`

This provides the MD380 software vocoder through Dynarmic for Android ARM64.

## Updating

To update either dependency, replace its source directory with the desired
upstream revision, remove any nested `.git` metadata, and update the source
URL and commit recorded above.
