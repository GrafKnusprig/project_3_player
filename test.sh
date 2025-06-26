#!/bin/sh
set -e

echo "Building and running button handler unit tests..."
gcc -o main/test_button_handler main/test_button_handler.c
./main/test_button_handler
