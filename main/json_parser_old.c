#include "json_parser.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

#ifndef TEST_MODE
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "sd_card.h"
#else
// Test mode definitions
#define ESP_LOGI(tag, format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[WARN] " format "\n", ##__VA_ARGS__)
// Mock SD card function - only define if not already defined
extern const char* sd_card_get_mount_point();
#endif

static const char *TAG = "json_parser";
#define ESP32_MUSIC_DIR "/ESP32_MUSIC"  // Use the original directory name

// Forward declaration for streaming parser
static esp_err_t parse_index_streaming(const char *filepath, index_file_t *index);

// Forward declaration for minimal parser
static esp_err_t parse_index_minimal(const char *filepath, index_file_t *index);

// Simple JSON parser for index.json
// This is a simple parser that doesn't handle all JSON cases but works for our specific format

// Helper function to extract string value between quotes
static char* extract_string(const char* json, const char* key) {
    char search_key[256];
    sprintf(search_key, "\"%s\":", key);
    
    char* key_pos = strstr(json, search_key);
    if (!key_pos) {
        return NULL;
    }
    
    // Move pointer to after key
    key_pos += strlen(search_key);
    
    // Skip whitespace
    while (*key_pos == ' ' || *key_pos == '\t' || *key_pos == '\n' || *key_pos == '\r') {
        key_pos++;
    }
    
    // Check if we have a string (starts with ")
    if (*key_pos != '"') {
        return NULL;
    }
    
    // Move past opening quote
    key_pos++;
    
    // Find closing quote
    char* end_pos = strchr(key_pos, '"');
    if (!end_pos) {
        return NULL;
    }
    
    // Calculate length
    size_t len = end_pos - key_pos;
    
    // Allocate and copy string
    char* result = malloc(len + 1);
    if (!result) {
        return NULL;
    }
    
    strncpy(result, key_pos, len);
    result[len] = '\0';
    
    return result;
}

// Helper function to extract integer value
static int extract_int(const char* json, const char* key) {
    char search_key[256];
    sprintf(search_key, "\"%s\":", key);
    
    char* key_pos = strstr(json, search_key);
    if (!key_pos) {
        return 0;
    }
    
    // Move pointer to after key
    key_pos += strlen(search_key);
    
    // Skip whitespace
    while (*key_pos == ' ' || *key_pos == '\t' || *key_pos == '\n' || *key_pos == '\r') {
        key_pos++;
    }
    
    // Convert to integer
    return atoi(key_pos);
}

// Helper function to find array size
static int get_array_size(const char* array_start) {
    int count = 0;
    int brace_level = 0;
    bool in_object = false;
    
    // Skip the opening bracket
    array_start++;
    
    while (*array_start) {
        if (*array_start == '{') {
            if (brace_level == 0 && !in_object) {
                in_object = true;
            }
            brace_level++;
        } else if (*array_start == '}') {
            brace_level--;
            if (brace_level == 0 && in_object) {
                count++;
                in_object = false;
            }
        }
        
        // End of array
        if (*array_start == ']' && brace_level == 0 && !in_object) {
            break;
        }
        
        array_start++;
    }
    
    return count;
}

// Helper to extract a JSON object as a string
static char* extract_object(const char* start) {
    // Skip leading whitespace
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }
    
    if (*start != '{') {
        return NULL;
    }
    
    int brace_count = 1;
    const char* end = start + 1;
    
    while (*end && brace_count > 0) {
        if (*end == '{') {
            brace_count++;
        } else if (*end == '}') {
            brace_count--;
        }
        end++;
    }
    
    if (brace_count != 0) {
        // Unbalanced braces
        return NULL;
    }
    
    size_t len = end - start;
    char* obj = malloc(len + 1);
    if (!obj) {
        return NULL;
    }
    
    strncpy(obj, start, len);
    obj[len] = '\0';
    
    return obj;
}

