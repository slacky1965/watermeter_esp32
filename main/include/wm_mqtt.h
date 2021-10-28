#ifndef MAIN_INCLUDE_WM_MQTT_H_
#define MAIN_INCLUDE_WM_MQTT_H_

#include "wm_filesys.h"
#include "wm_httpd.h"
#include "wm_utils.h"

/* For MQTT client server */
#define MQTT_SSL_ENABLE true
#define END_TOPIC_HOT_IN "HotWater" DELIM "In"
#define END_TOPIC_COLD_IN "ColdWater" DELIM "In"
#define END_TOPIC_HOT_OUT "HotWater" DELIM "Out"
#define END_TOPIC_COLD_OUT "ColdWater" DELIM "Out"
#define END_TOPIC_CONTROL "Control"
#define SIZE_END_TOPIC 32
#define MQTT_TOPIC_SIZE 64

/* For MQTT commands */
#define CMD_HTTP_RESTART    "http_restart"
#define CMD_REBOOT          "reboot"
#define CMD_NEW_CERT        "new_cert_mqtt"

/* MQTT server port */
#if MQTT_SSL_ENABLE
#define MQTT_PORT   8883
#else
#define MQTT_PORT   1883
#endif


typedef enum {
    hot_in = 0,
    hot_out,
    cold_in,
    cold_out,
    control,
    topic_name_max
} topic_name;

void mqtt_init();
void mqtt_start();
void mqtt_stop();
void mqtt_restart();
bool mqtt_connected();
char *mqtt_get_topic(topic_name tn);
int mqtt_publish(const char *topic, const char *data);
bool mqtt_new_cert_from_webserver(const char *cert);

#endif /* MAIN_INCLUDE_WM_MQTT_H_ */
