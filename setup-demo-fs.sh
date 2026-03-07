#!/bin/bash

# Setup anonymous demo filesystem for GIF recording
# Creates temporary directories with randomized names and fake file structure

DEMO_ROOT="${1:-$HOME/nemo-demo-fs}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Setting up anonymous demo filesystem at: $DEMO_ROOT${NC}"

# Clean up if exists
if [ -d "$DEMO_ROOT" ]; then
    echo -e "${YELLOW}Removing existing demo filesystem...${NC}"
    rm -rf "$DEMO_ROOT"
fi

# Create root directories with fake names
mkdir -p "$DEMO_ROOT"

# Create fake volumes/mounts
mkdir -p "$DEMO_ROOT/Main_Storage"
mkdir -p "$DEMO_ROOT/External_Drive"
mkdir -p "$DEMO_ROOT/Mobile_Device"

# Populate Main Storage with fake files/dirs of various sizes
echo -e "${YELLOW}Populating Main Storage...${NC}"
for i in {1..8}; do
    SIZE=$((RANDOM % 500 + 50))  # 50-550 MB
    DIR_NAME="Project_$i"
    mkdir -p "$DEMO_ROOT/Main_Storage/$DIR_NAME"
    
    # Create fake files with random sizes
    for j in {1..3}; do
        FILE_SIZE=$((RANDOM % 200 + 10))  # 10-210 MB
        dd if=/dev/zero of="$DEMO_ROOT/Main_Storage/$DIR_NAME/file_$j.bin" bs=1M count=$FILE_SIZE 2>/dev/null
    done
done

# Create nested directory structures
mkdir -p "$DEMO_ROOT/Main_Storage/Archives/Archive_A/SubFolder_1"
dd if=/dev/zero of="$DEMO_ROOT/Main_Storage/Archives/Archive_A/SubFolder_1/dataset.bin" bs=1M count=300 2>/dev/null

mkdir -p "$DEMO_ROOT/Main_Storage/Archives/Archive_B"
dd if=/dev/zero of="$DEMO_ROOT/Main_Storage/Archives/Archive_B/backup.tar" bs=1M count=450 2>/dev/null

# Populate External Drive with different sizes
echo -e "${YELLOW}Populating External Drive...${NC}"
for i in {1..5}; do
    DIR_NAME="Media_Set_$i"
    mkdir -p "$DEMO_ROOT/External_Drive/$DIR_NAME"
    for j in {1..2}; do
        FILE_SIZE=$((RANDOM % 300 + 50))  # 50-350 MB
        dd if=/dev/zero of="$DEMO_ROOT/External_Drive/$DIR_NAME/media_$j.bin" bs=1M count=$FILE_SIZE 2>/dev/null
    done
done

# Populate Mobile Device (smaller files)
echo -e "${YELLOW}Populating Mobile Device...${NC}"
for i in {1..12}; do
    FILE_SIZE=$((RANDOM % 50 + 5))  # 5-55 MB
    dd if=/dev/zero of="$DEMO_ROOT/Mobile_Device/photo_$i.jpg" bs=1M count=$FILE_SIZE 2>/dev/null
done

echo -e "${GREEN}✓ Demo filesystem created successfully!${NC}"
echo -e "${YELLOW}Location: $DEMO_ROOT${NC}"
echo ""
echo -e "${YELLOW}To use in Nemo:${NC}"
echo "  1. Open Nemo and navigate to: $DEMO_ROOT"
echo "  2. Click Overview in the sidebar"
echo "  3. Record your GIF demo"
echo ""
echo -e "${YELLOW}To clean up after recording:${NC}"
echo "  rm -rf \"$DEMO_ROOT\""
echo ""
echo -e "${YELLOW}Directory structure:${NC}"
du -sh "$DEMO_ROOT"/* 2>/dev/null || true
echo "Total:"
du -sh "$DEMO_ROOT"
