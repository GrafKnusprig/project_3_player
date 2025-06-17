#include "sd_card.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"

// SD Card pins
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23
#define SD_SCK_PIN  18
#define SD_CS_PIN   5

#define MOUNT_POINT "/sdcard"
#define MAX_FILES 5

static const char *TAG = "sd_card";
static bool is_mounted = false;
static sdmmc_card_t *card;
static sdmmc_host_t host = SDSPI_HOST_DEFAULT();

esp_err_t sd_card_init(void) {
    ESP_LOGI(TAG, "Initializing SD card");

    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = MAX_FILES,
        .allocation_unit_size = 16 * 1024
    };

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI_PIN,
        .miso_io_num = SD_MISO_PIN,
        .sclk_io_num = SD_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus. Error: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card. Error: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    is_mounted = true;
    ESP_LOGI(TAG, "SD card initialized successfully");
    return ESP_OK;
}

bool sd_card_is_mounted(void) {
    return is_mounted;
}

const char* sd_card_get_mount_point(void) {
    ESP_LOGI(TAG, "Returning mount point: %s", MOUNT_POINT);
    return MOUNT_POINT;
}

esp_err_t sd_card_write_file(const char *filepath, const void *data, size_t len) {
    if (!is_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", MOUNT_POINT, filepath);

    ESP_LOGI(TAG, "Writing file: %s", full_path);
    FILE *file = fopen(full_path, "wb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, file);
    fclose(file);

    if (written != len) {
        ESP_LOGE(TAG, "Failed to write file. Wrote %d of %d bytes", written, len);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t sd_card_read_file(const char *filepath, void *data, size_t max_len, size_t *bytes_read) {
    if (!is_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", MOUNT_POINT, filepath);

    ESP_LOGI(TAG, "Reading file: %s", full_path);
    FILE *file = fopen(full_path, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    *bytes_read = fread(data, 1, max_len, file);
    fclose(file);

    return ESP_OK;
}

bool sd_card_file_exists(const char *path) {
    if (!is_mounted) {
        return false;
    }
    
    char full_path[256];
    
    // If path already starts with the mount point, use it directly
    if (strncmp(path, MOUNT_POINT, strlen(MOUNT_POINT)) == 0) {
        strncpy(full_path, path, sizeof(full_path) - 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s%s", MOUNT_POINT, path);
    }
    
    struct stat st;
    return (stat(full_path, &st) == 0);
}

esp_err_t sd_card_list_dir(const char *dir_path) {
    if (!is_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char full_path[256];
    
    // If path already starts with the mount point, use it directly
    if (strncmp(dir_path, MOUNT_POINT, strlen(MOUNT_POINT)) == 0) {
        strncpy(full_path, dir_path, sizeof(full_path) - 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s%s", MOUNT_POINT, dir_path);
    }
    
    ESP_LOGI(TAG, "Listing directory: %s", full_path);
    
    DIR *dir = opendir(full_path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory (errno: %d)", errno);
        return ESP_FAIL;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "  %s", entry->d_name);
    }
    
    closedir(dir);
    return ESP_OK;
}

// sd_card_resolve_path has been removed as we're using long filenames with FATFS