// Helper to find the beginning of an array
static const char* find_array(const char* json, const char* key) {
    char search_key[256];
    sprintf(search_key, "\"%s\":", key);
    
    char* key_pos = strstr(json, search_key);
    if (!key_pos) {
        return NULL;
    }
    
    // Move pointer to after key
    key_pos += strlen(search_key);
    
    // Skip whitespace
    while (*key_pos == ' ' || *key_pos == '\t' || *key_pos == '\n' || *key_pos == '\r') {
        key_pos++;
    }
    
    // Check if we have an array (starts with [)
    if (*key_pos != '[') {
        return NULL;
    }
    
    return key_pos;
}

// Helper function to parse a file entry with all metadata
static void parse_file_entry(const char *file_obj, file_entry_t *file_entry) {
    // Initialize with defaults
    memset(file_entry, 0, sizeof(file_entry_t));
    file_entry->sample_rate = 44100;  // Default
    file_entry->bit_depth = 16;       // Default
    file_entry->channels = 2;         // Default
    file_entry->folder_index = 0;     // Default
    
    // Parse name
    char *name = extract_string(file_obj, "name");
    if (name) {
        strncpy(file_entry->name, name, sizeof(file_entry->name) - 1);
        file_entry->name[sizeof(file_entry->name) - 1] = '\0';
        free(name);
    } else {
        strncpy(file_entry->name, "unknown", sizeof(file_entry->name) - 1);
    }
    
    // Parse path
    char *path = extract_string(file_obj, "path");
    if (path) {
        strncpy(file_entry->path, path, sizeof(file_entry->path) - 1);
        file_entry->path[sizeof(file_entry->path) - 1] = '\0';
        free(path);
    } else {
        strncpy(file_entry->path, "", sizeof(file_entry->path) - 1);
    }
    
    // Parse audio parameters
    file_entry->sample_rate = extract_int(file_obj, "sampleRate");
    file_entry->bit_depth = extract_int(file_obj, "bitDepth");
    file_entry->channels = extract_int(file_obj, "channels");
    file_entry->folder_index = extract_int(file_obj, "folderIndex");
    
    // Parse song metadata
    char *song = extract_string(file_obj, "song");
    if (song) {
        strncpy(file_entry->song, song, sizeof(file_entry->song) - 1);
        file_entry->song[sizeof(file_entry->song) - 1] = '\0';
        free(song);
    } else {
        strncpy(file_entry->song, "Unknown Song", sizeof(file_entry->song) - 1);
    }
    
    char *album = extract_string(file_obj, "album");
    if (album) {
        strncpy(file_entry->album, album, sizeof(file_entry->album) - 1);
        file_entry->album[sizeof(file_entry->album) - 1] = '\0';
        free(album);
    } else {
        strncpy(file_entry->album, "Unknown Album", sizeof(file_entry->album) - 1);
    }
    
    char *artist = extract_string(file_obj, "artist");
    if (artist) {
        strncpy(file_entry->artist, artist, sizeof(file_entry->artist) - 1);
        file_entry->artist[sizeof(file_entry->artist) - 1] = '\0';
        free(artist);
    } else {
        strncpy(file_entry->artist, "Unknown Artist", sizeof(file_entry->artist) - 1);
    }
}

