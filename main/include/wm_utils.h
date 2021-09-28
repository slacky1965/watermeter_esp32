#ifndef MAIN_INCLUDE_WM_UTILS_H_
#define MAIN_INCLUDE_WM_UTILS_H_

char *getVoltage();
void light_sleep_init();
void reset_sleep_count();
void strtrim(char *s);
char *read_file(const char *fileName);
bool write_file(const char *fileName, const char *wbuff, size_t flen);
int my_printf(const char *frm, ...);
int my_vprintf(const char *frm, va_list args);

#endif /* MAIN_INCLUDE_WM_UTILS_H_ */
