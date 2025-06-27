#include "json_parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

#ifndef TEST_MODE
#include "esp_log.h"
#include "sd_card.h"
#else
#define ESP_LOGI(tag, format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[WARN] " format "\n", ##__VA_ARGS__)
extern const char* sd_card_get_mount_point(void); // Defined in test file
#endif

static const char *TAG = "json_parser";
#define ESP32_MUSIC_DIR "/ESP32_MUSIC"
#define BUFFER_SIZE 512  // Small buffer for streaming

/**
 * Helper function to read a small chunk from file at position
 */
static esp_err_t read_chunk_at_position(FILE *file, long position, char *buffer, size_t buffer_size) {
    if (fseek(file, position, SEEK_SET) != 0) {
        return ESP_FAIL;
    }
    
    size_t read_bytes = fread(buffer, 1, buffer_size - 1, file);
    buffer[read_bytes] = '\0';
    
    return ESP_OK;
}

/**
 * Find the next occurrence of a pattern in the file starting from position
 */
static long find_pattern_in_file(FILE *file, long start_pos, const char *pattern, long max_search) {
    size_t pattern_len = strlen(pattern);
    if (pattern_len == 0) return -1;
    
    char *search_buffer = malloc(BUFFER_SIZE + pattern_len);
    if (!search_buffer) return -1;
    
    long current_pos = start_pos;
    long searched = 0;
    size_t buffer_overlap = 0;
    
    while (searched < max_search) {
        // Seek to current position
        if (fseek(file, current_pos, SEEK_SET) != 0) {
            free(search_buffer);
            return -1;
        }
        
        // Read new data after any overlap from previous buffer
        size_t bytes_to_read = BUFFER_SIZE;
        size_t bytes_read = fread(search_buffer + buffer_overlap, 1, bytes_to_read, file);
        
        if (bytes_read == 0) {
            // End of file
            break;
        }
        
        // Null terminate for string operations
        size_t total_bytes = buffer_overlap + bytes_read;
        search_buffer[total_bytes] = '\0';
        
        // Search for pattern in current buffer
        char *found = strstr(search_buffer, pattern);
        if (found) {
            long found_pos = current_pos - buffer_overlap + (found - search_buffer);
            free(search_buffer);
            return found_pos;
        }
        
        // If we read less than requested, we're at end of file
        if (bytes_read < bytes_to_read) {
            break;
        }
        
        // Prepare overlap for next iteration
        // Copy last (pattern_len - 1) bytes to beginning of buffer
        if (total_bytes >= pattern_len) {
            memmove(search_buffer, search_buffer + total_bytes - (pattern_len - 1), pattern_len - 1);
            buffer_overlap = pattern_len - 1;
        } else {
            buffer_overlap = total_bytes;
        }
        
        // Move forward by the amount we actually processed
        current_pos += bytes_read;
        searched += bytes_read;
    }
    
    free(search_buffer);
    return -1; // Not found
}

/**
 * Extract a JSON string value from a small buffer
 */
static esp_err_t extract_string_from_buffer(const char *buffer, const char *key, char *result, size_t max_len) {
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char *key_pos = strstr(buffer, search_pattern);
    if (!key_pos) {
        return ESP_FAIL;
    }
    
    // Move to value
    key_pos += strlen(search_pattern);
    while (*key_pos == ' ' || *key_pos == '\t' || *key_pos == '\n') key_pos++;
    
    if (*key_pos != '"') {
        return ESP_FAIL;
    }
    key_pos++; // Skip opening quote
    
    // Find closing quote
    char *end_quote = strchr(key_pos, '"');
    if (!end_quote) {
        return ESP_FAIL;
    }
    
    size_t len = end_quote - key_pos;
    if (len >= max_len) {
        len = max_len - 1;
    }
    
    strncpy(result, key_pos, len);
    result[len] = '\0';
    
    return ESP_OK;
}

/**
 * Extract a JSON integer value from a small buffer
 */
static int extract_int_from_buffer(const char *buffer, const char *key) {
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char *key_pos = strstr(buffer, search_pattern);
    if (!key_pos) {
        return 0;
    }
    
    // Move to value
    key_pos += strlen(search_pattern);
    while (*key_pos == ' ' || *key_pos == '\t' || *key_pos == '\n') key_pos++;
    
    return atoi(key_pos);
}

/**
 * Find the position of the nth JSON object in an array
 */
