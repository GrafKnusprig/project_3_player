# ESP32 Music Manager Specifications

This document details the technical specifications for the ESP32 Music Manager, including the custom PCM audio format and the index.json structure.

## Custom PCM Audio Format

### File Extension
- `.pcm`

### PCM Header Structure
The custom ESP32 PCM audio header is a 32-byte structure that precedes the raw PCM audio data:

| Offset | Size (bytes) | Description                                           | Format          |
|--------|--------------|-------------------------------------------------------|-----------------|
| 0      | 8            | Magic number ("ESP32PCM")                             | ASCII string    |
| 8      | 4            | Sample rate (e.g., 44100 Hz)                          | uint32, LE      |
| 12     | 2            | Bit depth (e.g., 16 bits)                             | uint16, LE      |
| 14     | 2            | Number of channels (e.g., 2 for stereo)               | uint16, LE      |
| 16     | 4            | Data size in bytes                                    | uint32, LE      |
| 20     | 12           | Reserved (padding, filled with zeros)                 | zeros           |

- **LE**: Little-endian byte order
- Total header size: 32 bytes

### Audio Data Format
Following the header is the raw PCM audio data with these characteristics:

- **Format**: PCM (Pulse Code Modulation)
- **Encoding**: Signed 16-bit little-endian (s16le)
- **Channels**: 2 (Stereo)
- **Sample Rate**: 44100 Hz (CD quality)
- **Byte Order**: Little-endian

### Creation Process
1. Input audio files (various formats) are converted to raw PCM using FFmpeg
2. A custom 32-byte ESP32PCM header is prepended to the raw PCM data
3. The resulting file is saved with a .pcm extension

### Example Header Creation (JavaScript)
```javascript
const createPCMHeader = (sampleRate, bitDepth, channels, dataSize) => {
  const header = Buffer.alloc(32); // 32-byte header
  let offset = 0;
  
  // Magic number "ESP32PCM" (8 bytes)
  header.write('ESP32PCM', offset);
  offset += 8;
  
  // Sample rate (4 bytes, little-endian)
  header.writeUInt32LE(sampleRate, offset);
  offset += 4;
  
  // Bit depth (2 bytes, little-endian)
  header.writeUInt16LE(bitDepth, offset);
  offset += 2;
  
  // Channels (2 bytes, little-endian)
  header.writeUInt16LE(channels, offset);
  offset += 2;
  
  // Data size (4 bytes, little-endian)
  header.writeUInt32LE(dataSize, offset);
  offset += 4;
  
  // Reserved/padding (12 bytes)
  header.fill(0, offset);
  
  return header;
};
```

## Index.json Structure

The index.json file provides a structured inventory of all audio files on the SD card. It enables the ESP32 to navigate and play the audio files efficiently.

### File Placement
- Located at the root of the ESP32_MUSIC folder on the SD card

### Structure
```json
{
  "version": "1.0",
  "totalFiles": 10,
  "allFiles": [
    {
      "name": "Song1.mp3",
      "path": "folder1/song1.pcm"
    },
    {
      "name": "Song2.mp3",
      "path": "song2.pcm"
    },
    ...
  ],
  "musicFolders": [
    {
      "name": "folder1",
      "files": [
        {
          "name": "Song1.mp3",
          "path": "folder1/song1.pcm"
        },
        ...
      ]
    },
    {
      "name": "folder2",
      "files": [
        ...
      ]
    },
    ...
  ]
}
```

### Fields Description

#### Top Level
- **version** (string): Format version of the index file (e.g., "1.0")
- **totalFiles** (number): Total count of audio files in the index
- **allFiles** (array): A flat list of all audio files, regardless of folder
- **musicFolders** (array): List of folders containing audio files

#### File Object
- **name** (string): Original file name (includes the original extension like .mp3)
- **path** (string): Path to the PCM file on the SD card, using forward slashes (/) as separators, relative to the ESP32_MUSIC directory

#### Folder Object
- **name** (string): Folder name
- **files** (array): List of file objects contained in this folder

### Path Handling
- All paths use forward slashes (/) as separators for ESP32 compatibility, regardless of the host OS
- Paths are relative to the ESP32_MUSIC root folder
- Each path includes the folder name and file name with .pcm extension

### Directories Structure on SD Card
```
SD_CARD/
└── ESP32_MUSIC/
    ├── index.json
    ├── song1.pcm
    ├── song2.pcm
    ├── folder1/
    │   ├── song3.pcm
    │   └── song4.pcm
    └── folder2/
        ├── song5.pcm
        └── song6.pcm
```

## Usage with ESP32 Hardware

1. The ESP32 reads the index.json file at boot
2. It uses the file to:
   - Display available music folders and files
   - Locate requested audio files on the SD card
   - Handle proper playback with the correct audio format parameters

3. When a file is selected for playback:
   - The ESP32 opens the corresponding PCM file
   - It reads the 32-byte header to determine playback parameters
   - It streams the raw PCM data to the audio output hardware

---

This specification document defines the custom ESP32 PCM format and the structure of the index.json file used by the ESP32 Music Manager application. It provides all the necessary information for implementing compatible hardware and software systems.
