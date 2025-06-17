# Project 3 Player

This is an ESP-IDF based project for an audio player.

## Requirements

- ESP-IDF v4.4 or later
- ESP32 development board

## Building and Flashing

To build the project:

```bash
idf.py build
```

To flash the project:

```bash
idf.py -p PORT flash
```

Replace `PORT` with your serial port (e.g., `/dev/ttyUSB0` on Linux or `/dev/cu.SLAB_USBtoUART` on macOS).

## Monitor

To monitor the serial output:

```bash
idf.py -p PORT monitor
```

You can combine flashing and monitoring:

```bash
idf.py -p PORT flash monitor
```

## Clean

To clean the project:

```bash
idf.py clean
```
