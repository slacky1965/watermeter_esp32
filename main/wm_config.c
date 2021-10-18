#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "wm_config.h"
#include "wm_filesys.h"
#include "wm_time.h"

static const char *TAG = "watermeter_config";
static char configFileName[128];

static watermeter_config_t watermeter_config;

uint32_t crc_table[16] = { 0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
        0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c, 0xedb88320, 0xf00f9344,
        0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c };

static uint32_t crc_update(uint8_t *buf, uint32_t len) {
    unsigned long crc = ~0L;
    int i;

    for (i = 0; i < len; i++) {
        crc = crc_table[(crc ^ buf[i]) & 0x0f] ^ (crc >> 4);
        crc = crc_table[(crc ^ (buf[i] >> 4)) & 0x0f] ^ (crc >> 4);
        crc = ~crc;
    }
    return crc;
}

void copyConfig(watermeter_config_t *destcfg, watermeter_config_t *origcfg) {
    memcpy(destcfg, origcfg, sizeof(watermeter_config_t));
}

void criptConfig(watermeter_config_t *config) {

    uint8_t *buf = (uint8_t*) config;

    int i;

    for (i = sizeof(watermeter_config_t) + 1; i > 0; i--) {
        if (i == 1)
            buf[i - 1] = buf[i - 1] ^ CRIPTBYTE;
        else
            buf[i - 1] = buf[i - 1] ^ buf[i - 2];
    }

}

void decriptConfig(watermeter_config_t *config) {

    uint8_t *buf = (uint8_t*) config;

    int i;

    for (i = 0; i < sizeof(watermeter_config_t) + 1; i++) {
        if (i == 0)
            buf[i] = buf[i] ^ CRIPTBYTE;
        else
            buf[i] = buf[i] ^ buf[i - 1];
    }

}

void clearConfig(watermeter_config_t *config) {
    memset(config, 0, sizeof(watermeter_config_t));
}

void initDefConfig() {

    clearConfig(&watermeter_config);
    strcpy(watermeter_config.webAdminLogin, WEB_ADMIN_LOGIN);
    strcpy(watermeter_config.webAdminPassword, WEB_ADMIN_PASSWORD);
    watermeter_config.fullSecurity = false;
    watermeter_config.configSecurity = false;
    memset(watermeter_config.staSsid, 0, sizeof(watermeter_config.staSsid));
    memset(watermeter_config.staPassword, 0, sizeof(watermeter_config.staPassword));
    watermeter_config.apMode = true;
    strcpy(watermeter_config.apSsid, AP_SSID);
    strcpy(watermeter_config.apPassword, AP_PASSWORD);
    strcpy(watermeter_config.mqttBroker, MQTT_BROKER);
    strcpy(watermeter_config.mqttUser, MQTT_USER);
    strcpy(watermeter_config.mqttPassword, MQTT_PASSWORD);
    strcpy(watermeter_config.mqttTopic, MQTT_TOPIC);
    strcpy(watermeter_config.ntpServerName, NTP_SERVER_NAME);
    watermeter_config.timeZone = TIME_ZONE;
    watermeter_config.litersPerPulse = LITERS_PER_PULSE;
    watermeter_config.hotTime = watermeter_config.coldTime = time(NULL);
    watermeter_config.hotWater = watermeter_config.coldWater = 0;

    staConfigure = false;
}

/* copy from tmp watermeter_config_t to watermeter_config */
void setConfig(watermeter_config_t *config) {

    clearConfig(&watermeter_config);

    strcpy(watermeter_config.webAdminLogin, config->webAdminLogin);
    strcpy(watermeter_config.webAdminPassword, config->webAdminPassword);
    watermeter_config.fullSecurity = config->fullSecurity;
    watermeter_config.configSecurity = config->configSecurity;
    strcpy(watermeter_config.staSsid, config->staSsid);
    strcpy(watermeter_config.staPassword, config->staPassword);
    watermeter_config.apMode = config->apMode;
    strcpy(watermeter_config.apSsid, config->apSsid);
    strcpy(watermeter_config.apPassword, config->apPassword);
    strcpy(watermeter_config.mqttBroker, config->mqttBroker);
    strcpy(watermeter_config.mqttUser, config->mqttUser);
    strcpy(watermeter_config.mqttPassword, config->mqttPassword);
    strcpy(watermeter_config.mqttTopic, config->mqttTopic);
    strcpy(watermeter_config.ntpServerName, config->ntpServerName);
    watermeter_config.timeZone = config->timeZone;
    watermeter_config.litersPerPulse = config->litersPerPulse;
    watermeter_config.hotTime = config->hotTime;
    watermeter_config.coldTime = config->coldTime;
    watermeter_config.hotWater = config->hotWater;
    watermeter_config.coldWater = config->coldWater;

}

