# Demo Shotlist (5 Animated GIFs)

This is the production checklist for README visuals.

## Existing assets found

- [Documents/map_preview.png](Documents/map_preview.png)
- [Documents/key bindings.png](Documents/key%20bindings.png)

## GIF Plan (one by one)

1. **Preview Pane + GPS Map**
   - File target: [Documents/media/gifs/01-preview-gps.gif](Documents/media/gifs/01-preview-gps.gif)
   - Show: open geotagged image, toggle preview (`Alt+F3`), map appears, metadata panel toggle

2. **Video Preview**
   - File target: [Documents/media/gifs/02-video-preview.gif](Documents/media/gifs/02-video-preview.gif)
   - Show: select mp4/mkv, thumbnail/frame preview and duration/codec

3. **Archive Browsing**
   - File target: [Documents/media/gifs/03-archive-browse.gif](Documents/media/gifs/03-archive-browse.gif)
   - Show: open zip/7z as folder, navigate inside, drag one file out

4. **Disk Usage Overview**
   - File target: [Documents/media/gifs/04-disk-usage.gif](Documents/media/gifs/04-disk-usage.gif)
   - Show: open overview, click chart segment, navigate to large folder

5. **Custom Keybindings + Workflow Speed**
   - File target: [Documents/media/gifs/05-keybindings.gif](Documents/media/gifs/05-keybindings.gif)
   - Show: open keybindings editor (`Ctrl+Shift+K`), remap one action, use it immediately

## Recording recommendations

- Record at 1920x1080, then export GIF at width 1280
- Keep each GIF to 5–8 seconds
- Prefer 12 fps for quality/size balance
- Avoid cursor jitter; use deliberate pauses

## Convert raw recordings to GIF

Raw clips should go to [Documents/media/raw](Documents/media/raw).

Example:

./support/make-demo-gif.sh Documents/media/raw/01-preview-gps.mp4 Documents/media/gifs/01-preview-gps.gif 00:00:01 6 12 1280
