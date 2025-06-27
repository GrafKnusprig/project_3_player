#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_MODE
#include "json_parser.h"

// Mock SD card function
const char* sd_card_get_mount_point() { 
    return "."; 
}

// Create a large index.json file for testing
void create_large_index_file(const char* filename, int num_files) {
    printf("Creating file: %s\n", filename);
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Failed to create test file: %s\n", filename);
        return;
    }
    
    fprintf(file, "{\n");
    fprintf(file, "  \"version\": \"1.1\",\n");
    fprintf(file, "  \"totalFiles\": %d,\n", num_files);
    fprintf(file, "  \"allFiles\": [\n");
    
    for (int i = 0; i < num_files; i++) {
        fprintf(file, "    {\n");
        fprintf(file, "      \"name\": \"song%d.pcm\",\n", i + 1);
        fprintf(file, "      \"path\": \"Folder%d/song%d.pcm\",\n", (i % 10) + 1, i + 1);
        fprintf(file, "      \"song\": \"Generated Song %d\",\n", i + 1);
        fprintf(file, "      \"album\": \"Generated Album %d\",\n", (i % 5) + 1);
        fprintf(file, "      \"artist\": \"Generated Artist %d\",\n", (i % 3) + 1);
        fprintf(file, "      \"sampleRate\": %d,\n", (i % 2) ? 44100 : 48000);
        fprintf(file, "      \"bitDepth\": %d,\n", (i % 2) ? 16 : 24);
        fprintf(file, "      \"channels\": 2,\n");
        fprintf(file, "      \"folderIndex\": %d\n", (i % 10));
        fprintf(file, "    }%s\n", (i < num_files - 1) ? "," : "");
    }
    
    fprintf(file, "  ],\n");
    fprintf(file, "  \"folders\": [\n");
    
    for (int i = 0; i < 10; i++) {
        fprintf(file, "    {\n");
        fprintf(file, "      \"name\": \"Folder%d\",\n", i + 1);
        fprintf(file, "      \"files\": []\n");
        fprintf(file, "    }%s\n", (i < 9) ? "," : "");
    }
    
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
}

int main() {
    printf("Testing JSON parser with large files...\n");
    
    // Test different file sizes
    int test_sizes[] = {100, 500, 1000, 2000};
    int num_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    for (int t = 0; t < num_tests; t++) {
        int num_files = test_sizes[t];
        char filename[256];
        snprintf(filename, sizeof(filename), "large_index_%d.json", num_files);
        
        printf("\n🔄 Testing with %d files...\n", num_files);
        
        // Create large test file
        create_large_index_file(filename, num_files);
        
        // Check file size
        FILE *file = fopen(filename, "r");
        if (file) {
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fclose(file);
            printf("Generated file size: %ld bytes\n", size);
        }
        
        // Test parsing
        index_file_t index;
        esp_err_t ret = json_parse_index(filename, &index);
        
        if (ret == ESP_OK) {
            printf("✅ Successfully parsed large index.json with %d files\n", num_files);
            printf("   Total files parsed: %d\n", index.total_files);
            printf("   Expected files: %d\n", num_files);
            
            if (index.total_files == num_files) {
                printf("   ✅ File count matches expected\n");
            } else {
                printf("   ❌ File count mismatch!\n");
            }
            
            // Verify some random entries
            if (index.total_files > 0) {
                file_entry_t file_entry;
                if (json_get_file_entry(&index, 0, &file_entry) == ESP_OK) {
                    printf("   First file: %s (%s)\n", file_entry.name, file_entry.song);
                }
            }
            if (index.total_files > 10) {
                file_entry_t file_entry;
                if (json_get_file_entry(&index, 9, &file_entry) == ESP_OK) {
                    printf("   10th file: %s (%s)\n", file_entry.name, file_entry.song);
                }
            }
            if (index.total_files > 100) {
                file_entry_t file_entry;
                if (json_get_file_entry(&index, 99, &file_entry) == ESP_OK) {
                    printf("   100th file: %s (%s)\n", file_entry.name, file_entry.song);
                }
            }
            
            // Clean up
            json_free_index(&index);
        } else {
            printf("❌ Failed to parse large index.json with %d files\n", num_files);
        }
        
        // Keep test file for inspection
        // remove(filename);
    }
    
    printf("\n🎉 Large file testing completed!\n");
    return 0;
}
