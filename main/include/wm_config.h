#ifndef MAIN_INCLUDE_WM_CONFIG_H_
#define MAIN_INCLUDE_WM_CONFIG_H_

#include "wm_global.h"

#define WM_CONFIG_FILE "watermeter.dat" 		 /* File name of config file		 */

/* For config (default settings) */
#define WEB_ADMIN_LOGIN "Admin"                  /* Login for web Auth                */
#define WEB_ADMIN_PASSWORD "1111"                /* Password for web Auth             */
#define AP_SSID "WaterMeter_" MODULE_VERSION	 /* Name WiFi AP Mode                 */
#define AP_PASSWORD "12345678"                   /* Password WiFi AP Mode             */
#define MQTT_USER "test"                         /* mqtt user                         */
#define MQTT_PASSWORD "1111"                     /* mqtt password                     */
#define MQTT_BROKER "192.168.1.1"			     /* URL mqtt broker                   */
#define MQTT_TOPIC "WaterMeter"                  /* Primary mqtt topic name           */
#define LITERS_PER_PULSE 10                      /* How many liters per one pulse     */
#define TIME_ZONE 3                              /* Default Time Zone                 */



#define MAX_WEBADMINLOGIN		16
#define MAX_WEBADMINPASSWORD	16
#define MAX_STASSID				16
#define MAX_STAPASSWORD			16
#define MAX_APSSID				16
#define MAX_APPASSWORD			16
#define MAX_MQTTBROKER			32
#define MAX_MQTTUSER			16
#define MAX_MQTTPASSWORD		16
#define MAX_MQTTTOPIC			64
#define MAX_NTPSERVERNAME		16

/* Structure of config file */
typedef struct _watermeter_config {
  char webAdminLogin[MAX_WEBADMINLOGIN];      		/* Login for web Auth                    */
  char webAdminPassword[MAX_WEBADMINPASSWORD];   	/* Password for web Auth                 */
  bool fullSecurity;  							    /* true - all web Auth, false - free     */
  bool configSecurity;         						/* true - only config and update Auth    */
  char staSsid[MAX_STASSID];			            /* STA SSID WiFi                         */
  char staPassword[MAX_STAPASSWORD]; 		        /* STA Password WiFi                     */
  bool apMode;                 						/* true - AP Mode, false - STA Mode      */
  char apSsid[MAX_APSSID];				            /* WiFi Name in AP mode                  */
  char apPassword[MAX_APPASSWORD]; 			        /* WiFi Password in AP mode              */
  char mqttBroker[MAX_MQTTBROKER]; 			        /* URL or IP-address of mqtt-broker      */
  char mqttUser[MAX_MQTTUSER];			            /* mqtt user                             */
  char mqttPassword[MAX_MQTTPASSWORD]; 			    /* mqtt password                         */
  char mqttTopic[MAX_MQTTTOPIC]; 			        /* mqtt topic                            */
  char ntpServerName[MAX_NTPSERVERNAME]; 		    /* URL or IP-address of NTP server       */
  int8_t timeZone;             						/* Time Zone                             */
  uint8_t litersPerPulse;      						/* liters per pulse                      */
  time_t hotTime;	           						/* Last update time of hot water         */
  uint32_t hotWater;           						/* Last number of liters hot water       */
  time_t coldTime; 	           						/* Last update time of cold water        */
  uint32_t coldWater;          						/* Last number of litres cold water      */
} watermeter_config_t;

char *config_get_webAdminLogin();
char *config_get_webAdminPassword();
bool config_get_fullSecurity();
void config_set_fullSecurity(bool set);
bool config_get_configSecurity();
void config_set_configSecurity(bool set);
char *config_get_staSsid();
void config_set_staSsid(const char *ssid);
char *config_get_staPassword();
void config_set_staPassword(const char *password);
bool config_get_apMode();
char *config_get_apSsid();
char *config_get_apPassword();
char *config_get_mqttBroker();
char *config_get_mqttUser();
char *config_get_mqttPassword();
char *config_get_mqttTopic();
char *config_get_ntpServerName();
uint8_t config_get_timeZone();
uint8_t config_get_litersPerPulse();
time_t config_get_hotTime();
void config_set_hotTime(time_t time);
uint32_t config_get_hotWater();
void config_set_hotWater(uint32_t water);
time_t config_get_coldTime();
void config_set_coldTime(time_t time);
uint32_t config_get_coldWater();
void config_set_coldWater(uint32_t water);

void copyConfig(watermeter_config_t *destcfg, watermeter_config_t *origcfg);
void criptConfig(watermeter_config_t *config);
void decriptConfig(watermeter_config_t *config);
void removeConfig();
void clearConfig(watermeter_config_t *config);
void initDefConfig();
void setConfig(watermeter_config_t *config);
void printConfig();
void printTMPConfig(watermeter_config_t *config);
bool readConfig();
void saveConfig();
void set_configFileName (char *path);

#endif /* MAIN_INCLUDE_WM_CONFIG_H_ */
