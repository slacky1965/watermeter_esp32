#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "hal/adc_types.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_sleep.h"
#include "esp_wifi.h"

#include <wm_global.h>

#include "wm_config.h"
#include "wm_wifi.h"
#include "wm_mqtt.h"

#define DEFAULT_VREF    			1100
#define SLEEP_COUNT_MAX				5


//static const char *TAG = "watermeter_util";
static int sleep_count = 0;

void reset_sleep_count() {
	sleep_count = 0;
}

static void light_sleep_task(void *pvParameter) {

	const TickType_t xDelay = 2500 / portTICK_PERIOD_MS;

	int power_level;

    sleep_count = 0;

	for (;;) {
		power_level = gpio_get_level(EXT_PW_PIN);

		if (power_level == 0) {
			powerLow = true;
			if (sleep_count < SLEEP_COUNT_MAX) {
				if (sleep_count == 0) {
					printf("-- Power low! Preparing for sleep light mode.\n");
				}
				sleep_count++;
			} else {
				sleepNow = true;
				if (!dontSleep) {
					printf("-- Light sleep set now!\n");
					mqtt_stop();
					esp_wifi_stop();
				    vTaskDelay(1000 / portTICK_PERIOD_MS);
			        esp_sleep_enable_gpio_wakeup();
			        esp_light_sleep_start();
			        sleep_count = 1;
			        printf("-- Awoke from a light sleep\n");
				}
			}
		} else {
			powerLow = false;
			if (sleep_count != 0) {
				printf("-- External power high.\n");
				sleepNow = false;
				sleep_count = 0;
				esp_wifi_start();
			}
		}

		vTaskDelay(xDelay);
	}
}

void light_sleep_init() {

    const int sleep_gpio_num1 = HOT_PIN;
    const int sleep_gpio_num2 = COLD_PIN;
    const int extpower_gpio_num = EXT_PW_PIN;

    gpio_config_t config_extpower = {
            .pin_bit_mask = BIT64(extpower_gpio_num),
            .mode = GPIO_MODE_INPUT,
			.pull_down_en = GPIO_PULLDOWN_ENABLE
    };


    ESP_ERROR_CHECK(gpio_config(&config_extpower));
    gpio_wakeup_enable(sleep_gpio_num1, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(sleep_gpio_num2, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(extpower_gpio_num, GPIO_INTR_HIGH_LEVEL);

	if (!SLEEP_MODE_ON) return;

	xTaskCreate(&light_sleep_task, "light_sleep_task", 2048, NULL, 0, NULL);
}

char *getVoltage() {

	const int COUNT = 20;

	static char buf[32];
	static esp_adc_cal_characteristics_t adc_chars;
	const adc1_channel_t VBAT = VBUS;
	const adc_atten_t atten = ADC_ATTEN_DB_11;
	const adc_unit_t unit = ADC_UNIT_1;

	uint32_t adc_reading = 0;

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(VBAT, atten);

    esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);

    for (int i = 0; i < COUNT; i++) {
        adc_reading += adc1_get_raw(VBAT);
    }

    adc_reading /= COUNT;


    uint32_t voltage = 2 * esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);

    sprintf(buf, "Battery: %d,%dV", voltage/1000, (voltage%1000)/10);

	return buf;
}



