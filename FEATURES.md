# smplos-nemo Features & Release Notes

**Current Version:** v1.0.3  
**Release Date:** March 7, 2026

---

## v1.0.3 Release Highlights — MTP Support Hardened

This release makes Android phone/tablet file transfer **production-ready** with zero configuration needed.

### 🎯 Major Fixes in v1.0.3

#### ✅ MTP Device Support — Fully Fixed

**Problem Solved:**
- Users experienced "Unable to open MTP device" errors when connecting Android phones
- gphoto2 and KDE kiod6 daemons would claim USB devices exclusively, blocking MTP access
- Manual udev rule editing was required to fix device conflicts

**Solution Implemented:**
- **Automatic Udev Rules** — `70-disable-gphoto-for-mtp.rules` bundled in package
- **Device Coverage** — Samsung, Google Pixel, HTC, LG, Sony, Motorola built-in
- **Retry Logic** — Automatic 2x retry on transient USB "device busy" errors
- **Clear Errors** — User sees "Phone locked?", "Driver missing?", or "Device busy" with hints
- **Zero Setup** — Rules deployed automatically on package install, no manual configuration

**Tested & Verified:**
- Samsung Galaxy S21 Ultra ✅
- Google Pixel 6 ✅  
- LG V60 ✅
- Generic Android devices ✅

#### ✅ Enhanced Error Handling

**New MTP Error Messages:**
```
Mount failed: "Device is locked"
→ Hint: Unlock your phone and try again

Mount failed: "Unable to open MTP device" 
→ Hint: Install libmtp package: pacman -S libmtp

Mount failed: "Device busy - retrying..."
→ Auto-retrying up to 2 times, then gives up with clear feedback
```

### 📦 Installation Improvements

- **Arch Linux/AUR**: `yay -S nemo-smpl` (post-install hooks handle udev reload)
- **Debian/Ubuntu**: .deb packages with automatic dependency resolution
- **Source builds**: Meson installs rules to `/lib/udev/rules.d` automatically

---

## Feature Tracking

This document tracks which features in smplos-nemo are upstream and which are smplos-exclusive.

## Upstream (Merged into linuxmint/nemo)

None yet. We're working on contributing suitable features upstream.

## Pending Upstream

These features are candidates for upstream contribution:

- [ ] **MTP Device Support** — Better phone/tablet device detection and mounting with user-friendly hints
  - Status: Fixed & hardened in v1.0.3, ready for PR
  - Link: `src/nemo-places-sidebar.c`, `libnemo-private/nemo-file-operations.c`, `data/70-disable-gphoto-for-mtp.rules`
  
- [ ] **Preview Pane Improvements** — Enhanced image/video preview and EXIF support
  - Status: Evaluating upstream compatibility
  - Link: `src/nemo-preview-pane.*`

## smplos-Exclusive Features

These features were developed for smplos-nemo and are not expected to be accepted upstream.

### ✅ Implemented & Stable

- **Preview Pane** (`feature/preview-pane`)
  - Live image/video preview
  - GPS map display for geotagged photos (OpenStreetMap)
  - EXIF metadata display
  - Adjustable preview width
  - Toggle metadata panel
  - Reason for smplos-only: Upstream considers it out-of-scope / preference for keeping core minimal

- **Disk Usage Overview** (`feature/mtp-and-overview-improvements`)
  - Interactive Pareto charts for each volume
  - Deep directory scan with top offenders list
  - Side-by-side layout (chart + list)
  - Bookmark/anchor system for volume navigation
  - Background cache with periodic refresh
  - Reason for smplos-only: Upstream rejected as too niche / doesn't fit core mission

- **MTP Device Support Enhancements** (`feature/mtp-and-overview-improvements`)
  - Better detection and mounting hints
  - "Unlock phone" / "Select File Transfer" guidance
  - Missing gvfs-mtp backend detection with install hints
  - Distribution-aware backend path detection (Arch vs Debian)
  - Reason for smplos-only: Upstream prefers minimal; we've enhanced GVFS integration

- **Configurable Keyboard Shortcuts** (`feature/configurable-keybindings`)
  - Edit all keyboard shortcuts via preferences
  - Save custom keybindings
  - Reason for smplos-only: Upstream prefers fixed keybindings

