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

// Maximum string lengths for on-demand reading
#define MAX_NAME_LEN 128
#define MAX_PATH_LEN 256
#define MAX_METADATA_LEN 128

// File entry structure for on-demand reading - minimal size
typedef struct {
    char name[MAX_NAME_LEN];
    char path[MAX_PATH_LEN];
    uint32_t sample_rate;
    uint16_t bit_depth;
    uint16_t channels;
    int folder_index;
    char song[MAX_METADATA_LEN];
    char album[MAX_METADATA_LEN];
    char artist[MAX_METADATA_LEN];
} file_entry_t;

// Folder structure for on-demand reading - minimal size
typedef struct {
    char name[MAX_NAME_LEN];
    int file_count;
    int first_file_index;  // Index of first file in this folder
} folder_t;

// Index file structure - ONLY stores positions, no string data
typedef struct {
    int total_files;
    int folder_count;
    char index_filepath[MAX_PATH_LEN];  // Path to the index.json file for on-demand reading
    long *file_positions;      // Array of file start positions in the JSON file
    long *folder_positions;    // Array of folder start positions in the JSON file
} index_file_t;

/**
 * @brief Parse index.json file - only stores file positions for streaming access
 * 
 * @param filepath Path to index.json file
 * @param index Pointer to store parsed index
 * @return ESP_OK on success
 */
esp_err_t json_parse_index(const char *filepath, index_file_t *index);

/**
 * @brief Read a specific file entry on-demand from the JSON file
 * 
 * @param index Pointer to index
 * @param file_index Index of the file to read (0-based)
 * @param file_entry Pointer to store file entry
 * @return ESP_OK on success
 */
esp_err_t json_get_file_entry(const index_file_t *index, int file_index, file_entry_t *file_entry);

/**
 * @brief Read a specific folder entry on-demand from the JSON file
 * 
 * @param index Pointer to index
 * @param folder_index Index of the folder to read (0-based)
 * @param folder Pointer to store folder
 * @return ESP_OK on success
 */
esp_err_t json_get_folder_entry(const index_file_t *index, int folder_index, folder_t *folder);

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
