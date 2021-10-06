#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "wm_config.h"
#include "wm_filesys.h"
#include "wm_utils.h"
#include "wm_wifi.h"
#include "wm_httpd.h"
#include "wm_time.h"
#include "wm_mqtt.h"
#include "wm_pulse.h"
#include "wm_log.h"

//static char *TAG = "watermeter_main";

/* sharing flags */
bool sleepNow;
bool dontSleep;
bool powerLow;
bool apModeNow;
bool staModeNow;
bool staApModeNow;
bool staConfigure;
bool firstStart;
bool subsHotWater;
bool subsColdWater;
bool spiffs;
bool sdcard;

void main_task(void *pvParameter) {

	char buff[32];

	const TickType_t xDelay = 150 / portTICK_PERIOD_MS;

	uint16_t get_count;

	for(;;) {

		/* If counter of hot water was added */
		get_count = get_hot_count();
		if (get_count) {
			reset_sleep_count();
			config_set_hotTime(time(NULL));
			config_set_hotWater(config_get_hotWater() + get_count * config_get_litersPerPulse());
			clear_hot_count();

			sprintf(buff, "%lu %u", config_get_hotTime(), config_get_hotWater());

			PRINT("-- %s <== %s\n", mqtt_get_topic(hot_out), buff);

			if (mqtt_connected()) {
				if (mqtt_publish(mqtt_get_topic(hot_out), buff) == -1) {
					saveConfig();
				}
			} else
				saveConfig();

		}

		/* If counter of cold water was added */
		get_count = get_cold_count();
		if (get_count) {
			reset_sleep_count();
			config_set_coldTime(time(NULL));
			config_set_coldWater(config_get_coldWater() + get_count * config_get_litersPerPulse());
			clear_cold_count();

			sprintf(buff, "%lu %u", config_get_coldTime(), config_get_coldWater());

			PRINT("-- %s <== %s\n", mqtt_get_topic(cold_out), buff);

			if (mqtt_connected()) {
				if (mqtt_publish(mqtt_get_topic(cold_out), buff) == -1) {
					saveConfig();
				}
			} else
				saveConfig();

		}

		/* If reconfigured counter of hot water */
		if (subsHotWater) {
			reset_sleep_count();

			subsHotWater = false;
			sprintf(buff, "%lu %u NEW", config_get_hotTime(), config_get_hotWater());

			PRINT("-- %s <== %s\n", mqtt_get_topic(hot_out), buff);

			if (mqtt_connected())
				mqtt_publish(mqtt_get_topic(hot_out), buff);
		}

		/* If reconfigured counter of cold water */
		if (subsColdWater) {
			reset_sleep_count();

			subsColdWater = false;
			sprintf(buff, "%lu %u NEW", config_get_coldTime(), config_get_coldWater());

			PRINT("-- %s <== %s\n", mqtt_get_topic(cold_out), buff);

			if (mqtt_connected())
				mqtt_publish(mqtt_get_topic(cold_out), buff);
		}
		vTaskDelay(xDelay);
	}
}

void app_main(void) {

	setTimeStart(esp_timer_get_time());

	/* set sharing flags */
	sleepNow = false;
	dontSleep = false;
	powerLow = false;
	apModeNow = true;
	staModeNow = false;
	staApModeNow = false;
	staConfigure = false;
	firstStart = false;
	subsHotWater = false;
	subsColdWater = false;
	spiffs = false;
	sdcard = false;

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_log_set_vprintf((vprintf_like_t)my_vprintf);

    PRINT("-- Startup...\n");
    PRINT("-- Free memory: %d bytes\n", esp_get_free_heap_size());
    PRINT("-- IDF version: %s\n", esp_get_idf_version());

	initDefConfig();

	set_time_zone();

	init_spiffs();

	init_sdcard();

	if (sdcard) {
		set_configFileName(MOUNT_POINT_SDCARD);
	} else if (spiffs) {
		set_configFileName(MOUNT_POINT_SPIFFS);
	}

	if (!readConfig()) {
		firstStart = true;
		saveConfig();
	}

	pulse_init();

	setNullWiFiConfigDefault();

	if (firstStart || config_get_apMode() || config_get_staSsid()[0] == '0') {
		if (!sleepNow) {
			startWiFiAP();
		}
	} else {
		if (!sleepNow) {
			startWiFiSTA();
		}
	}

	webserver_init(HTML_PATH);
	wm_sntp_init();

	mqtt_init();

	light_sleep_init();

	xTaskCreate(&main_task, "main_task", 4096, NULL, 0, NULL);
}

