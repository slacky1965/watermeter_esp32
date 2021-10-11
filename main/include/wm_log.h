#ifndef MAIN_INCLUDE_WM_LOG_H_
#define MAIN_INCLUDE_WM_LOG_H_

/* number of log lines */
#define LSTACK_SIZE 30

#define WM_LOG_COLOR_BLACK  "30"
#define WM_LOG_COLOR_RED    "31"
#define WM_LOG_COLOR_GREEN  "32"
#define WM_LOG_COLOR_BROWN  "33"
#define WM_LOG_COLOR_BLUE   "34"
#define WM_LOG_COLOR_PURPLE "35"
#define WM_LOG_COLOR_CYAN   "36"
#define WM_LOG_COLOR        "\033[0;" WM_LOG_COLOR_RED "m"
#define WM_LOG_COLOR_RESET  "\033[0m"


typedef struct lstack_elem_t lstack_elem_t;

struct lstack_elem_t {
	lstack_elem_t *prev;
	lstack_elem_t *next;
	char *str;
};

bool write_to_lstack(char *str);
lstack_elem_t *get_lstack();
void wm_log(const char * tag, const char *str, ...);

#endif /* MAIN_INCLUDE_WM_LOG_H_ */
