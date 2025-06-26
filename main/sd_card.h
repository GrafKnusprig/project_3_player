#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>

#ifdef TEST_MODE
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_NO_MEM -3
#define ESP_ERR_INVALID_STATE -4
#define ESP_ERR_NOT_FOUND -5
#else
#include "esp_err.h"
#endif

/**
 * @brief Initialize the SD card
 * 
 * @return ESP_OK on success
 */
esp_err_t sd_card_init(void);

/**
 * @brief Check if SD card is mounted
 * 
 * @return true if mounted
 */
bool sd_card_is_mounted(void);

/**
 * @brief Get the path to the SD card mount point
 * 
 * @return Const string with SD card mount point
 */
const char* sd_card_get_mount_point(void);

/**
 * @brief Write data to a file on the SD card
 * 
 * @param filepath Path to the file
 * @param data Data buffer
 * @param len Length of data
 * @return ESP_OK on success
 */
esp_err_t sd_card_write_file(const char *filepath, const void *data, size_t len);

/**
 * @brief Read data from a file on the SD card
 * 
 * @param filepath Path to the file
 * @param data Buffer to store read data
 * @param max_len Maximum length to read
 * @param bytes_read Pointer to store number of bytes read
 * @return ESP_OK on success
 */
esp_err_t sd_card_read_file(const char *filepath, void *data, size_t max_len, size_t *bytes_read);

/**
 * @brief Check if a file exists on the SD card
 * 
 * @param path Path to check
 * @return true if file exists, false otherwise
 */
bool sd_card_file_exists(const char *path);

/**
 * @brief List all files in a directory and print them for debugging
 * 
 * @param dir_path Directory path to list
 * @return ESP_OK on success
 */
esp_err_t sd_card_list_dir(const char *dir_path);

#endif // SD_CARD_H
