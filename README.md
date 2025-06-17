# Project 3 Player

## Overview
This project implements an ESP32-based music player with a PCM5102 DAC for audio output. It reads PCM audio files from an SD card and provides user control through buttons and visual feedback via a NeoPixel LED.

## Hardware Components
- ESP32-WROOM-32 module
- PCM5102 DAC module
- SD Card reader module
- 3 buttons (next, previous, mode)
- 1 NeoPixel RGB LED

## Features
- Four playback modes:
  1. Play all files in order
  2. Play all files in shuffle mode
  3. Play one folder in order
  4. Play one folder in shuffle mode
- Mode selection with visual feedback via the NeoPixel LED
- Navigation controls:
  - Next track
  - Previous track / Restart current track
  - Next folder / Previous folder
- Persistent state (mode and current track) saved to SD card

## Pin Configuration

### SD Card Module
- MISO: GPIO19
- MOSI: GPIO23
- SCK: GPIO18
- CS: GPIO5
- VCC: 3.3V
- GND: GND

### PCM5102 DAC Module
- DIN (DATA): GPIO22
- BCK (BITCLK): GPIO26
- LRC (LRCLK): GPIO25
- VCC: 3.3V
- GND: GND

### Buttons
- BTN_FWD (Next): GPIO33
- BTN_BCK (Back): GPIO27
- BTN_MENU (Mode): GPIO22

### NeoPixel
- DATA: GPIO21

## Playback Modes
- **Play All Order**: Plays all files from index.json in order (Red LED)
- **Play All Shuffle**: Plays all files from index.json in random order (Green LED)
- **Play Folder Order**: Plays all songs in the current folder in order (Blue LED)
- **Play Folder Shuffle**: Plays all songs in the current folder in random order (Yellow LED)

## Button Controls
- **Next Button (BTN_FWD)**:
  - Short press: Skip to next track
  - Long press (in folder modes): Next folder
- **Back Button (BTN_BCK)**:
  - Short press (within 5 seconds of last press): Previous track
  - Short press (after 5 seconds): Restart current track
  - Long press (in folder modes): Previous folder
- **Mode Button (BTN_MENU)**:
  - Short press: Cycle through modes

## Requirements
- ESP-IDF v4.4 or later
- ESP32 development board
- cJSON library (automatically installed by build script)

## Building and Flashing

To build the project:

```bash
./build.sh
```

To flash the project:

```bash
./flash.sh
```

Alternatively, you can use ESP-IDF commands:

```bash
idf.py build
idf.py -p PORT flash
```

Replace `PORT` with your serial port (e.g., `/dev/ttyUSB0` on Linux or `/dev/cu.SLAB_USBtoUART` on macOS).

## Monitor

To monitor the serial output:

```bash
idf.py -p PORT monitor
```

## SD Card Preparation
- Create a folder named `ESP32_MUSIC` at the root of your SD card
- Place your PCM audio files and folders in this directory
- Create an `index.json` file according to the format in the specifications
