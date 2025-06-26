#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifndef TEST_MODE
#include "esp_err.h"
#else
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_NO_MEM -3
#endif

// File entry structure
typedef struct {
    char name[256];
    char path[256];
    uint32_t sample_rate;
    uint16_t bit_depth;
    uint16_t channels;
    int folder_index;
    char song[256];
    char album[256];
    char artist[256];
} file_entry_t;

// Folder structure
typedef struct {
    char name[256];
    file_entry_t *files;
    int file_count;
} folder_t;

// Index file structure
typedef struct {
    char version[16];
    int total_files;
    file_entry_t *all_files;
    folder_t *music_folders;
    int folder_count;
} index_file_t;

/**
 * @brief Parse index.json file
 * 
 * @param filepath Path to index.json file
 * @param index Pointer to store parsed index
 * @return ESP_OK on success
 */
esp_err_t json_parse_index(const char *filepath, index_file_t *index);

/**
 * @brief Free memory allocated for index file
 * 
 * @param index Pointer to index
 * @return ESP_OK on success
 */
esp_err_t json_free_index(index_file_t *index);

/**
 * @brief Get full file path including SD card mount point
 * 
 * @param relative_path Relative path from index.json
 * @param full_path Buffer to store full path
 * @param max_len Maximum buffer length
 * @return ESP_OK on success
 */
esp_err_t json_get_full_path(const char *relative_path, char *full_path, size_t max_len);

#endif // JSON_PARSER_H
