#!/bin/bash
# Integration test with real files and minimal mocking

echo "Setting up integration test environment..."

# Create test directory structure
mkdir -p test_data/ESP32_MUSIC/Pop
mkdir -p test_data/ESP32_MUSIC/Rock

# Create dummy PCM files (small but real files)
dd if=/dev/zero of=test_data/ESP32_MUSIC/Pop/song1.pcm bs=1024 count=1 2>/dev/null
dd if=/dev/zero of=test_data/ESP32_MUSIC/Pop/song2.pcm bs=1024 count=2 2>/dev/null
dd if=/dev/zero of=test_data/ESP32_MUSIC/Rock/song3.pcm bs=1024 count=1 2>/dev/null
dd if=/dev/zero of=test_data/ESP32_MUSIC/Rock/song4.pcm bs=1024 count=3 2>/dev/null

# Create real index.json
cat > test_data/ESP32_MUSIC/index.json << 'EOF'
{
  "version": "1.1",
  "totalFiles": 4,
  "allFiles": [
    {
      "name": "song1.pcm",
      "path": "Pop/song1.pcm",
      "song": "Pop Song One",
      "album": "Pop Hits",
      "artist": "Pop Artist A",
      "sampleRate": 44100,
      "bitDepth": 16,
      "channels": 2,
      "folderIndex": 0
    },
    {
      "name": "song2.pcm", 
      "path": "Pop/song2.pcm",
      "song": "Pop Song Two",
      "album": "Pop Hits",
      "artist": "Pop Artist B",
      "sampleRate": 48000,
      "bitDepth": 24,
      "channels": 2,
      "folderIndex": 0
    },
    {
      "name": "song3.pcm",
      "path": "Rock/song3.pcm", 
      "song": "Rock Anthem",
      "album": "Rock Collection",
      "artist": "Rock Artist C",
      "sampleRate": 44100,
      "bitDepth": 16,
      "channels": 2,
      "folderIndex": 1
    },
    {
      "name": "song4.pcm",
      "path": "Rock/song4.pcm",
      "song": "Heavy Metal", 
      "album": "Rock Collection",
      "artist": "Rock Artist D",
      "sampleRate": 96000,
      "bitDepth": 32,
      "channels": 2,
      "folderIndex": 1
    }
  ],
  "folders": [
    {
      "name": "Pop",
      "files": [
        {
          "name": "song1.pcm",
          "path": "Pop/song1.pcm",
          "song": "Pop Song One",
          "album": "Pop Hits", 
          "artist": "Pop Artist A",
          "sampleRate": 44100,
          "bitDepth": 16,
          "channels": 2,
          "folderIndex": 0
        },
        {
          "name": "song2.pcm",
          "path": "Pop/song2.pcm", 
          "song": "Pop Song Two",
          "album": "Pop Hits",
          "artist": "Pop Artist B",
          "sampleRate": 48000,
          "bitDepth": 24,
          "channels": 2,
          "folderIndex": 0
        }
      ]
    },
    {
      "name": "Rock", 
      "files": [
        {
          "name": "song3.pcm",
          "path": "Rock/song3.pcm",
          "song": "Rock Anthem", 
          "album": "Rock Collection",
          "artist": "Rock Artist C",
          "sampleRate": 44100,
          "bitDepth": 16,
          "channels": 2,
          "folderIndex": 1
        },
        {
          "name": "song4.pcm",
          "path": "Rock/song4.pcm",
          "song": "Heavy Metal",
          "album": "Rock Collection", 
          "artist": "Rock Artist D",
          "sampleRate": 96000,
          "bitDepth": 32,
          "channels": 2,
          "folderIndex": 1
        }
      ]
    }
  ]
}
EOF

echo "✅ Integration test environment created"
echo "Real files created:"
ls -la test_data/ESP32_MUSIC/Pop/
ls -la test_data/ESP32_MUSIC/Rock/
echo ""
echo "Real index.json created:"
wc -c test_data/ESP32_MUSIC/index.json

echo ""
echo "🔍 To run integration tests with real files:"
echo "1. Use test_data/ as mount point"
echo "2. Run only PCM file and JSON parser tests with real files"
echo "3. These will test actual file I/O, parsing, and data integrity"

# Test actual JSON parsing with real file
echo ""
echo "Testing real JSON parsing..."
gcc -I./main -o test_real_json main/test_json_parser.c main/json_parser.c -DTEST_MODE -DREAL_FILES
if [ -f test_real_json ]; then
    echo "✅ Real JSON test compiled"
    # Could run it here with modified test to use test_data/ESP32_MUSIC/index.json
else
    echo "❌ Real JSON test compilation failed"
fi

# Cleanup
rm -f test_real_json

echo ""
echo "💡 Recommendation: The current unit tests are valuable for business logic,"
echo "   but adding integration tests with real files would improve confidence."
