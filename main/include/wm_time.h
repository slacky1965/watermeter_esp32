#ifndef MAIN_INCLUDE_WM_TIME_H_
#define MAIN_INCLUDE_WM_TIME_H_

/* default URL of NTP server */
#define NTP_SERVER_NAME "pool.ntp.org"

void setTimeStart(uint64_t ts);
uint64_t getTimeStart();
void sntp_start_handler();
void sntp_reinitialize();
char *localUpTime();
char *localDateTimeStr();
char *localDateStr();
char *localTimeStr();
void set_time_zone();
suseconds_t get_msec();

#endif /* MAIN_INCLUDE_WM_TIME_H_ */
