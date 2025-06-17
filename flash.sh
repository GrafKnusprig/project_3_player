#!/bin/bash
# Flash script for Project 3 Player

echo "Project 3 Player Flash Script"
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

# Flash the firmware
echo "Flashing firmware to ESP32..."
idf.py flash

echo "Firmware flashed successfully!"
echo ""
echo "To monitor the device, run: idf.py monitor"