//void printConfig() {
//	printf("webAdminLogin: \"%s\"\n", watermeter_config.webAdminLogin);
//	printf("webAdminPassword: \"%s\"\n", watermeter_config.webAdminPassword);
//	printf("fullSecurity: %s\n", watermeter_config.fullSecurity?"true":"false");
//	printf("configSecurity: %s\n", watermeter_config.configSecurity?"true":"false");
//	printf("staSsid: \"%s\"\n", watermeter_config.staSsid);
//	printf("staPassword: \"%s\"\n", watermeter_config.staPassword);
//	printf("apMode: %s\n", watermeter_config.apMode?"true":"false");
//	printf("apSsid: \"%s\"\n", watermeter_config.apSsid);
//	printf("apPassword: \"%s\"\n", watermeter_config.apPassword);
//	printf("mqttBroker: \"%s\"\n", watermeter_config.mqttBroker);
//	printf("mqttUser: \"%s\"\n", watermeter_config.mqttUser);
//	printf("mqttPassword: \"%s\"\n", watermeter_config.mqttPassword);
//	printf("mqttTopic: \"%s\"\n", watermeter_config.mqttTopic);
//	printf("ntpServerName: \"%s\"\n", watermeter_config.ntpServerName);
//	printf("timeZone: %d\n", watermeter_config.timeZone);
//	printf("litersPerPulse: %d\n", watermeter_config.litersPerPulse);
//	printf("hotTime: %ld\n", watermeter_config.hotTime);
//	printf("coldTime: %ld\n", watermeter_config.coldTime);
//	printf("hotWater: %d\n", watermeter_config.hotWater);
//	printf("coldWater: %d\n", watermeter_config.coldWater);
//}
//
//void printTMPConfig(watermeter_config_t *config) {
//	printf("webAdminLogin: \"%s\"\n", config->webAdminLogin);
//	printf("webAdminPassword: \"%s\"\n", config->webAdminPassword);
//	printf("fullSecurity: %s\n", config->fullSecurity?"true":"false");
//	printf("configSecurity: %s\n", config->configSecurity?"true":"false");
//	printf("staSsid: \"%s\"\n", config->staSsid);
//	printf("staPassword: \"%s\"\n", config->staPassword);
//	printf("apMode: %s\n", config->apMode?"true":"false");
//	printf("apSsid: \"%s\"\n", config->apSsid);
//	printf("apPassword: \"%s\"\n", config->apPassword);
//	printf("mqttBroker: \"%s\"\n", config->mqttBroker);
//	printf("mqttUser: \"%s\"\n", config->mqttUser);
//	printf("mqttPassword: \"%s\"\n", config->mqttPassword);
//	printf("mqttTopic: \"%s\"\n", config->mqttTopic);
//	printf("ntpServerName: \"%s\"\n", config->ntpServerName);
//	printf("timeZone: %d\n", config->timeZone);
//	printf("litersPerPulse: %d\n", config->litersPerPulse);
//	printf("hotTime: %ld\n", config->hotTime);
//	printf("coldTime: %ld\n", config->coldTime);
//	printf("hotWater: %d\n", config->hotWater);
//	printf("coldWater: %d\n", config->coldWater);
//}

void removeConfig() {

    struct stat st;

    if (sdcard || spiffs) {
        if (stat(configFileName, &st) == 0) {
            // Delete it if it exists
            if (unlink(configFileName) != 0) {
                WM_LOGE(TAG, "Can\'t delete config file \"%s\". (%s:%u)", configFileName, __FILE__, __LINE__);
            } else {
                ESP_LOGI(TAG, "Config file \"%s\" is delete. (%s:%u)", configFileName, __FILE__, __LINE__);
            }
        }
    } else {
        WM_LOGE(TAG, "Not initializing sdcard or spiffs. (%s:%u)", __FILE__, __LINE__);
    }
}

bool readConfig() {

    watermeter_config_t config;
    bool ret = false;
    uint32_t crc_cfg, crc;

    if (sdcard || spiffs) {
        /* read from spiffs */
        FILE *file = fopen(configFileName, "rb");
        if (file) {
            fseek(file, 0, 0);
            if (fread(&config, 1, sizeof(watermeter_config_t), file)
                    != sizeof(watermeter_config_t)) {
                WM_LOGE(TAG, "Config file \"%s\" read error from %s! (%s:%u)",
                        configFileName, sdcard ? "sdcard" : "spiffs", __FILE__, __LINE__);
                fclose(file);
                return ret;
            }

            decriptConfig(&config);
            if (fread(&crc_cfg, 1, sizeof(crc), file) != sizeof(crc)) {
                WM_LOGE(TAG, "Crc read error from spiffs! (%s:%u)", __FILE__, __LINE__);
                fclose(file);
                return ret;

            }
            fclose(file);

            crc = crc_update((uint8_t*) &config, sizeof(watermeter_config_t));
            if (crc != crc_cfg)
                return ret;

            setConfig(&config);
            if (watermeter_config.staSsid[0] != 0)
                staConfigure = true;
            ret = true;
            PRINT("Config is read from \"%s\" file\n", configFileName);
        } else {
            WM_LOGE(TAG, "Config file \"%s\" read error! (%s:%u)", configFileName, __FILE__, __LINE__);
        }
    } else {
        WM_LOGE(TAG, "Not initializing sdcard or spiffs. (%s:%u)", __FILE__, __LINE__);
    }

    return ret;
}

