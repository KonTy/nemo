![build](https://github.com/linuxmint/nemo/actions/workflows/build.yml/badge.svg)

smplos-nemo
===========

**smplos-nemo** is an enhanced fork of [Nemo](https://github.com/linuxmint/nemo), the file manager for the Cinnamon desktop environment. While we actively contribute improvements upstream, this fork exists because many of our features have been rejected or deemed out-of-scope by the upstream maintainers.

This fork is maintained for the **smplos** Linux distribution, ensuring users get a feature-rich file manager with modern UX improvements, better device support, and advanced preview capabilities.

## Why Fork?

We've implemented numerous features that enhance the file manager experience:
- **Preview Pane** with GPS mapping, image/video preview, and metadata display
- **Disk Usage Overview** with interactive Pareto charts and navigation
- **MTP Device Support** for seamless phone/tablet access
- **Archive Browsing** to explore ZIP/7z/TAR contents like folders
- **Archive Creation** with right-click compression to 7z
- **Substring Search** for smarter file discovery
- **Configurable Keyboard Shortcuts** for power users
- **Tab-based Pane Splitting** for advanced multitasking

While we've contributed some improvements upstream, several features were rejected as out-of-scope or too niche for the core Nemo project. Rather than compromise the user experience, we maintain this enhanced version for smplos users.

## Installation

### From smplos Distribution
```bash
sudo pacman -S smplos-nemo
```

### Building from Source
```bash
git clone https://github.com/KonTy/nemo.git
cd nemo
meson build -Dsmplos_branding=true
ninja -C build install
```

---

Nemo — Original Project
========================

Nemo is a free and open-source file manager for the Cinnamon desktop environment. 
It is a fork of GNOME Files (formerly named Nautilus).

Nemo also manages the Cinnamon desktop.
Since Cinnamon 6.0 (Mint 21.3), users can enhance Nemo with Spices named Actions.

### History

Nemo started as a fork of GNOME's Nautilus v3.4. Version 1.0.0 was released in July 2012 alongside Cinnamon 1.6,
reaching version 1.1.2 in November 2012.

Developer Gwendal Le Bihan named the project "nemo" after Jules Verne's famous character Captain Nemo.

### Original Features

1. Ability to SSH into remote servers
2. Native support for FTP (File Transfer Protocol) and MTP (Media Transfer Protocol)
3. All the features Nautilus 3.4 had that are missing in Nautilus 3.6 (desktop icons, compact view, etc.)
4. Open in terminal (integral part of Nemo)
5. Open as root (integral part of Nemo)
6. Uses GVfs and GIO for filesystem abstraction
7. File operations progress information in window title and taskbar
8. Proper GTK bookmarks management
9. Full navigation options (back, forward, up, refresh)
10. Toggle between path entry and breadcrumb navigation widgets
11. Extensive configuration options

---

smplos-nemo Features
====================

## Preview Pane — Live File Preview & Metadata

The Preview Pane provides a live file preview and metadata display directly inside the file manager window. Toggle it with **Alt+F3** or click the preview button in the status bar.

### Features

- **Image Preview**: Display JPEG, PNG, GIF, WebP, and other image formats with automatic scaling
- **Video Thumbnails**: Show video preview frames with duration and codec information  
- **GPS Map Preview**: Display OpenStreetMap tiles for geotagged images with a crosshair marker at shot location
  - Click the map to open in OpenStreetMap browser
  - Tiles cached locally in `~/.cache/nemo/map-tiles/` for instant subsequent loads
  - Works fully offline-safe — no internet errors or UI disruption
  - No API keys or external dependencies required
- **Metadata Display**: View EXIF data, file info, timestamps, permissions, and more
- **Adjustable Width**: Resize the pane with **Ctrl+[** (shrink) / **Ctrl+]** (grow)
- **Toggle Details**: Press **Shift+Alt+F3** to show/hide the metadata panel within the preview

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| **Alt+F3** | Toggle the Preview Pane on/off |
| **Shift+Alt+F3** | Toggle metadata/details panel within Preview Pane |
| **Ctrl+[** | Shrink the Preview Pane (narrower) |
| **Ctrl+]** | Grow the Preview Pane (wider) |

---

## Disk Usage Overview — Interactive Dashboard

Access the **Overview** entry at the top of the sidebar under *My Computer* to see a full disk-usage dashboard without leaving the file manager.

### Features

- **Donut Charts**: One per mounted volume showing used vs. free space with percentage labels
- **Deep Scan**: Automatic full recursive disk scan (equivalent to `du -a | sort -nr | head`) runs in the background
- **Pareto Bar Chart**: Horizontal bar per top offender, scaled to the largest item
- **Hover Tooltips**: Full path and human-readable size on bar hover
- **Interactive Directory List**: Click any top offender to navigate directly to that folder
- **Side-by-Side Layout**: Charts and directory lists displayed horizontally to minimize vertical space
- **Bookmark/Anchor System**: Click a volume heading to bookmark it; navigating away and returning scrolls back to the same volume
- **Process-wide Cache**: Scan results shared across all windows and tabs
- **Periodic Rescan**: Cache refreshes automatically every 120 seconds in the background
- **Full Navigation Integration**: Back/forward buttons, sidebar clicks, and address bar entries all work with `overview://`

---
## Archive Support — Browse and Create Archives

Transparently browse archive contents as folders and create compressed archives with a single click.

### Archive Browsing

Double-click any archive to explore its contents like a regular folder:

- **Supported Formats**: ZIP, 7z, TAR, TAR.GZ, TAR.BZ2, TAR.XZ, RAR
- **FUSE Mounting**: Uses fuse-zip and archivemount for transparent access
- **Navigation**: Browse, preview files, and copy/extract contents like normal folders
- **Automatic Mount Points**: Archives mounted in `/run/user/$UID/nemo-archives/`
- **Graceful Errors**: User-friendly dialogs when tools are missing

### Archive Creation

Right-click files/folders and select **"Compress to Archive"** to create compressed archives:

- **Interactive Dialog**: Choose custom archive names
- **Progress Feedback**: Real progress via `pv` (bytes processed)
- **Progress Feedback**: Visual progress bar during compression
- **Overwrite Protection**: Prompts before replacing existing archives
- **Multiple Selection**: Compress multiple files/folders at once

### Dependencies

Install the following tools for full functionality:

```bash
# Arch Linux
sudo pacman -S fuse-zip pv gzip zenity

# Ubuntu/Debian
sudo apt install fuse-zip archivemount pv gzip zenity
```

---

## MTP Device Support — Access Android Phones & Tablets

Seamless support for Android devices and other MTP-capable devices, making them appear in the Devices section just like USB drives.

### Features

- **Phone/Tablet Detection**: Automatically detects MTP devices when connected
- **Device Mounting**: One-click mounting of Android internal storage and SD cards
- **Permission Handling**: Helpful hints when device needs to be unlocked or set to "File Transfer" mode
- **Missing Backend Detection**: Shows informative messages when `gvfs-mtp` backend isn't installed
  - Suggests installation command for the current distribution (Arch, Debian, Ubuntu, etc.)
- **Cross-Distribution Compatibility**: Correctly detects gvfsd-mtp on both Arch and Debian systems
- **Volume Filtering**: Internal non-removable drives (like Samsung SSDs) are excluded from Devices list

### Usage

1. Connect your Android phone via USB
2. Set phone to "File Transfer" (MTP) mode (not "Charging only")
3. Unlock your phone when prompted
4. Device appears in Nemo's Devices section
5. Click to mount and browse Internal storage and SD card
6. Copy files to/from your device

---

## Additional Features

| Feature | Description |
|---------|-------------|
| **Configurable Keyboard Shortcuts** | All keyboard shortcuts are editable via Preferences; customize your workflow |
| **Substring Search** | Interactive filename search matches characters anywhere in the filename, not just at the start; press **Ctrl+F** to search |
| **Copy Path** | *Copy Path* entry in right-click context menu; quickly copy full paths to clipboard |
| **Tab-based Pane Splitting** | Press **Tab** to switch focus between left and right split panes; press **Ctrl+N** to open new split pane |
| **Sidebar Context Menus** | Right-click on sidebar items for context actions; **Insert** / **F4** keybindings for quick access |
| **Memory Leak Fixes** | Several memory leaks in the original codebase have been corrected |
| **USB Copy Performance** | Throttling and lockup fixes for large file transfers to USB devices |
| **Use-After-Free Fixes** | Fixed crash when renaming files in certain conditions |

---

## Upstream vs. smplos-nemo Comparison

| Feature | Upstream Nemo | smplos-nemo |
|---------|---------------|------------|
| Preview Pane | ❌ Not implemented | ✅ Full image/video/GPS preview |
| Disk Overview | ❌ Not available | ✅ Interactive Pareto charts |
| MTP Device Support | ⚠️ Via GVFS only | ✅ Enhanced with helpful hints |
| Substring Search | ❌ Not implemented | ✅ Full implementation |
| Tab Pane Switching | ❌ Not available | ✅ Tab key navigation |
| Configurable Shortcuts | ❌ Not available | ✅ Full customization |
| Performance Fixes | ⚠️ Upstream varies | ✅ USB throttling, memory leaks fixed |

---

Contributing & Development
============================

### For smplos Users
If you find bugs or have feature requests specific to smplos-nemo, please open an issue on the [smplos-nemo GitHub repository](https://github.com/KonTy/nemo).

### For Upstream Contributors
We actively cherry-pick suitable improvements to contribute upstream. If you have a feature that you believe fits upstream Nemo's scope, we can help prepare a PR to [linuxmint/nemo](https://github.com/linuxmint/nemo).

### Building from Source

```bash
# Clone the repository
git clone https://github.com/KonTy/nemo.git
cd nemo

# Build with smplos branding (creates smplos-nemo binary)
meson build -Dsmplos_branding=true
ninja -C build

# Run it
./build/src/smplos-nemo

# Install
sudo ninja -C build install
```

---

## License

Nemo and smplos-nemo are free software released under the GNU General Public License v2.0 or later. See COPYING for details.

---

**smplos-nemo is maintained with ❤️ for the smplos community.**  
**Upstream Nemo is maintained by the Linux Mint team.**