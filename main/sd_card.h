#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include "esp_err.h"

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

#endif // SD_CARD_H
