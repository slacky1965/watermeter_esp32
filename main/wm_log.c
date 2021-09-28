#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"

#include "wm_log.h"

char *TAG = "watermeter_log";

static lstack_elem_t *lstack = NULL;

lstack_elem_t *get_lstack() {

	return lstack;
}


/* delete color symbols and "\n" from str */
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
		ESP_LOGE(TAG, "Error allocation memory");
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


