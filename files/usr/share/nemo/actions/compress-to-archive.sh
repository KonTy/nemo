#!/bin/bash
# compress-to-archive.sh - Create a tar.gz archive with real progress
# Used by Nemo action

set -u

# Ensure required tools exist
for tool in tar pv gzip zenity; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        zenity --error --title="Compression Failed" --text="Missing required tool: $tool" 2>/dev/null || \
        notify-send "Compression Failed" "Missing required tool: $tool"
        exit 1
    fi
done

# Get the first file/folder to determine the parent directory
FIRST_FILE="$1"
PARENT_DIR="$(dirname "$FIRST_FILE")"

# Generate default archive name based on selection
if [ $# -eq 1 ]; then
    # Single file/folder - use its name
    DEFAULT_NAME="$(basename "$FIRST_FILE" | sed 's/\.[^.]*$//')"
else
    # Multiple items - use parent folder name with timestamp
    DEFAULT_NAME="archive-$(date +%Y%m%d-%H%M%S)"
fi

# Ask user for archive name
ARCHIVE_NAME=$(zenity --entry \
    --title="Create Archive" \
    --text="Enter archive name (without extension):" \
    --entry-text="$DEFAULT_NAME" 2>/dev/null)

# Exit if user cancelled
if [ -z "$ARCHIVE_NAME" ]; then
    exit 0
fi

# Add .tar.gz extension if not present
if [[ ! "$ARCHIVE_NAME" =~ \.tar\.gz$ ]]; then
    ARCHIVE_NAME="${ARCHIVE_NAME}.tar.gz"
fi

# Build full archive path
ARCHIVE_PATH="${PARENT_DIR}/${ARCHIVE_NAME}"

# Check if archive already exists
if [ -f "$ARCHIVE_PATH" ]; then
    zenity --question \
        --title="Archive Exists" \
        --text="Archive '$ARCHIVE_NAME' already exists. Overwrite?" 2>/dev/null
    if [ $? -ne 0 ]; then
        exit 0
    fi
    rm -f "$ARCHIVE_PATH"
fi

# Create array of basenames for files to compress
ITEMS=()
for file in "$@"; do
    ITEMS+=("$(basename "$file")")
done

# Calculate total size for progress (bytes)
TOTAL_BYTES=$(du -sb --apparent-size "$@" 2>/dev/null | awk '{sum+=$1} END {print sum}')
if [ -z "$TOTAL_BYTES" ] || [ "$TOTAL_BYTES" -le 0 ]; then
    TOTAL_BYTES=1
fi

# Change to parent directory and compress
cd "$PARENT_DIR"

# Run tar + pv + gzip with real progress
LOG_FILE="/tmp/archive_output_$$.log"
ERROR_DISPLAY="/tmp/archive_error_$$.txt"

(
    tar -cf - "${ITEMS[@]}" 2>"$LOG_FILE" | \
    pv -n -s "$TOTAL_BYTES" | \
    gzip -c > "$ARCHIVE_PATH"
) 2>>"$LOG_FILE" | \
zenity --progress \
    --title="Creating Archive" \
    --text="Compressing files to $ARCHIVE_NAME..." \
    --percentage=0 \
    --auto-close 2>/dev/null

EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ] && [ -f "$ARCHIVE_PATH" ]; then
    ARCHIVE_SIZE=$(du -h "$ARCHIVE_PATH" | cut -f1)
    zenity --info \
        --title="Compression Complete" \
        --text="Archive created successfully!\n\nFile: $ARCHIVE_NAME\nSize: $ARCHIVE_SIZE" 2>/dev/null || \
    notify-send "Compression Complete" "Archive created: $ARCHIVE_NAME ($ARCHIVE_SIZE)"
else
    ERROR_MSG=$(tail -50 "$LOG_FILE")
    {
        echo "Compression failed for: $ARCHIVE_NAME"
        echo ""
        echo "Archive path: $ARCHIVE_PATH"
        echo ""
        echo "--- tar/pv/gzip output (last 50 lines) ---"
        echo "$ERROR_MSG"
    } > "$ERROR_DISPLAY"
    zenity --text-info \
        --title="Compression Failed" \
        --filename="$ERROR_DISPLAY" \
        --width=700 --height=420 \
        --editable 2>/dev/null || \
    notify-send "Compression Failed" "See $ERROR_DISPLAY for details"
fi

# Clean up temp logs
rm -f "$LOG_FILE"

exit 0
