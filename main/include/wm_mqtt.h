#ifndef MAIN_INCLUDE_WM_MQTT_H_
#define MAIN_INCLUDE_WM_MQTT_H_

#include "wm_filesys.h"

/* For MQTT client server */
//#define MQTT_SSL_ENABLE
#define END_TOPIC_HOT_IN "HotWater" DELIM "In"
#define END_TOPIC_COLD_IN "ColdWater" DELIM "In"
#define END_TOPIC_HOT_OUT "HotWater" DELIM "Out"
#define END_TOPIC_COLD_OUT "ColdWater" DELIM "Out"
#define SIZE_END_TOPIC 32
#define MQTT_TOPIC_SIZE  64

#define MQTT_PORT     1883

typedef enum {
	hot_in = 0,
	hot_out,
	cold_in,
	cold_out
} topic_name;

void mqtt_init();
void mqtt_start();
void mqtt_stop();
void mqtt_restart();
void mqtt_reinit();
bool mqtt_connected();
char *mqtt_get_topic(topic_name tn);
int mqtt_publish(const char *topic, const char *data);

#endif /* MAIN_INCLUDE_WM_MQTT_H_ */
