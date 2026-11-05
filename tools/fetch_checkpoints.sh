#!/bin/bash

# Checkpoint Hash Fetcher Script
# Usage: ./fetch_checkpoints.sh <start_height>

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if start height is provided
if [[ $# -eq 0 ]]; then
    echo -e "${RED}Error: Please provide a start height${NC}" >&2
    echo "Usage: $0 <start_height>" >&2
    echo "Example: $0 1730000" >&2
    exit 1
fi

START_HEIGHT=$1
URL1="https://explorer.conceal.network/daemon"
URL2="https://conceal.nodesandbits.tech"

# Create output directory
OUTPUT_DIR="checkpoints_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT_DIR"
OUTPUT_FILE="$OUTPUT_DIR/checkpoints.txt"

echo -e "${GREEN}Starting checkpoint hash collection...${NC}"
echo "Start height: $START_HEIGHT"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Function to get current blockchain height
get_current_height() {
    local height=$(curl -s "$URL1/getheight" | grep -o '"height":[0-9]*' | cut -d':' -f2)
    if [[ -z "$height" ]]; then
        echo -e "${RED}Error: Could not fetch current height from $URL1${NC}" >&2
        exit 1
    fi
    echo $height
}

# Function to fetch block hash from any URL
fetch_hash() {
    local url=$1
    local height=$2
    local response=$(curl -s -X POST "$url/json_rpc" \
        -H "Content-Type: application/json" \
        -d "{\"jsonrpc\":\"2.0\",\"id\":\"blockbyheight\",\"method\":\"getblockheaderbyheight\",\"params\":{\"height\":$height}}")
    
    local hash=$(echo "$response" | grep -o '"hash":"[a-f0-9]*"' | cut -d'"' -f4)
    echo "$hash"
}

# Get current height and calculate end height
echo -e "${YELLOW}Fetching current blockchain height...${NC}"
CURRENT_HEIGHT=$(get_current_height)
echo "Current height: $CURRENT_HEIGHT"

# Calculate end height (nearest 10000 increment, minus 10000)
END_HEIGHT=$(( ((CURRENT_HEIGHT / 10000) - 1) * 10000 ))
echo "End height: $END_HEIGHT"
echo ""

# Initialize counters
total_blocks=0
successful_blocks=0

echo -e "${GREEN}Starting hash collection...${NC}"
echo ""

# Main loop
for ((height=START_HEIGHT; height<=END_HEIGHT; height+=10000)); do
    echo -n "Processing height $height... "
    
    # Fetch hash from URL1
    hash1=$(fetch_hash "$URL1" $height)
    if [[ -z "$hash1" ]]; then
        echo -e "${RED}FAILED (URL1)${NC}"
        echo -e "${RED}Stopping collection due to failed request${NC}"
        break
    fi
    
    # Fetch hash from URL2
    hash2=$(fetch_hash "$URL2" $height)
    if [[ -z "$hash2" ]]; then
        echo -e "${RED}FAILED (URL2)${NC}"
        echo -e "${RED}Stopping collection due to failed request${NC}"
        break
    fi
    
    # Compare hashes - CRITICAL: if they don't match, stop immediately
    if [[ "$hash1" = "$hash2" ]]; then
        echo -e "${GREEN}MATCH${NC}"
        ((successful_blocks++))
        
        # Write to output file
        echo "{$height, \"$hash1\"}," >> "$OUTPUT_FILE"
    else
        echo -e "${RED}MISMATCH - STOPPING COLLECTION${NC}"
        echo "Height: $height"
        echo "  URL1: $hash1"
        echo "  URL2: $hash2"
        echo ""
        echo -e "${RED}Hash mismatch detected. Collection stopped for safety.${NC}"
        echo -e "${RED}Only $successful_blocks blocks were verified and saved.${NC}"
        
        # Remove the output file since it's incomplete
        rm -f "$OUTPUT_FILE"
        
        exit 1
    fi
    
    ((total_blocks++))
    
    # Small delay to be respectful to the servers
    sleep 0.5
done

echo ""
echo -e "${GREEN}=== Collection Complete ===${NC}"
echo "Total blocks processed: $total_blocks"
echo -e "${GREEN}Successfully verified blocks: $successful_blocks${NC}"
echo ""
echo "Output file: $OUTPUT_FILE"

# Create a summary file
SUMMARY_FILE="$OUTPUT_DIR/summary.txt"
echo "Checkpoint Collection Summary" > "$SUMMARY_FILE"
echo "=============================" >> "$SUMMARY_FILE"
echo "Start height: $START_HEIGHT" >> "$SUMMARY_FILE"
echo "End height: $END_HEIGHT" >> "$SUMMARY_FILE"
echo "Total blocks processed: $total_blocks" >> "$SUMMARY_FILE"
echo "Successfully verified blocks: $successful_blocks" >> "$SUMMARY_FILE"
echo "Collection date: $(date)" >> "$SUMMARY_FILE"
echo "Status: COMPLETE - All hashes verified" >> "$SUMMARY_FILE"

echo ""
echo -e "${GREEN}Summary saved to: $SUMMARY_FILE${NC}"
echo -e "${GREEN}Your checkpoint list is ready and verified!${NC}"
