#ifndef MAIN_INCLUDE_WM_GLOBAL_H_
#define MAIN_INCLUDE_WM_GLOBAL_H_

#include "esp_system.h"
#include "wm_utils.h"
#include "wm_log.h"

#define PRINT  my_printf						/* Use PRINT instead of printf				*/
#define WM_LOGE wm_log							/* Use WM_LOGE instead of ESP_LOGE			*/
#define ESP_LOG_OUT	false						/* false - no msg ESP_LOG*, only PRINT		 */
#define CRIPTBYTE	0xA3						/* For light crypt configure file	 		 */

#define SLEEP_MODE_ON false						/* To pass into sleep mode if true   		 */
#define NOT_READ_EEPROM true					/* Dont't read from EEPROM if true   		 */

/* Define pin name */
#define VBUS 	   ADC1_CHANNEL_7;				/* Pin of get voltage - GPIO35 		 		  */
#define HOT_PIN    25							/* Pin of hot water - GPIO25         		  */
#define COLD_PIN   26							/* Pin of cold water - GPIO26      	 		  */
#define EXT_PW_PIN 16							/* Pin of monitoring external power - GPIO16  */

/* Name and Version */
#define WEB_WATERMETER_FIRST_NAME "Water"
#define WEB_WATERMETER_LAST_NAME "Meter"
#define PLATFORM "Esp32 & Micro SD"
#define MODULE_VERSION "v4.0"
#define MODULE_NAME WEB_WATERMETER_FIRST_NAME WEB_WATERMETER_LAST_NAME " " MODULE_VERSION

/* sharing flags */
extern bool sleepNow;
extern bool dontSleep;
extern bool powerLow;
extern bool apModeNow;
extern bool staModeNow;
extern bool staApModeNow;
extern bool staConfigure;
extern bool firstStart;
extern bool subsHotWater;
extern bool subsColdWater;
extern bool spiffs;
extern bool sdcard;

#endif /* MAIN_INCLUDE_WM_GLOBAL_H_ */