static long find_nth_object_in_array(FILE *file, long array_start, int n) {
    long current_pos = array_start;
    int found_count = 0;
    
    // Skip to after opening bracket
    char buffer[BUFFER_SIZE];
    if (read_chunk_at_position(file, current_pos, buffer, BUFFER_SIZE) != ESP_OK) {
        return -1;
    }
    
    char *bracket_pos = strchr(buffer, '[');
    if (!bracket_pos) {
        return -1;
    }
    
    current_pos += (bracket_pos - buffer) + 1;
    
    // Find the nth object - increase search limit for large files
    while (found_count <= n) {
        long obj_pos = find_pattern_in_file(file, current_pos, "{", 10000000); // 10MB search limit
        if (obj_pos == -1) {
            return -1;
        }
        
        if (found_count == n) {
            return obj_pos;
        }
        
        found_count++;
        current_pos = obj_pos + 1;
    }
    
    return -1;
}

/**
 * Count objects in a JSON array more precisely
 */
static int count_objects_in_array(FILE *file, long array_start) {
    char buffer[BUFFER_SIZE];
    if (read_chunk_at_position(file, array_start, buffer, BUFFER_SIZE) != ESP_OK) {
        return 0;
    }
    
    char *bracket_pos = strchr(buffer, '[');
    if (!bracket_pos) {
        return 0;
    }
    
    // Start after the opening bracket
    long current_pos = array_start + (bracket_pos - buffer) + 1;
    int count = 0;
    
    // Simply count all '{' characters until we hit the closing ']'
    // This is simpler and more reliable than tracking brace levels
    while (true) {
        long object_pos = find_pattern_in_file(file, current_pos, "{", 10000000);
        if (object_pos == -1) {
            // No more objects found
            ESP_LOGI(TAG, "No more '{' found after position %ld, ending count at %d", current_pos, count);
            break;
        }
        
        // Check if we've hit the end of the array (closing ']' before this object)
        // Search a much larger distance ahead for the closing bracket
        long search_distance = 50000; // Large enough to find the end of most arrays
        long closing_bracket = find_pattern_in_file(file, current_pos, "]", search_distance);
        if (closing_bracket != -1 && closing_bracket < object_pos) {
            // Found closing bracket before next object, we're done
            ESP_LOGI(TAG, "Found closing bracket at %ld before next object at %ld", closing_bracket, object_pos);
            break;
        }
        
        count++;
        current_pos = object_pos + 1;
        
        // Safety check for very large arrays
        if (count > 10000) {
            ESP_LOGW(TAG, "Array has more than 10000 objects, limiting to 10000");
            break;
        }
    }
    
    ESP_LOGI(TAG, "Counted %d objects in array", count);
    return count;
}