esp_err_t json_parse_index(const char *filepath, index_file_t *index) {
    if (filepath == NULL || index == NULL) {
        ESP_LOGE(TAG, "Invalid arguments for json_parse_index");
        return ESP_ERR_INVALID_ARG;
    }

#ifndef TEST_MODE
    // Log current memory status
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    ESP_LOGI(TAG, "Memory status: Free heap: %zu bytes, Min free: %zu bytes", free_heap, min_free);
#endif

    ESP_LOGI(TAG, "Attempting to open index file: %s", filepath);
    
    // Try to open the file directly
    FILE *file = fopen(filepath, "rb");
    
    if (!file) {
        ESP_LOGE(TAG, "Failed to open index file: %s (errno: %d)", filepath, errno);
        
        // For debugging, list contents of the directory
        const char* mount_point = sd_card_get_mount_point();
        ESP_LOGI(TAG, "Listing directory contents of the SD card:");
        DIR* dir = opendir(mount_point);
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                ESP_LOGI(TAG, "Found: %s", entry->d_name);
            }
            closedir(dir);
            
            // Check ESP32_MUSIC directory
            char esp32_music_path[256];
            snprintf(esp32_music_path, sizeof(esp32_music_path), "%s/ESP32_MUSIC", mount_point);
            ESP_LOGI(TAG, "Checking directory: %s", esp32_music_path);
            
            dir = opendir(esp32_music_path);
            if (dir) {
                ESP_LOGI(TAG, "ESP32_MUSIC directory found, listing contents:");
                while ((entry = readdir(dir)) != NULL) {
                    ESP_LOGI(TAG, "Found in ESP32_MUSIC: %s", entry->d_name);
                }
                closedir(dir);
            } else {
                ESP_LOGE(TAG, "Failed to open ESP32_MUSIC directory (errno: %d)", errno);
            }
        } else {
            ESP_LOGE(TAG, "Failed to open root directory (errno: %d)", errno);
        }
        
        struct stat st;
        if (stat(filepath, &st) == 0) {
            ESP_LOGI(TAG, "File exists but could not be opened. Size: %lld bytes", st.st_size);
        } else {
            ESP_LOGE(TAG, "File does not exist or cannot be accessed (stat errno: %d)", errno);
        }
        return ESP_FAIL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    ESP_LOGI(TAG, "Index file size: %ld bytes", file_size);

    if (file_size <= 0) {
        ESP_LOGE(TAG, "Invalid index file size");
        fclose(file);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Index file size: %ld bytes", file_size);

    // For large files, use streaming approach to avoid memory issues
    if (file_size > 10000) {  // Reduced threshold to 10KB for better memory usage
        ESP_LOGI(TAG, "Large index file detected (%ld bytes). Using streaming parser.", file_size);
        fclose(file);
        esp_err_t streaming_result = parse_index_streaming(filepath, index);
        if (streaming_result == ESP_OK) {
            return ESP_OK;
        }
        
        ESP_LOGW(TAG, "Streaming parser failed, trying minimal parser as fallback");
        return parse_index_minimal(filepath, index);
    }

    // For smaller files, use the original in-memory approach
    // Allocate memory for the file content
    char *file_content = (char *)malloc(file_size + 1);
    if (!file_content) {
        ESP_LOGE(TAG, "Failed to allocate memory for file content");
        fclose(file);
        return ESP_ERR_NO_MEM;
    }

    // Read the file
    size_t read_size = fread(file_content, 1, file_size, file);
    fclose(file);
    
    if (read_size != file_size) {
        ESP_LOGE(TAG, "Failed to read file content");
        free(file_content);
        return ESP_FAIL;
    }

    // Null-terminate the string
    file_content[file_size] = '\0';

    // Print a small portion of the file content to verify
    ESP_LOGI(TAG, "File content preview (first 100 chars): %.100s", file_content);

    // Parse version
    char *version_str = extract_string(file_content, "version");
    if (version_str) {
        strncpy(index->version, version_str, sizeof(index->version) - 1);
        index->version[sizeof(index->version) - 1] = '\0';
        free(version_str);
    } else {
        strncpy(index->version, "1.0", sizeof(index->version) - 1);
    }
    
    // Parse totalFiles
    index->total_files = extract_int(file_content, "totalFiles");

    // Parse allFiles array
    const char *all_files_array = find_array(file_content, "allFiles");
    if (all_files_array) {
        int files_count = get_array_size(all_files_array);
        index->total_files = files_count;  // Update with actual count
        
        // Allocate memory for files
        index->all_files = (file_entry_t *)malloc(sizeof(file_entry_t) * files_count);
        if (!index->all_files) {
            ESP_LOGE(TAG, "Failed to allocate memory for all_files");
            free(file_content);
            return ESP_ERR_NO_MEM;
        }
        
        // Parse each file entry
        const char *pos = all_files_array + 1;  // Skip opening bracket
        for (int i = 0; i < files_count; i++) {
            // Find next object
            while (*pos && (*pos != '{')) {
                pos++;
            }
            
            if (!*pos) break;  // End of string
            
            // Extract object
            char *obj = extract_object(pos);
            if (obj) {
                parse_file_entry(obj, &index->all_files[i]);
                
                free(obj);
                
                // Move to end of object
                while (*pos && (*pos != '}')) {
                    pos++;
                }
                
                if (*pos) pos++;  // Skip closing brace
            }
        }
    } else {
        index->all_files = NULL;
        index->total_files = 0;
    }

    // Parse musicFolders array
    const char *folders_array = find_array(file_content, "musicFolders");
    if (folders_array) {
        int folders_count = get_array_size(folders_array);
        index->folder_count = folders_count;
        
        // Allocate memory for folders
        index->music_folders = (folder_t *)malloc(sizeof(folder_t) * folders_count);
        if (!index->music_folders) {
            ESP_LOGE(TAG, "Failed to allocate memory for music_folders");
            if (index->all_files) free(index->all_files);
            free(file_content);
            return ESP_ERR_NO_MEM;
        }
        
        // Parse each folder entry
        const char *pos = folders_array + 1;  // Skip opening bracket
        for (int i = 0; i < folders_count; i++) {
            ESP_LOGI(TAG, "Parsing folder %d", i);
            
            // Find next object
            while (*pos && (*pos != '{')) {
                pos++;
            }
            
            if (!*pos) {
                ESP_LOGW(TAG, "No opening brace found for folder %d", i);
                break;  // End of string
            }
            
            // Extract object
            char *folder_obj = extract_object(pos);
            if (folder_obj) {
                ESP_LOGI(TAG, "Folder object %d: %.100s...", i, folder_obj);
                
                char *name = extract_string(folder_obj, "name");
                if (name) {
                    ESP_LOGI(TAG, "Found folder name: %s", name);
                    strncpy(index->music_folders[i].name, name, sizeof(index->music_folders[i].name) - 1);
                    index->music_folders[i].name[sizeof(index->music_folders[i].name) - 1] = '\0';
                    free(name);
                } else {
                    ESP_LOGW(TAG, "No name found for folder %d", i);
                    strncpy(index->music_folders[i].name, "unknown", sizeof(index->music_folders[i].name) - 1);
                }
                
                // Find files array in folder
                const char *files_array = find_array(folder_obj, "files");
                if (files_array) {
                    int files_count = get_array_size(files_array);
                    index->music_folders[i].file_count = files_count;
                    
                    // Allocate memory for folder files
                    index->music_folders[i].files = (file_entry_t *)malloc(sizeof(file_entry_t) * files_count);
                    if (!index->music_folders[i].files) {
                        ESP_LOGE(TAG, "Failed to allocate memory for folder files");
                        // Clean up previously allocated folders
                        for (int j = 0; j < i; j++) {
                            if (index->music_folders[j].files) {
                                free(index->music_folders[j].files);
                            }
                        }
                        if (index->music_folders) free(index->music_folders);
                        if (index->all_files) free(index->all_files);
                        free(folder_obj);
                        free(file_content);
                        return ESP_ERR_NO_MEM;
                    }
                    
                    // Parse each file in folder
                    const char *file_pos = files_array + 1;  // Skip opening bracket
                    for (int j = 0; j < files_count; j++) {
                        // Find next object
                        while (*file_pos && (*file_pos != '{')) {
                            file_pos++;
                        }
                        
                        if (!*file_pos) break;  // End of string
                        
                        // Extract object
                        char *file_obj = extract_object(file_pos);
                        if (file_obj) {
                            parse_file_entry(file_obj, &index->music_folders[i].files[j]);
                            
                            free(file_obj);
                            
                            // Move to end of object
                            while (*file_pos && (*file_pos != '}')) {
                                file_pos++;
                            }
                            
                            if (*file_pos) file_pos++;  // Skip closing brace
                        }
                    }
                } else {
                    index->music_folders[i].files = NULL;
                    index->music_folders[i].file_count = 0;
                }
                
                free(folder_obj);
                
                // Move to end of the extracted object
                int brace_count = 1;
                pos++; // Move past the opening brace we found
                while (*pos && brace_count > 0) {
                    if (*pos == '{') {
                        brace_count++;
                    } else if (*pos == '}') {
                        brace_count--;
                    }
                    pos++;
                }
            } else {
                // If extract_object failed, try to skip this malformed object
                int brace_count = 1;
                pos++;
                while (*pos && brace_count > 0) {
                    if (*pos == '{') {
                        brace_count++;
                    } else if (*pos == '}') {
                        brace_count--;
                    }
                    pos++;
                }
            }
        }
    } else {
        index->music_folders = NULL;
        index->folder_count = 0;
    }

    // Cleanup
    free(file_content);
    ESP_LOGI(TAG, "Index file successfully parsed");
    
    return ESP_OK;
}

