#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_MODE
#include "json_parser.h"

// Mock SD card function
const char* sd_card_get_mount_point() { 
    return "test_data"; 
}

int main() {
    printf("Testing JSON parser with real files...\n");
    
    index_file_t index;
    
    // Test with real file
    esp_err_t ret = json_parse_index("test_data/ESP32_MUSIC/index.json", &index);
    
    if (ret == ESP_OK) {
        printf("✅ Successfully parsed real index.json\n");
        printf("Total files: %d\n", index.total_files);
        printf("Folders: %d\n", index.folder_count);
        
        printf("\nFirst file details:\n");
        printf("  Name: %s\n", index.all_files[0].name);
        printf("  Song: %s\n", index.all_files[0].song);
        printf("  Artist: %s\n", index.all_files[0].artist);
        printf("  Sample Rate: %u Hz\n", index.all_files[0].sample_rate);
        
        printf("\nFirst folder details:\n");
        if (index.folder_count > 0 && index.music_folders) {
            printf("  Name: %s\n", index.music_folders[0].name);
            printf("  File count: %d\n", index.music_folders[0].file_count);
        } else {
            printf("  No folders found or parsed\n");
        }
        
        // Test path generation
        char full_path[256];
        ret = json_get_full_path(index.all_files[0].path, full_path, sizeof(full_path));
        if (ret == ESP_OK) {
            printf("Generated path: %s\n", full_path);
        }
        
        // Clean up
        json_free_index(&index);
        printf("✅ Real file JSON parsing test passed!\n");
        return 0;
    } else {
        printf("❌ Failed to parse real index.json\n");
        return 1;
    }
}