void saveConfig() {

    uint32_t crc;
    crc = crc_update((uint8_t*) &watermeter_config, sizeof(watermeter_config_t));
    watermeter_config_t config;
    memcpy(&config, &watermeter_config, sizeof(watermeter_config_t));

    criptConfig(&config);

    if (sdcard || spiffs) {
        FILE *file = fopen(configFileName, "wb");
        if (file) {
            fseek(file, 0, 0);
            if (fwrite(&config, 1, sizeof(watermeter_config_t), file) != sizeof(watermeter_config_t)) {
                WM_LOGE(TAG, "Config file \"%s\" write error to %s! (%s:%u)",
                        configFileName, sdcard ? "sdcard" : "spiffs", __FILE__, __LINE__);
                fclose(file);
                return;
            }
            if (fwrite(&crc, 1, sizeof(crc), file) != sizeof(crc)) {
                WM_LOGE(TAG, "Crc write error to %s! (%s:%u)", sdcard ? "sdcard" : "spiffs", __FILE__, __LINE__);
                fclose(file);
                return;
            }
            fclose(file);
            PRINT("Save config to %s file\n", configFileName);
        } else
            WM_LOGE(TAG, "Config file \"%s\" write error! (%s:%u)", configFileName, __FILE__, __LINE__);
    } else {
        WM_LOGE(TAG, "Not initializing sdcard or spiffs. (%s:%u)", __FILE__, __LINE__);
    }
}

void set_configFileName(char *path) {
    sprintf(configFileName, "%s%s%s", path, DELIM, WM_CONFIG_FILE);
}

char* config_get_webAdminLogin() {
    return watermeter_config.webAdminLogin;
}

char* config_get_webAdminPassword() {
    return watermeter_config.webAdminPassword;
}

bool config_get_fullSecurity() {
    return watermeter_config.fullSecurity;
}

void config_set_fullSecurity(bool set) {
    watermeter_config.fullSecurity = set;
}

bool config_get_configSecurity() {
    return watermeter_config.configSecurity;
}

void config_set_configSecurity(bool set) {
    watermeter_config.configSecurity = set;
}

char* config_get_staSsid() {
    return watermeter_config.staSsid;
}

void config_set_staSsid(const char *ssid) {
    strcpy(watermeter_config.staSsid, ssid);
}

char* config_get_staPassword() {
    return watermeter_config.staPassword;
}
void config_set_staPassword(const char *password) {
    strcpy(watermeter_config.staPassword, password);
}

bool config_get_apMode() {
    return watermeter_config.apMode;
}

char* config_get_apSsid() {
    return watermeter_config.apSsid;
}

char* config_get_apPassword() {
    return watermeter_config.apPassword;
}

char* config_get_mqttBroker() {
    return watermeter_config.mqttBroker;
}

char* config_get_mqttUser() {
    return watermeter_config.mqttUser;
}

char* config_get_mqttPassword() {
    return watermeter_config.mqttPassword;
}

char* config_get_mqttTopic() {
    return watermeter_config.mqttTopic;
}

char* config_get_ntpServerName() {
    return watermeter_config.ntpServerName;
}

uint8_t config_get_timeZone() {
    return watermeter_config.timeZone;
}

uint8_t config_get_litersPerPulse() {
    return watermeter_config.litersPerPulse;
}

time_t config_get_hotTime() {
    return watermeter_config.hotTime;
}

void config_set_hotTime(time_t time) {
    watermeter_config.hotTime = time;
}

uint32_t config_get_hotWater() {
    return watermeter_config.hotWater;
}

void config_set_hotWater(uint32_t water) {
    watermeter_config.hotWater = water;
}

time_t config_get_coldTime() {
    return watermeter_config.coldTime;
}

void config_set_coldTime(time_t time) {
    watermeter_config.coldTime = time;
}

uint32_t config_get_coldWater() {
    return watermeter_config.coldWater;
}

void config_set_coldWater(uint32_t water) {
    watermeter_config.coldWater = water;
}

