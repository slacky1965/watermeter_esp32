#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#include "wm_global.h"
#include "wm_filesys.h"

static const char *TAG = "watermeter_filesystem";

static esp_vfs_spiffs_conf_t conf = {
        .base_path = MOUNT_POINT_SPIFFS,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false };

void init_spiffs() {

    PRINT("Initializing SPIFFS\n");

    spiffs = true;

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NO_MEM) {
            WM_LOGE(TAG, "Objects could not be allocated. (%s:%u)", __FILE__, __LINE__);
        } else if (ret == ESP_ERR_INVALID_STATE) {
            WM_LOGE(TAG, "Already mounted or partition is encrypted. (%s:%u)", __FILE__, __LINE__);
        } else if (ret == ESP_ERR_NOT_FOUND) {
            WM_LOGE(TAG, "Partition for SPIFFS was not found. (%s:%u)", __FILE__, __LINE__);
        } else if (ret == ESP_FAIL) {
            WM_LOGE(TAG, "Mount or format fails. (%s:%u)", __FILE__, __LINE__);
        }
        spiffs = false;
    }
}

void init_sdcard() {

    sdcard = false;

    esp_err_t ret;

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT_SDCARD;

    PRINT("Initializing SD card\n");

    ESP_LOGI(TAG, "Using SPI peripheral");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }


    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            WM_LOGE(TAG,
                    "Failed to mount filesystem."
                    "If you want the card to be formatted, set format_if_mount_failed = true. (%s:%u)",
                    __FILE__, __LINE__);
        } else {
            WM_LOGE(TAG,
                    "Failed to initialize the card (%s)."
                    "Make sure SD card lines have pull-up resistors in place. (%s:%u)",
                    esp_err_to_name(ret), __FILE__, __LINE__);
        }
        return;
    }

    sdcard = true;

    return;
}

size_t get_fs_free_space() {
    size_t full;
    size_t used;

    if (esp_spiffs_info(conf.partition_label, &full, &used) != ESP_OK) {
        return 0;
    }

    return full - used;
}
