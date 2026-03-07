#!/bin/bash
# compress-to-7z.sh - Create a 7z archive from selected files
# Used by Nemo action

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
    --title="Create 7z Archive" \
    --text="Enter archive name (without extension):" \
    --entry-text="$DEFAULT_NAME" 2>/dev/null)

# Exit if user cancelled
if [ -z "$ARCHIVE_NAME" ]; then
    exit 0
fi

# Add .7z extension if not present
if [[ ! "$ARCHIVE_NAME" =~ \.7z$ ]]; then
    ARCHIVE_NAME="${ARCHIVE_NAME}.7z"
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

# Change to parent directory and compress
cd "$PARENT_DIR"

# Start a pulsating progress dialog (avoid running 7z twice)
( while true; do echo "# Compressing files to $ARCHIVE_NAME..."; echo "50"; sleep 1; done ) | \
    zenity --progress \
    --title="Creating Archive" \
    --text="Compressing files to $ARCHIVE_NAME..." \
    --pulsate \
    --auto-close 2>/dev/null &

ZENITY_PID=$!

# Run 7z compression once and capture full log
LOG_FILE="/tmp/7z_output_$$.log"
/usr/bin/7z a -mx=9 "$ARCHIVE_NAME" "${ITEMS[@]}" > "$LOG_FILE" 2>&1
EXIT_CODE=$?

# Kill progress dialog
kill $ZENITY_PID 2>/dev/null
wait $ZENITY_PID 2>/dev/null

if [ $EXIT_CODE -eq 0 ]; then
    # Get archive size
    ARCHIVE_SIZE=$(du -h "$ARCHIVE_PATH" | cut -f1)
    zenity --info \
        --title="Compression Complete" \
        --text="Archive created successfully!\n\nFile: $ARCHIVE_NAME\nSize: $ARCHIVE_SIZE" 2>/dev/null || \
    notify-send "Compression Complete" "Archive created: $ARCHIVE_NAME ($ARCHIVE_SIZE)"
else
    # Show error with copyable text
    ERROR_MSG=$(tail -50 "$LOG_FILE")
    ERROR_DISPLAY="/tmp/7z_error_$$.txt"
    {
        echo "Compression failed for: $ARCHIVE_NAME"
        echo ""
        echo "Archive path: $ARCHIVE_PATH"
        echo ""
        echo "--- 7z output (last 50 lines) ---"
        echo "$ERROR_MSG"
    } > "$ERROR_DISPLAY"
    zenity --text-info \
        --title="Compression Failed" \
        --filename="$ERROR_DISPLAY" \
        --width=700 --height=420 \
        --editable 2>/dev/null || \
    notify-send "Compression Failed" "See $ERROR_DISPLAY for details"
fi

# Clean up temp log
rm -f "$LOG_FILE"
