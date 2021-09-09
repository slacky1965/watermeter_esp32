#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_sntp.h"

#include "wm_config.h"

static const char *TAG = "watermeter_time";
uint64_t timeStart;
static char buff[18] = { 0 };


void setTimeStart(uint64_t ts) {
	timeStart = ts;
}

uint64_t getTimeStart() {
	return timeStart;
}

void set_time_zone() {

	char time_zone[32];

	int8_t tz = config_get_timeZone();

	if (tz > 0) {
		sprintf(time_zone, "UTC-%d", tz);
	} else if (tz < 0) {
		sprintf(time_zone, "UTC+%d", tz);
	} else {
		sprintf(time_zone, "UTC");
	}

	setenv("TZ", time_zone, 1);
    tzset();
}

static void watermeter_time_sync_notification(struct timeval *tv) {
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}


void wm_sntp_init() {

	char *sntp_server = config_get_ntpServerName();

    printf("-- Initializing SNTP. Server: %s, TZ: %d\n", sntp_server, config_get_timeZone());

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_setservername(0, sntp_server);
    sntp_set_time_sync_notification_cb(watermeter_time_sync_notification);
    sntp_init();
}

void sntp_reinit() {
	sntp_stop();
	set_time_zone();
	wm_sntp_init();
}

char *localUpTime() {

	char tmp[6] = { 0 };

	uint32_t secs = esp_timer_get_time() / 1000000, mins = secs / 60;
	uint16_t hours = mins / 60, days = hours / 24;

	secs -= mins * 60;
	mins -= hours * 60;
	hours -= days * 24;

	if (days) {
		sprintf(tmp, "%ud", days);
	}

	sprintf(buff, "%s %02u:%02u:%02u", tmp[0] == 0 ? "" : tmp, hours, mins, secs);

	return buff;
}

char *localTimeStr() {

	static char strftime_buf[64];
	struct tm timeinfo;
	time_t now;

	time(&now);

	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "Local time: %X %d.%m.%Y", &timeinfo);
	return strftime_buf;
}

