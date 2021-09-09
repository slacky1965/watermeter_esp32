#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/pcnt.h"
#include "esp_log.h"

#include "wm_global.h"
#include "wm_pulse.h"

static char *TAG = "watermeter_pulse";

static uint16_t water_hot_count;
static uint16_t water_cold_count;


void pulse_hot_task(void *pvParameter) {

	bool btPressed = false;
	bool btState = false;
	uint16_t btbit = 1;

	for(;;) {
		if (!gpio_get_level(HOT_PIN)) {
		    if (btbit == 32768) {
		    	btState = true;
		    } else {
		    	btbit <<= 1;
		    }
		  } else {
		    if (btbit == 1) {
		    	btState = false;
		    }
		    else {
		    	btbit >>= 1;
		    }
		}
		if (btState == true) {

			if (btPressed == false) {
            	water_hot_count++;
				btPressed = true;
		    }
		} else {
			btPressed = false;
		}
	    vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

void pulse_cold_task(void *pvParameter) {

	bool btPressed = false;
	bool btState = false;
	uint16_t btbit = 1;

	for(;;) {
		if (!gpio_get_level(COLD_PIN)) {
		    if (btbit == 32768) {
		    	btState = true;
		    } else {
		    	btbit <<= 1;
		    }
		  } else {
		    if (btbit == 1) {
		    	btState = false;
		    }
		    else {
		    	btbit >>= 1;
		    }
		}
		if (btState == true) {

			if (btPressed == false) {
            	water_cold_count++;
				btPressed = true;
		    }
		} else {
			btPressed = false;
		}
	    vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

void pulse_init() {
	ESP_LOGI(TAG, "Pulse init");

	water_hot_count = 0;
	water_cold_count = 0;

    const int hot_gpio = HOT_PIN;
    const int cold_gpio = COLD_PIN;

    gpio_config_t config_hot_gpio = {
            .pin_bit_mask = BIT64(hot_gpio),
            .mode = GPIO_MODE_INPUT,
			.pull_up_en = GPIO_PULLUP_ENABLE
    };

    gpio_config_t config_cold_gpio = {
            .pin_bit_mask = BIT64(cold_gpio),
            .mode = GPIO_MODE_INPUT,
			.pull_up_en = GPIO_PULLUP_ENABLE
    };

    ESP_ERROR_CHECK(gpio_config(&config_hot_gpio));
    ESP_ERROR_CHECK(gpio_config(&config_cold_gpio));


	xTaskCreate(&pulse_hot_task, "pulse_hot_task", 2048, NULL, 0, NULL);
	xTaskCreate(&pulse_cold_task, "pulse_cold_task", 2048, NULL, 0, NULL);
}

uint16_t get_hot_count() {
	return water_hot_count;
}

uint16_t get_cold_count() {
	return water_cold_count;
}

void clear_hot_count() {
	water_hot_count = 0;
}

void clear_cold_count() {
	water_cold_count = 0;
}
