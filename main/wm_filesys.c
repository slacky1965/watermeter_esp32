#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <wm_global.h>

#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#include "wm_filesys.h"

static const char *TAG = "watermeter_filesystem";

esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = false
};

void init_spiffs() {

    PRINT("-- Initializing SPIFFS\n");

    spiffs =  true;

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_ERR_NO_MEM) {
			ESP_LOGE(TAG, "Objects could not be allocated");
		} else if (ret == ESP_ERR_INVALID_STATE) {
			ESP_LOGE(TAG, "Already mounted or partition is encrypted");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Partition for SPIFFS was not found");
		} else if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Mount or format fails");
		}
		spiffs = false;
	}
}

void init_sdcard() {

	sdcard = false;

    esp_err_t ret;

    PRINT("-- Initializing SD card\n");

    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = PIN_NUM_MISO;
    slot_config.gpio_mosi = PIN_NUM_MOSI;
    slot_config.gpio_sck  = PIN_NUM_CLK;
    slot_config.gpio_cs   = PIN_NUM_CS;
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t* card;
    const char mount_point[] = MOUNT_POINT_SDCARD;

    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }

    sdcard = true;

	return;
}