esp_err_t json_parse_index(const char *filepath, index_file_t *index) {
    if (filepath == NULL || index == NULL) {
        ESP_LOGE(TAG, "Invalid arguments for json_parse_index");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting minimal memory streaming parse of: %s", filepath);
    
    // Initialize index structure
    memset(index, 0, sizeof(index_file_t));
    strncpy(index->index_filepath, filepath, sizeof(index->index_filepath) - 1);
    
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open index file: %s", filepath);
        return ESP_FAIL;
    }

    // Get file size for bounds checking
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    ESP_LOGI(TAG, "Index file size: %ld bytes", file_size);
    
    // Find allFiles array position
    long all_files_pos = find_pattern_in_file(file, 0, "\"allFiles\":", file_size);
    if (all_files_pos == -1) {
        ESP_LOGE(TAG, "Could not find allFiles array");
        fclose(file);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Found allFiles array at position %ld", all_files_pos);
    
    // Count files
    index->total_files = count_objects_in_array(file, all_files_pos);
    if (index->total_files == 0) {
        ESP_LOGE(TAG, "No files found in allFiles array");
        fclose(file);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Found %d files, allocating position array (%lu bytes)", 
             index->total_files, (unsigned long)(index->total_files * sizeof(long)));
    
    // Allocate ONLY the position arrays (minimal memory)
    index->file_positions = malloc(index->total_files * sizeof(long));
    if (!index->file_positions) {
        ESP_LOGE(TAG, "Failed to allocate file positions array");
        fclose(file);
        return ESP_ERR_NO_MEM;
    }
    
    // Store positions of each file object
    for (int i = 0; i < index->total_files; i++) {
        long file_pos = find_nth_object_in_array(file, all_files_pos, i);
        if (file_pos == -1) {
            ESP_LOGE(TAG, "Could not find file %d", i);
            free(index->file_positions);
            fclose(file);
            return ESP_FAIL;
        }
        index->file_positions[i] = file_pos;
    }
    
    // Find musicFolders array position
    long folders_pos = find_pattern_in_file(file, 0, "\"musicFolders\":", file_size);
    if (folders_pos != -1) {
        index->folder_count = count_objects_in_array(file, folders_pos);
        
        if (index->folder_count > 0) {
            ESP_LOGI(TAG, "Found %d folders, allocating position array (%lu bytes)", 
                     index->folder_count, (unsigned long)(index->folder_count * sizeof(long)));
            
            index->folder_positions = malloc(index->folder_count * sizeof(long));
            if (!index->folder_positions) {
                ESP_LOGE(TAG, "Failed to allocate folder positions array");
                free(index->file_positions);
                fclose(file);
                return ESP_ERR_NO_MEM;
            }
            
            // Store positions of each folder object
            for (int i = 0; i < index->folder_count; i++) {
                long folder_pos = find_nth_object_in_array(file, folders_pos, i);
                if (folder_pos == -1) {
                    ESP_LOGE(TAG, "Could not find folder %d", i);
                    free(index->file_positions);
                    free(index->folder_positions);
                    fclose(file);
                    return ESP_FAIL;
                }
                index->folder_positions[i] = folder_pos;
            }
        }
    } else {
        ESP_LOGI(TAG, "No musicFolders array found");
        index->folder_count = 0;
        index->folder_positions = NULL;
    }
    
    fclose(file);
    
    ESP_LOGI(TAG, "Successfully parsed index with %d files and %d folders (minimal memory mode)", 
             index->total_files, index->folder_count);
    
    return ESP_OK;
}

esp_err_t json_get_file_entry(const index_file_t *index, int file_index, file_entry_t *file_entry) {
    if (!index || !file_entry || file_index < 0 || file_index >= index->total_files) {
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE *file = fopen(index->index_filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open index file for reading file entry");
        return ESP_FAIL;
    }
    
    // Read a chunk around the file position
    char buffer[BUFFER_SIZE * 2]; // Larger buffer for file objects
    long file_pos = index->file_positions[file_index];
    
    if (read_chunk_at_position(file, file_pos, buffer, sizeof(buffer)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read file entry at position %ld", file_pos);
        fclose(file);
        return ESP_FAIL;
    }
    
    // Initialize with defaults
    memset(file_entry, 0, sizeof(file_entry_t));
    file_entry->folder_index = -1;
    
    // Extract values from buffer
    extract_string_from_buffer(buffer, "name", file_entry->name, sizeof(file_entry->name));
    extract_string_from_buffer(buffer, "path", file_entry->path, sizeof(file_entry->path));
    extract_string_from_buffer(buffer, "song", file_entry->song, sizeof(file_entry->song));
    extract_string_from_buffer(buffer, "album", file_entry->album, sizeof(file_entry->album));
    extract_string_from_buffer(buffer, "artist", file_entry->artist, sizeof(file_entry->artist));
    
    file_entry->sample_rate = extract_int_from_buffer(buffer, "sampleRate");
    file_entry->bit_depth = extract_int_from_buffer(buffer, "bitDepth");
    file_entry->channels = extract_int_from_buffer(buffer, "channels");
    file_entry->folder_index = extract_int_from_buffer(buffer, "folderIndex");
    
    fclose(file);
    return ESP_OK;
}

esp_err_t json_get_folder_entry(const index_file_t *index, int folder_index, folder_t *folder) {
    if (!index || !folder || folder_index < 0 || folder_index >= index->folder_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE *file = fopen(index->index_filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open index file for reading folder entry");
        return ESP_FAIL;
    }
    
    // Read a chunk around the folder position
    char buffer[BUFFER_SIZE];
    long folder_pos = index->folder_positions[folder_index];
    
    if (read_chunk_at_position(file, folder_pos, buffer, sizeof(buffer)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read folder entry at position %ld", folder_pos);
        fclose(file);
        return ESP_FAIL;
    }
    
    // Initialize with defaults
    memset(folder, 0, sizeof(folder_t));
    
    // Extract values from buffer
    extract_string_from_buffer(buffer, "name", folder->name, sizeof(folder->name));
    folder->file_count = extract_int_from_buffer(buffer, "fileCount");
    folder->first_file_index = extract_int_from_buffer(buffer, "firstFileIndex");
    
    fclose(file);
    return ESP_OK;
}

esp_err_t json_free_index(index_file_t *index) {
    if (!index) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (index->file_positions) {
        free(index->file_positions);
        index->file_positions = NULL;
    }
    
    if (index->folder_positions) {
        free(index->folder_positions);
        index->folder_positions = NULL;
    }
    
    memset(index, 0, sizeof(index_file_t));
    return ESP_OK;
}

esp_err_t json_get_full_path(const char *relative_path, char *full_path, size_t max_len) {
    if (!relative_path || !full_path) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *mount_point = sd_card_get_mount_point();
    if (!mount_point) {
        return ESP_FAIL;
    }
    
    // Handle paths that start with "ESP32_MUSIC/" by removing it
    const char *clean_path = relative_path;
    if (strncmp(relative_path, "ESP32_MUSIC/", 12) == 0) {
        clean_path = relative_path + 12;
    }
    
    snprintf(full_path, max_len, "%s%s/%s", mount_point, ESP32_MUSIC_DIR, clean_path);
    return ESP_OK;
}
