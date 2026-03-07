#!/usr/bin/env bash
set -euo pipefail

# Convert a short demo video clip into a lightweight GIF.
# Usage:
#   ./support/make-demo-gif.sh input.mp4 output.gif [start] [duration] [fps] [width]
# Example:
#   ./support/make-demo-gif.sh Documents/media/raw/01-preview-pane.mp4 Documents/media/gifs/01-preview-pane.gif 00:00:01 6 12 1280

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <input-video> <output-gif> [start] [duration] [fps] [width]"
  exit 1
fi

INPUT="$1"
OUTPUT="$2"
START="${3:-00:00:00}"
DURATION="${4:-6}"
FPS="${5:-12}"
WIDTH="${6:-1280}"

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg is required but not installed."
  exit 1
fi

PALETTE="$(mktemp --suffix=.png)"
trap 'rm -f "$PALETTE"' EXIT

mkdir -p "$(dirname "$OUTPUT")"

# 1) Build optimal palette
ffmpeg -y -ss "$START" -t "$DURATION" -i "$INPUT" \
  -vf "fps=${FPS},scale=${WIDTH}:-1:flags=lanczos,palettegen=stats_mode=diff" \
  "$PALETTE"

# 2) Render gif with that palette
ffmpeg -y -ss "$START" -t "$DURATION" -i "$INPUT" -i "$PALETTE" \
  -lavfi "fps=${FPS},scale=${WIDTH}:-1:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=3" \
  "$OUTPUT"

echo "GIF created: $OUTPUT"
