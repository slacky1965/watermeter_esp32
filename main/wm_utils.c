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
#include "wm_time.h"
#include "wm_mqtt.h"
#include "wm_log.h"

#define DEFAULT_VREF    			1100
#define SLEEP_COUNT_MAX				5

static const char *TAG = "watermeter_util";
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
                    PRINT("Power low! Preparing for sleep light mode.\n");
                }
                sleep_count++;
            } else {
                sleepNow = true;
                if (!dontSleep) {
                    PRINT("Light sleep set now!\n");
                    mqtt_stop();
                    esp_wifi_stop();
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    esp_sleep_enable_gpio_wakeup();
                    esp_light_sleep_start();
                    sleep_count = 1;
                    PRINT("Awoke from a light sleep\n");
                }
            }
        } else {
            powerLow = false;
            if (sleep_count != 0) {
                PRINT("External power high.\n");
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
            .pull_down_en = GPIO_PULLDOWN_ENABLE };

    ESP_ERROR_CHECK(gpio_config(&config_extpower));
    gpio_wakeup_enable(sleep_gpio_num1, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(sleep_gpio_num2, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(extpower_gpio_num, GPIO_INTR_HIGH_LEVEL);

    if (!SLEEP_MODE_ON) return;

    xTaskCreate(&light_sleep_task, "light_sleep_task", 2048, NULL, 0, NULL);
}

char* getVoltage() {

    const int COUNT = 20;

    static char buf[32];
    static esp_adc_cal_characteristics_t adc_chars;
    const adc1_channel_t VBAT = VBUS
    ;
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

    sprintf(buf, "Battery: %d,%dV", voltage / 1000, (voltage % 1000) / 10);

    return buf;
}

void strtrim(char *s) {

    /* With begin */
    int i = 0, j;
    while ((s[i] == ' ') || (s[i] == '\t')) {
        i++;
    }
    if (i > 0) {
        for (j = 0; j < strlen(s); j++) {
            s[j] = s[j + i];
        }
        s[j] = '\0';
    }

    /* With end */
    i = strlen(s) - 1;
    while ((s[i] == ' ') || (s[i] == '\t')) { /* || (s[i] == '\r') || (s[i] == '\n') */
        i--;
    }
    if (i < (strlen(s) - 1)) {
        s[i + 1] = '\0';
    }
}

/* filename - full path and name, ex. - "/spiffs/path/text.html" */
char* read_file(const char *fileName) {

    char *rbuff = NULL;
    FILE *fp;
    size_t flen;

    if (sdcard || spiffs) {
        fp = fopen(fileName, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            flen = ftell(fp);

            if (flen > 1) {
                if (flen >= MAX_BUFF_RW) {
                    WM_LOGE(TAG,
                            "The file size is too large. \"%s\" len - %u, more, than %u. (%s:%u)",
                            fileName, flen, MAX_BUFF_RW, __FILE__, __LINE__);
                    fclose(fp);
                    return NULL;
                }
            } else {
                WM_LOGE(TAG, "Empty file - \"%s\". (%s:%u)", fileName, __FILE__, __LINE__);
                fclose(fp);
                return NULL;
            }

            rewind(fp);

            rbuff = malloc(flen + 1);

            if (rbuff == NULL) {
                WM_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
                fclose(fp);
                return NULL;
            }

            memset(rbuff, 0, flen + 1);

            if (fread(rbuff, sizeof(uint8_t), flen, fp) != flen) {
                WM_LOGE(TAG, "File \"%s\" read error from %s. (%s:%u)",
                        fileName, sdcard ? "sdcard" : "spiffs", __FILE__, __LINE__);
                fclose(fp);
                free(rbuff);
                return NULL;
            }

            fclose(fp);

        } else {
            WM_LOGE(TAG, "Cannot open file \"%s\". (%s:%u)", fileName, __FILE__, __LINE__);
        }
    } else {
        WM_LOGE(TAG, "Not initializing sdcard or spiffs. (%s:%u)", __FILE__, __LINE__);
    }

    return rbuff;
}

bool write_file(const char *fileName, const char *wbuff, size_t flen) {

    FILE *fp;
    bool ret = false;

    if (sdcard || spiffs) {
        fp = fopen(fileName, "wb");
        if (fp) {
            fseek(fp, 0, SEEK_SET);
            if (fwrite(wbuff, sizeof(uint8_t), flen, fp) != flen) {
                WM_LOGE(TAG, "File \"%s\" write error to %s. (%s:%u)",
                        fileName, sdcard ? "sdcard" : "spiffs",
                        __FILE__, __LINE__);
                fclose(fp);
                return ret;
            }
            ret = true;
            fclose(fp);

        } else {
            WM_LOGE(TAG, "Cannot open file \"%s\". (%s:%u)", fileName, __FILE__, __LINE__);
        }
    } else {
        WM_LOGE(TAG, "Not initializing sdcard or spiffs. (%s:%u)", __FILE__, __LINE__);
    }

    return ret;
}

int my_printf(const char *frm, ...) {

    va_list args;
    char *buff, *out_str, ltime[32];
    int len_buff = 0;
    int len_out_str = 0;
    time_t now;
    struct tm timeinfo;

    va_start(args, frm);

    len_buff = vasprintf(&buff, frm, args);

    va_end(args);

    if (buff == NULL)
        return 0;

    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        sprintf(ltime, "%u", esp_log_timestamp());
    } else {
        sprintf(ltime, "%s.%.2lu %s", localTimeStr(), get_msec(), localDateStr());
    }

    len_out_str = asprintf(&out_str, "[%s] %s", ltime, buff);

    if (out_str == NULL || strstr(buff, "E (")) {
        printf(buff);
        write_to_lstack(buff);
        return len_buff;
    }

    printf(out_str);
    write_to_lstack(out_str);
    free(buff);

    return len_out_str;
}

int my_vprintf(const char *frm, va_list args) {

    char *buff;
    int len = 0;

    len = vasprintf(&buff, frm, args);

    if (buff == NULL)
        return 0;

#if ESP_LOG_OUT
	printf(buff);

	write_to_lstack(buff);
#else
    free(buff);
#endif

    return len;
}

