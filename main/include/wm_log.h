#ifndef MAIN_INCLUDE_WM_LOG_H_
#define MAIN_INCLUDE_WM_LOG_H_

/* number of log lines */
#define LSTACK_SIZE		30

typedef struct lstack_elem_t lstack_elem_t;

struct lstack_elem_t {
	lstack_elem_t *prev;
	lstack_elem_t *next;
	char *str;
};

bool write_to_lstack(char *str);
lstack_elem_t *get_lstack();

#endif /* MAIN_INCLUDE_WM_LOG_H_ */