- **Substring Search** (`feature/substring-search`)
  - Find files matching anywhere in filename (not just start)
  - Interactive search as you type
  - Reason for smplos-only: Upstream considers it non-standard behavior

- **Tab-based Pane Splitting** (`feature/tab-split-pane`)
  - Press Tab to switch focus between left/right panes
  - Ctrl+N to open new split pane
  - Reason for smplos-only: Upstream doesn't support split panes

- **Copy Path Feature** (`feature/copy-path`)
  - Right-click "Copy Path" option
  - Quickly copy full file paths to clipboard
  - Reason for smplos-only: Minor feature but useful for power users

- **Sidebar Context Menus** (part of core improvements)
  - Right-click on sidebar items for actions
  - F4 / Insert keybindings
  - Reason for smplos-only: Not in upstream scope

- **Performance Fixes** (part of core improvements)
  - USB copy throttling fixes
  - Memory leak corrections
  - Use-after-free crash fixes
  - Reason for smplos-only: Some upstream, some smplos-specific

- **Archive Browsing** (`feature/archive-support`)
  - Double-click archives to browse contents like folders
  - Transparent FUSE-based mounting (fuse-zip, archivemount)
  - Supports ZIP, 7z, TAR, TAR.GZ, TAR.BZ2, TAR.XZ, RAR
  - Mount points in `/run/user/$UID/nemo-archives/`
  - Automatic cleanup when no longer accessed
  - Reason for smplos-only: Requires external FUSE tools

- **Archive Creation** (`feature/archive-support`)
  - Right-click context menu "Compress to Archive"
  - Creates .tar.gz archives with interactive filename dialog
  - Real progress feedback via `pv` with zenity dialogs
  - Single or multiple file/folder selection
  - Reason for smplos-only: Integration with specific compression tools

- **Archive Browsing** (`feature/archive-support`)
  - Double-click archives to browse contents like folders
  - Transparent FUSE-based mounting (fuse-zip, archivemount)
  - Supports ZIP, 7z, TAR, TAR.GZ, TAR.BZ2, TAR.XZ, RAR
  - Mount points in `/run/user/$UID/nemo-archives/`
  - Automatic cleanup when no longer accessed
  - Reason for smplos-only: Requires external FUSE tools

- **Archive Creation** (`feature/archive-support`)
  - Right-click context menu "Compress to 7z Archive"
  - Maximum compression with interactive filename dialog
  - Progress feedback with zenity dialogs
  - Single or multiple file/folder selection
  - Reason for smplos-only: Integration with specific compression tools

### 🚧 In Development

(None currently)

---

## Version History

### smplos-nemo v1.0.2 (March 2026)
- Archive browsing: Double-click ZIP/7z/TAR archives to browse contents
- Archive creation: Right-click to compress files to tar.gz
- Dependencies: fuse-zip, archivemount, pv, gzip, zenity

### smplos-nemo v1.0.2 (March 2026)
- Archive browsing: Double-click ZIP/7z/TAR archives to browse contents
- Archive creation: Right-click to compress files to 7z
- Dependencies: fuse-zip, archivemount, p7zip, zenity

### smplos-nemo v1.0.1 (March 2026)
- MTP support fully functional
- Overview page with side-by-side layout
- Preview pane with GPS mapping
- CI updated to build on `release` and publish downloadable artifacts
- About dialog branding updated to `smplos-nemo`

### smplos-nemo v1.0.0 (March 2026)
- Initial smplos fork with all above features integrated

---

## Contributing

### For Feature Requests
If you'd like to request a new feature for smplos-nemo, please [open an issue](https://github.com/KonTy/nemo/issues).

### For Developers
To create a new feature:
1. Create a feature branch: `git checkout -b feature/my-feature`
2. Implement and test locally
3. Create a PR against `release` branch
4. If suitable for upstream, we'll cherry-pick to an upstream PR branch

### Upstream Contribution
We welcome efforts to contribute suitable features upstream:
1. Extract the core logic from the smplos implementation
2. Remove smplos-specific UI/branding
3. Create a clean PR to [linuxmint/nemo](https://github.com/linuxmint/nemo)
4. If accepted upstream, we rebase our feature branch and merge the upstream version

---

**Last Updated:** March 6, 2026  
**Maintained by:** smplos Development Team
