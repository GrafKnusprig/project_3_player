#include "json_parser.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "sd_card.h"

static const char *TAG = "json_parser";
#define ESP32_MUSIC_DIR "/ESP32_MUSIC"

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

esp_err_t json_parse_index(const char *filepath, index_file_t *index) {
    if (filepath == NULL || index == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Read the file content
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open index file: %s", filepath);
        return ESP_FAIL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        ESP_LOGE(TAG, "Invalid index file size");
        fclose(file);
        return ESP_FAIL;
    }

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
                char *name = extract_string(obj, "name");
                if (name) {
                    strncpy(index->all_files[i].name, name, sizeof(index->all_files[i].name) - 1);
                    index->all_files[i].name[sizeof(index->all_files[i].name) - 1] = '\0';
                    free(name);
                } else {
                    strncpy(index->all_files[i].name, "unknown", sizeof(index->all_files[i].name) - 1);
                }
                
                char *path = extract_string(obj, "path");
                if (path) {
                    strncpy(index->all_files[i].path, path, sizeof(index->all_files[i].path) - 1);
                    index->all_files[i].path[sizeof(index->all_files[i].path) - 1] = '\0';
                    free(path);
                } else {
                    strncpy(index->all_files[i].path, "", sizeof(index->all_files[i].path) - 1);
                }
                
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
            // Find next object
            while (*pos && (*pos != '{')) {
                pos++;
            }
            
            if (!*pos) break;  // End of string
            
            // Extract object
            char *folder_obj = extract_object(pos);
            if (folder_obj) {
                char *name = extract_string(folder_obj, "name");
                if (name) {
                    strncpy(index->music_folders[i].name, name, sizeof(index->music_folders[i].name) - 1);
                    index->music_folders[i].name[sizeof(index->music_folders[i].name) - 1] = '\0';
                    free(name);
                } else {
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
                            char *file_name = extract_string(file_obj, "name");
                            if (file_name) {
                                strncpy(index->music_folders[i].files[j].name, file_name, sizeof(index->music_folders[i].files[j].name) - 1);
                                index->music_folders[i].files[j].name[sizeof(index->music_folders[i].files[j].name) - 1] = '\0';
                                free(file_name);
                            } else {
                                strncpy(index->music_folders[i].files[j].name, "unknown", sizeof(index->music_folders[i].files[j].name) - 1);
                            }
                            
                            char *file_path = extract_string(file_obj, "path");
                            if (file_path) {
                                strncpy(index->music_folders[i].files[j].path, file_path, sizeof(index->music_folders[i].files[j].path) - 1);
                                index->music_folders[i].files[j].path[sizeof(index->music_folders[i].files[j].path) - 1] = '\0';
                                free(file_path);
                            } else {
                                strncpy(index->music_folders[i].files[j].path, "", sizeof(index->music_folders[i].files[j].path) - 1);
                            }
                            
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
                
                // Move to end of object
                while (*pos && (*pos != '}')) {
                    pos++;
                }
                
                if (*pos) pos++;  // Skip closing brace
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
