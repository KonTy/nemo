# nemo-smpl — Features & Release Notes

**Current Version:** v1.2.0  
**Release Date:** March 9, 2026

---

## Feature Tracking

This document tracks which features in nemo-smpl are upstream and which are exclusive.

### Upstream (Merged into linuxmint/nemo)

None yet — we're working on contributing suitable features upstream.

### Pending Upstream

These features are candidates for upstream contribution:

- [ ] **Page Cache Throttle** — PR [#3726](https://github.com/linuxmint/nemo/pull/3726)
- [ ] **Configurable Keybindings** — PR [#3722](https://github.com/linuxmint/nemo/pull/3722)
- [ ] **Substring Search** — PR [#3718](https://github.com/linuxmint/nemo/pull/3718)

---

## nemo-smpl Exclusive Features

These features are developed for nemo-smpl and are not expected to be accepted upstream.

### F3 Quick Preview

Double Commander-style instant file viewer:

- Text, image (including animated GIFs), audio/video (GStreamer), and hex dump modes
- Directory analysis: F3 on a folder shows Pareto bar chart + ranked biggest-files list
- Paged text/hex viewer using `pread()` + LRU cache — handles multi-GB files with ~512 KB resident
- Escape to dismiss; singleton window reused across invocations
- Split view moved to Ctrl+F3
- Modular architecture: `NemoImageViewer`, `NemoPagedViewer`, `NemoPreviewUtils` shared between sidebar pane and quick preview

### Shared Directory Analyzer Widget

- Reusable `NemoDirAnalyzer` widget for directory-size Pareto analysis
- Used by both the Overview page (per-volume) and F3 Quick Preview (per-folder)
- Vertical bar chart + ranked list with clickable paths
- Background async scan with `"scan-finished"` signal

### Verify After Copy/Move

- Checkbox in F5 (Copy) and F6 (Move) dialogs: "Verify after copy/move"
- SHA-256 checksum comparison of source and destination after each file
- Bypasses page cache with `posix_fadvise(DONTNEED)` for true on-disk verification
- `fsync` before verification to ensure data is flushed to disk
- Mismatch dialog with Cancel/Skip options

### Per-Pane Location Labels

- Compact path label above each pane in dual-pane mode
- Tilde-shortened paths (e.g. `~/Documents`) or URIs for non-local locations
- Configurable via GSettings key `show-dual-pane-location-labels` (default: on)
- Preference checkbox under Views → Behavior

### Preview Pane (Alt+F3)

- Live image/video preview with EXIF metadata display
- GPS map display for geotagged photos (OpenStreetMap)
- Adjustable preview width (Ctrl+[ / Ctrl+])

### Disk Usage Overview

- Interactive Pareto charts for each mounted volume
- Deep directory scan with top offenders list
- Side-by-side layout (chart + list) with bookmark/anchor navigation
- Background cache with periodic refresh

### smplOS Live Theming

- Accent colors, backgrounds, and selection highlights update instantly via `theme-set`
- `GFileMonitor` watches `~/.config/smplos/nemo-theme.css` for live CSS reload
- All 15 smplOS themes ship pre-baked `nemo.css` files
- Compiled under `#ifdef SMPLOS` — upstream patches contain none of this code

### Archive Support

- **Browsing**: Double-click ZIP/7z/TAR archives to browse contents via FUSE
- **Creation**: Right-click "Compress to Archive" with progress feedback
- Supports ZIP, 7z, TAR, TAR.GZ, TAR.BZ2, TAR.XZ, RAR

### MTP Device Support

- Automatic udev rules prevent gphoto2 conflicts
- Retry logic for transient USB "device busy" errors
- Clear error messages: "Phone locked?", "Driver missing?", "Device busy"
- Covers Samsung, Google Pixel, HTC, LG, Sony, Motorola

### Command-Line Flags

- `--class` / `-c`: Set custom WM_CLASS at launch (for compositor floating rules)
- `--select` / `-s`: Open parent directory and highlight a specific file

### Other Enhancements

- **Configurable Keyboard Shortcuts** — edit all keybindings via preferences
- **Substring Search** — match anywhere in filename, not just prefix
- **Tab-based Pane Splitting** — Tab to switch focus, Ctrl+N for new split pane
- **Copy Path** — right-click "Copy Path" to clipboard
- **Cover Art Directory Icons** — directories with `cover.jpg`/`cover.png` use them as folder icons
- **Performance Fixes** — USB copy throttling, memory leak corrections, use-after-free crash fixes

---

## Version History

### v1.2.0 (March 2026)

- **F3 Quick Preview**: instant file viewer (text, image, media, hex, directory analysis)
- **Shared NemoDirAnalyzer widget**: directory-size Pareto analysis reused by Overview + F3
- **Verify after copy/move**: SHA-256 checksum checkbox in F5/F6 dialogs
- **Per-pane location labels**: compact path display above each pane in dual-pane mode
- **Cover art directory icons**: cherry-picked from upstream PR #3728
- **Keybinding changes**: F3 → Quick Preview, Ctrl+F3 → Split View
- Modular preview architecture: NemoImageViewer, NemoPagedViewer, NemoPreviewUtils
- Overview page refactored — ~350 lines of duplicated scan code removed
- Debian CI build fixed (LMDE 7 image)

---

## Contributing

### Feature Requests

[Open an issue](https://github.com/KonTy/nemo/issues) on the nemo-smpl repository.

### Development

1. Create a feature branch: `git checkout -b feature/my-feature`
2. Implement and test locally
3. Create a PR against the `release` branch
4. If suitable for upstream, we'll prepare a clean PR to [linuxmint/nemo](https://github.com/linuxmint/nemo)

---

**Maintained by:** smplOS Development Team
