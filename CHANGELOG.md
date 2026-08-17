# Changelog

## 0.1.0 — 2026-08-02

### Added

- Separate per-transmission RX and TX WAV recording.
- 8 kHz mono signed 16-bit PCM recording output.
- Metadata-rich recording filenames.
- Recording start, save, failure, duration, and size messages
  in the Log tab.
- In-app recording library with metadata, duration, file-size,
  sorting, and RX/TX filtering.
- Play, pause, stop, seeking, and automatic end-of-file
  handling.
- Automatic library refresh after a recording is saved.
- Files-app access to local recordings under
  `DroidStar/Recordings`.

### Privacy and behavior

- Recordings remain inside DroidStar's local application
  sandbox by default.
- No iCloud Documents capability or automatic cloud
  synchronization is enabled.
- Playback stops when leaving the Recordings tab.
- Live radio connections stop and lock out recording playback.

### Validation

- Native arm64 iOS build completed successfully.
- Physical-device recording and playback tested with DMR,
  P25, and NXDN.
- Files integration and local-only storage behavior verified.
