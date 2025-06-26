#!/bin/sh
set -e

echo "Building and running button handler unit tests..."
gcc -o main/test_button_handler main/test_button_handler.c
./main/test_button_handler

echo "Building and running PCM file unit tests..."
gcc -I./main -o main/test_pcm_file main/test_pcm_file.c main/pcm_file.c -DTEST_MODE
./main/test_pcm_file

echo "Building and running JSON parser unit tests..."
gcc -I./main -o main/test_json_parser main/test_json_parser.c main/json_parser.c -DTEST_MODE
./main/test_json_parser

echo "Building and running Audio Player unit tests..."
gcc -I./main -o main/test_audio_player main/test_audio_player.c main/audio_player.c -DTEST_MODE
./main/test_audio_player

echo "All tests passed!"
