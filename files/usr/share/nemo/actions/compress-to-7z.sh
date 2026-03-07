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

# Show progress with zenity
(
    echo "# Compressing files..."
    7z a -mx=9 "$ARCHIVE_NAME" "${ITEMS[@]}" 2>&1 | while IFS= read -r line; do
        if [[ "$line" =~ ([0-9]+)% ]]; then
            echo "${BASH_REMATCH[1]}"
        fi
    done
    echo "100"
) | zenity --progress \
    --title="Creating Archive" \
    --text="Compressing files to $ARCHIVE_NAME..." \
    --percentage=0 \
    --auto-close 2>/dev/null &

ZENITY_PID=$!

# Run 7z compression
7z a -mx=9 "$ARCHIVE_NAME" "${ITEMS[@]}" > /tmp/7z_output_$$.log 2>&1
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
    # Show error
    ERROR_MSG=$(tail -10 /tmp/7z_output_$$.log)
    zenity --error \
        --title="Compression Failed" \
        --text="Failed to create archive.\n\nError:\n$ERROR_MSG" 2>/dev/null || \
    notify-send "Compression Failed" "Could not create archive"
fi

# Clean up temp log
rm -f /tmp/7z_output_$$.log
