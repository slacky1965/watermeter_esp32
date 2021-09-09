#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "wm_config.h"
#include "wm_mqtt.h"

#define MYMACSTR "%02X%02X%02X%02X%02X%02X"
#define MAXMQTTCOUNT 5

static const char *TAG = "watermeter_mqtt";

static char topic_water[4][MQTT_TOPIC_SIZE];

static esp_mqtt_client_handle_t client;

static uint8_t mqtt_count_disconnect;

static bool connected = false;
static bool saveNewConfig = false;
static bool mqttFirstStart = true;

char *mqtt_get_topic(topic_name tn) {
	return topic_water[tn];
}

static void mqtt_data(const char *topic, size_t topic_len, const char *data, size_t data_len) {

	char *p, *data_buf = NULL;
	const char snew[] = "NEW";
	uint32_t waterFromServer;
	time_t timeFromServer;
	uint8_t pos;

	data_buf = malloc(data_len+1);

	if (!data_buf) {
		ESP_LOGE(TAG, "Error allocation memory");
		return;
	}

	memcpy(data_buf, data, data_len);
	data_buf[data_len] = 0;

	printf("-- %s ==> %.*s\n", data_buf, topic_len, topic);

	pos = strcspn(data_buf, " ");

	if (pos != strlen(data_buf)) {

		data_buf[pos] = 0;
		timeFromServer = strtoul(data_buf, 0, 10);
		p = data_buf + pos + 1;
		waterFromServer = strtoul(p, 0, 10);

		if (strstr(topic, END_TOPIC_HOT_IN)) {
			if (strstr(p, snew)) {
				config_set_hotWater(waterFromServer);
				config_set_hotTime(timeFromServer);
				saveNewConfig = true;
			} else if (waterFromServer > config_get_hotWater()) {
				config_set_hotWater(waterFromServer + config_get_litersPerPulse());
				config_set_hotTime(timeFromServer);
				saveNewConfig = true;
			}
		} else if (strstr(topic, END_TOPIC_COLD_IN)) {
			if (strstr(p, snew)) {
				config_set_coldWater(waterFromServer);
				config_set_coldTime(timeFromServer);
				saveNewConfig = true;
			} else if (waterFromServer > config_get_coldWater()) {
				config_set_coldWater(waterFromServer + config_get_litersPerPulse());
				config_set_coldTime(timeFromServer);
				saveNewConfig = true;
			}
		}
	}

	if (saveNewConfig) {
		saveNewConfig = false;
		saveConfig();
	}

	free(data_buf);

}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {

	esp_mqtt_client_handle_t client = event->client;

	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			mqtt_count_disconnect = 0;
			connected = true;
			if (mqttFirstStart) {
				printf("-- Full name input topic for hot water:   %s\n", topic_water[hot_in]);
				printf("-- Full name input topic for cold water:  %s\n", topic_water[cold_in]);
				printf("-- Full name output topic for hot water:  %s\n", topic_water[hot_out]);
				printf("-- Full name output topic for cold water: %s\n", topic_water[cold_out]);
				mqttFirstStart = false;
			}
			esp_mqtt_client_subscribe(client, topic_water[hot_in], 0);
			esp_mqtt_client_subscribe(client, topic_water[cold_in], 0);
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			connected = false;
			break;
		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			mqtt_data(event->topic, event->topic_len, event->data, event->data_len);
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			if (mqtt_count_disconnect < MAXMQTTCOUNT) {
				mqtt_count_disconnect++;
			} else {
				mqtt_stop();
				mqtt_count_disconnect = 0;
				connected = false;
			}
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
	return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

bool mqtt_connected() {
	return connected;
}

int mqtt_publish(const char *topic, const char *data) {
	int qos = 0;
	int retain = 0;
	int ret;
	ret = esp_mqtt_client_publish(client, topic, data, strlen(data), qos, retain);
	return ret;
}

void mqtt_check_task(void *pvParameter) {

	const TickType_t xDelay = 300 / portTICK_PERIOD_MS;
	static uint8_t count = 0;
	const uint8_t CMAX = 20;

	for(;;) {
		if (!connected && staModeNow && !powerLow) {
			if (count < CMAX) {
				count++;
			} else {
				count = 0;
				mqtt_restart();
			}
		}
		vTaskDelay(xDelay);
	}
}

void mqtt_init() {

	static char broker[MAX_MQTTBROKER+16];
	const char *username = config_get_mqttUser();
	const char *password = config_get_mqttPassword();
	char *p, *br = config_get_mqttBroker();
	uint8_t mac[6];

	printf("-- Initializing mqtt client\n");

	mqtt_count_disconnect = 0;

	esp_base_mac_addr_get(mac);

	sprintf(topic_water[hot_in], DELIM MQTT_TOPIC "/%02X%02X%02X%02X%02X%02X/" END_TOPIC_HOT_IN, MAC2STR(mac));
	sprintf(topic_water[cold_in], DELIM MQTT_TOPIC "/%02X%02X%02X%02X%02X%02X/" END_TOPIC_COLD_IN, MAC2STR(mac));
	sprintf(topic_water[hot_out], DELIM MQTT_TOPIC "/%02X%02X%02X%02X%02X%02X/" END_TOPIC_HOT_OUT, MAC2STR(mac));
	sprintf(topic_water[cold_out], DELIM MQTT_TOPIC "/%02X%02X%02X%02X%02X%02X/" END_TOPIC_COLD_OUT, MAC2STR(mac));

	p = strstr(br, "://");

	if (p) {
		br = p+3;
	}

	sprintf(broker, "mqtt://%s", br);

	esp_mqtt_client_config_t mqtt_cfg = {
		.uri = broker,
		.port = MQTT_PORT,
		.username = username,
		.password = password,
	};

	client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
	xTaskCreate(&mqtt_check_task, "mqtt_check_task", 2048, NULL, 0, NULL);

}

void mqtt_start() {
	ESP_LOGI(TAG, "Start mqtt client");
	mqtt_count_disconnect = 0;
	esp_mqtt_client_start(client);
}

void mqtt_stop() {
	if (connected) {
		ESP_LOGI(TAG, "Stop mqtt client");
		esp_mqtt_client_stop(client);
		connected = false;
	}
}

void mqtt_restart() {
	ESP_LOGI(TAG, "Restart mqtt client");
	esp_mqtt_client_stop(client);
	vTaskDelay(10000 / portTICK_PERIOD_MS);
	esp_mqtt_client_start(client);
}

void mqtt_reinit() {
	static char broker[MAX_MQTTBROKER+16];
	const char *username = config_get_mqttUser();
	const char *password = config_get_mqttPassword();
	char *p, *br = config_get_mqttBroker();
	uint8_t mac[6];

	ESP_LOGI(TAG, "Reinit mqtt client");

	mqtt_count_disconnect = 0;
	mqttFirstStart = true;

	esp_base_mac_addr_get(mac);

	sprintf(topic_water[hot_in], DELIM MQTT_TOPIC "/%02X%02X%02X%02X%02X%02X/" END_TOPIC_HOT_IN, MAC2STR(mac));
	sprintf(topic_water[cold_in], DELIM MQTT_TOPIC "/%02X%02X%02X%02X%02X%02X/" END_TOPIC_COLD_IN, MAC2STR(mac));
	sprintf(topic_water[hot_out], DELIM MQTT_TOPIC "/%02X%02X%02X%02X%02X%02X/" END_TOPIC_HOT_OUT, MAC2STR(mac));
	sprintf(topic_water[cold_out], DELIM MQTT_TOPIC "/%02X%02X%02X%02X%02X%02X/" END_TOPIC_COLD_OUT, MAC2STR(mac));

	p = strstr(br, "://");

	if (p) {
		br = p+3;
	}

	sprintf(broker, "mqtt://%s", br);

	esp_mqtt_client_config_t mqtt_cfg = {
		.uri = broker,
		.port = MQTT_PORT,
		.username = username,
		.password = password,
	};

	esp_mqtt_client_stop(client);
	vTaskDelay(10000 / portTICK_PERIOD_MS);
	esp_mqtt_set_config(client , &mqtt_cfg);
	esp_mqtt_client_start(client);
}