// Streaming JSON parser for large files
static esp_err_t parse_index_streaming(const char *filepath, index_file_t *index) {
    ESP_LOGI(TAG, "Using streaming parser for large index file");
    
    FILE *file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open index file for streaming: %s", filepath);
        return ESP_FAIL;
    }
    
    // Initialize index structure
    memset(index, 0, sizeof(index_file_t));
    
    // Read and parse in chunks with smaller buffers to save memory
    char buffer[512];     // Reduced from 1024
    char accumulated[2048] = {0}; // Reduced from 4096 - buffer for accumulating partial JSON objects
    int accumulated_len = 0;
    bool in_all_files = false;
    bool in_folders = false;
    int files_found = 0;
    
    // Pre-allocate arrays with very minimal initial sizes to save memory on ESP32
    int max_files = 2;   // Start extremely small - each file_entry_t is ~1.3KB
    int max_folders = 1; // Start extremely small
    
#ifndef TEST_MODE
    ESP_LOGI(TAG, "Memory before allocation: Free heap: %u bytes", esp_get_free_heap_size());
#endif
    
    index->all_files = (file_entry_t*)malloc(max_files * sizeof(file_entry_t));
    if (!index->all_files) {
        ESP_LOGE(TAG, "Failed to allocate files array (%zu bytes)", max_files * sizeof(file_entry_t));
        fclose(file);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Allocated files array: %zu bytes", max_files * sizeof(file_entry_t));
    
    index->music_folders = (folder_t*)malloc(max_folders * sizeof(folder_t));
    if (!index->music_folders) {
        ESP_LOGE(TAG, "Failed to allocate folders array (%zu bytes)", max_folders * sizeof(folder_t));
        fclose(file);
        free(index->all_files);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Allocated folders array: %zu bytes", max_folders * sizeof(folder_t));
    
    ESP_LOGI(TAG, "Allocated initial arrays: %d files (%zu bytes), %d folders (%zu bytes)", 
             max_files, max_files * sizeof(file_entry_t), max_folders, max_folders * sizeof(folder_t));
    
    while (fgets(buffer, sizeof(buffer), file)) {
        // Add to accumulated buffer
        int buffer_len = strlen(buffer);
        if (accumulated_len + buffer_len >= sizeof(accumulated) - 1) {
            // Buffer overflow protection - reset
            accumulated_len = 0;
            accumulated[0] = '\0';
        }
        
        strcat(accumulated, buffer);
        accumulated_len += buffer_len;
        
        // Look for complete JSON objects
        if (strstr(accumulated, "\"totalFiles\":")) {
            index->total_files = extract_int(accumulated, "totalFiles");
            ESP_LOGI(TAG, "Found totalFiles: %d", index->total_files);
            
            // Reallocate arrays based on actual count, but grow more conservatively
            if (index->total_files > max_files) {
                // Don't allocate the full amount immediately - grow in smaller chunks
                int new_max = (index->total_files > 100) ? (max_files + 50) : index->total_files;
                ESP_LOGI(TAG, "Need to grow files array from %d to %d entries (%zu bytes)", 
                         max_files, new_max, new_max * sizeof(file_entry_t));
                         
                // Check if we have enough memory
                size_t needed_memory = new_max * sizeof(file_entry_t);
#ifndef TEST_MODE
                if (esp_get_free_heap_size() < needed_memory + 10000) { // Keep 10KB buffer
                    ESP_LOGE(TAG, "Insufficient memory for reallocation. Need %zu bytes, have %u bytes", 
                             needed_memory, esp_get_free_heap_size());
                    fclose(file);
                    free(index->all_files);
                    free(index->music_folders);
                    return ESP_ERR_NO_MEM;
                }
#endif
                
                file_entry_t *new_files = (file_entry_t*)realloc(index->all_files, needed_memory);
                if (new_files) {
                    index->all_files = new_files;
                    max_files = new_max;
                    ESP_LOGI(TAG, "Successfully reallocated files array to %d entries", max_files);
                } else {
                    ESP_LOGE(TAG, "Failed to reallocate files array");
                    fclose(file);
                    free(index->all_files);
                    free(index->music_folders);
                    return ESP_ERR_NO_MEM;
                }
            }
        }
        
        // Check for allFiles section
        if (strstr(accumulated, "\"allFiles\":")) {
            in_all_files = true;
            ESP_LOGI(TAG, "Entering allFiles section");
        }
        
        // Check for folders section  
        if (strstr(accumulated, "\"folders\":")) {
            in_folders = true;
            in_all_files = false;
            ESP_LOGI(TAG, "Entering folders section");
        }
        
        // Look for complete file objects in allFiles
        if (in_all_files && strstr(accumulated, "\"name\":") && strstr(accumulated, "\"path\":") && 
            strstr(accumulated, "\"song\":") && strstr(accumulated, "}")) {
            
            // Check if we need to grow the array
            if (files_found >= max_files) {
                int new_max = max_files + 20; // Grow in chunks of 20
                size_t needed_memory = new_max * sizeof(file_entry_t);
                
                ESP_LOGI(TAG, "Growing files array from %d to %d entries (%zu bytes)", 
                         max_files, new_max, needed_memory);
                         
#ifndef TEST_MODE
                if (esp_get_free_heap_size() < needed_memory + 10000) { // Keep 10KB buffer
                    ESP_LOGE(TAG, "Out of memory during parsing. Have %u bytes, need %zu", 
                             esp_get_free_heap_size(), needed_memory);
                    fclose(file);
                    free(index->all_files);
                    free(index->music_folders);
                    return ESP_ERR_NO_MEM;
                }
#endif
                
                file_entry_t *new_files = (file_entry_t*)realloc(index->all_files, needed_memory);
                if (!new_files) {
                    ESP_LOGE(TAG, "Failed to grow files array");
                    fclose(file);
                    free(index->all_files);
                    free(index->music_folders);
                    return ESP_ERR_NO_MEM;
                }
                
                index->all_files = new_files;
                max_files = new_max;
            }
            
            // Parse this file entry
            file_entry_t *entry = &index->all_files[files_found];
            memset(entry, 0, sizeof(file_entry_t));
                
                char *name = extract_string(accumulated, "name");
                char *path = extract_string(accumulated, "path");
                char *song = extract_string(accumulated, "song");
                char *album = extract_string(accumulated, "album");
                char *artist = extract_string(accumulated, "artist");
                
                if (name && path && song) {
                    strncpy(entry->name, name, sizeof(entry->name) - 1);
                    strncpy(entry->path, path, sizeof(entry->path) - 1);
                    strncpy(entry->song, song, sizeof(entry->song) - 1);
                    if (album) strncpy(entry->album, album, sizeof(entry->album) - 1);
                    if (artist) strncpy(entry->artist, artist, sizeof(entry->artist) - 1);
                    
                    entry->sample_rate = extract_int(accumulated, "sampleRate");
                    entry->bit_depth = extract_int(accumulated, "bitDepth");
                    entry->channels = extract_int(accumulated, "channels");
                    entry->folder_index = extract_int(accumulated, "folderIndex");
                    
                    files_found++;
                    ESP_LOGI(TAG, "Parsed file %d: %s", files_found, entry->name);
                }
                
                if (name) free(name);
                if (path) free(path);
                if (song) free(song);
                if (album) free(album);
                if (artist) free(artist);
            
            // Clear accumulated buffer for next object
            accumulated_len = 0;
            accumulated[0] = '\0';
        }
        
        // Clear buffer if we see section endings
        if (strstr(accumulated, "],") && (in_all_files || in_folders)) {
            accumulated_len = 0;
            accumulated[0] = '\0';
            if (in_all_files) {
                in_all_files = false;
                ESP_LOGI(TAG, "Finished allFiles section, found %d files", files_found);
            }
        }
    }
    
    fclose(file);
    
    // Update final counts
    index->total_files = files_found;
    index->folder_count = 0; // For now, skip folders in streaming mode
    
    ESP_LOGI(TAG, "Streaming parser completed: %d files parsed", files_found);
    
    return (files_found > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t json_free_index(index_file_t *index) {
    if (index == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Free all files
    if (index->all_files != NULL) {
        free(index->all_files);
        index->all_files = NULL;
    }

    // Free folders and their files
    if (index->music_folders != NULL) {
        for (int i = 0; i < index->folder_count; i++) {
            if (index->music_folders[i].files != NULL) {
                free(index->music_folders[i].files);
                index->music_folders[i].files = NULL;
            }
        }
        free(index->music_folders);
        index->music_folders = NULL;
    }

    return ESP_OK;
}

esp_err_t json_get_full_path(const char *relative_path, char *full_path, size_t max_len) {
    if (relative_path == NULL || full_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *mount_point = sd_card_get_mount_point();
    snprintf(full_path, max_len, "%s%s/%s", mount_point, ESP32_MUSIC_DIR, relative_path);
    
    return ESP_OK;
}

// Minimal parser for extreme memory constraints - only loads first few files
static esp_err_t parse_index_minimal(const char *filepath, index_file_t *index) {
    ESP_LOGI(TAG, "Using minimal parser for extreme memory constraints");
    
    FILE *file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open index file for minimal parsing: %s", filepath);
        return ESP_FAIL;
    }
    
    // Initialize index structure
    memset(index, 0, sizeof(index_file_t));
    
    // Allocate space for only a few files
    int max_files = 1;
    index->all_files = (file_entry_t*)malloc(max_files * sizeof(file_entry_t));
    if (!index->all_files) {
        ESP_LOGE(TAG, "Cannot allocate even minimal memory");
        fclose(file);
        return ESP_ERR_NO_MEM;
    }
    
    // Parse just the first file we can find
    char line[256];
    bool found_file = false;
    int files_found = 0;
    
    while (fgets(line, sizeof(line), file) && files_found < max_files) {
        if (strstr(line, "\"name\":") && strstr(line, ".pcm")) {
            // Found a file entry, parse just the essential info
            file_entry_t *entry = &index->all_files[files_found];
            memset(entry, 0, sizeof(file_entry_t));
            
            // Try to extract basic info
            char *name_start = strstr(line, "\"name\":");
            if (name_start) {
                char *quote1 = strchr(name_start + 7, '"');
                char *quote2 = quote1 ? strchr(quote1 + 1, '"') : NULL;
                if (quote1 && quote2) {
                    int len = quote2 - quote1 - 1;
                    if (len > 0 && len < sizeof(entry->name) - 1) {
                        strncpy(entry->name, quote1 + 1, len);
                        entry->name[len] = '\0';
                        
                        // Set default values
                        strncpy(entry->path, entry->name, sizeof(entry->path) - 1);
                        strncpy(entry->song, "Unknown Song", sizeof(entry->song) - 1);
                        strncpy(entry->album, "Unknown Album", sizeof(entry->album) - 1);
                        strncpy(entry->artist, "Unknown Artist", sizeof(entry->artist) - 1);
                        entry->sample_rate = 44100;
                        entry->bit_depth = 16;
                        entry->channels = 2;
                        entry->folder_index = 0;
                        
                        files_found++;
                        found_file = true;
                        ESP_LOGI(TAG, "Minimal parser found file: %s", entry->name);
                    }
                }
            }
        }
    }
    
    fclose(file);
    
    index->total_files = files_found;
    index->folder_count = 0;
    index->music_folders = NULL;
    
    ESP_LOGI(TAG, "Minimal parser completed: %d files", files_found);
    
    return found_file ? ESP_OK : ESP_FAIL;
}
