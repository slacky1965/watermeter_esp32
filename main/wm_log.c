#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "wm_global.h"
#include "wm_log.h"

char *TAG = "watermeter_log";

static lstack_elem_t *lstack = NULL;

lstack_elem_t *get_lstack() {

	return lstack;
}


/* delete color symbols, '\n', '<', '>' from str */
static char *trim_str_lstack(char *str) {

	char *str_dest, *str_source, *pos;

	str_dest = str_source = str;

	do {
		if (*str_source == '\033') {
			pos = strchr(str_source, 'm');
			if (pos) {
				str_source = pos+1;
			} else {
				str_source++;
			}
		}

		if (*str_source == '\n') str_source++;
		if (*str_source == '<')  str_source++;
		if (*str_source == '>')  str_source++;

		if (str_source != str_dest) {
			*str_dest = *str_source;
		}

		if (*str_source == '\0') break;

		str_dest++;
		str_source++;

	} while(1);

	return str;
}



bool write_to_lstack(char *str) {

	bool ret = false;

	lstack_elem_t *lstack_new, *lstack_current, *lstack_prev;

	if (!str) {
		return ret;
	}

	lstack_new = malloc(sizeof(lstack_elem_t));

	if (lstack_new == NULL) {
		WM_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
		return ret;
	}

	memset(lstack_new, 0, sizeof(lstack_elem_t));

	lstack_new->str = trim_str_lstack(str);

	if (!lstack) {
		lstack = lstack_new;
		ret = true;
	} else {
		lstack_current = lstack;
		uint8_t count_lstack = 0;
		for(;;) {
			if (lstack_current->next) {
				lstack_current = lstack_current->next;
				count_lstack++;
				continue;
			} else {
				lstack_current->next = lstack_new;
				lstack_new->prev = lstack_current;
				lstack_current = lstack_new;
				count_lstack++;
			}

			if (count_lstack > LSTACK_SIZE) {
				lstack_prev = lstack->next;
				free(lstack_prev->prev->str);
				free(lstack_prev->prev);
				lstack_prev->prev = NULL;
				lstack = lstack_prev;
			}
			ret = true;
			break;
		}
	}


	return ret;
}

void wm_log(const char * tag, const char *str, ...) {
	char *buff;
	char *out_str;

	va_list args;

	va_start(args, str);

	vasprintf(&buff, str, args);

	va_end(args);

	if (buff == NULL) {
		PRINT("%sE (%llu) %s: Error writing to the log of watermeter%s\n", WM_LOG_COLOR, esp_timer_get_time(), tag, WM_LOG_COLOR_RESET);
		return;
	}

	// "E (823) vfs_fat_sdmmc: slot init failed (0x103)."

	asprintf(&out_str, "%sE (%llu) %s: %s%s\n", WM_LOG_COLOR, esp_timer_get_time(), tag, buff, WM_LOG_COLOR_RESET);

	if (out_str == NULL) {
		PRINT("%s\n", buff);
	} else {
		free(buff);
		PRINT(out_str);
	}
}
