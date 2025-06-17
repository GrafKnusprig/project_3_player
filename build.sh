#!/bin/bash
# Build script for Project 3 Player

echo "Project 3 Player Build Script"
echo "============================"

# Check if ESP-IDF is installed
if [ -z "$IDF_PATH" ]; then
    echo "Error: ESP-IDF not found. Please set up the ESP-IDF environment first."
    echo "Follow the instructions at: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/"
    exit 1
fi

# Get the directory of the script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Make sure components directory exists for custom components
if [ ! -d "components" ]; then
    mkdir -p components
fi

# Build the project using ESP-IDF build system
echo "Building project..."
idf.py build
