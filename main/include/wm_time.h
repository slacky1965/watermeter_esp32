#ifndef MAIN_INCLUDE_WM_TIME_H_
#define MAIN_INCLUDE_WM_TIME_H_

/* default URL of NTP server */
#define NTP_SERVER_NAME "pool.ntp.org"

void setTimeStart(uint64_t ts);
uint64_t getTimeStart();
void sntp_initialize();
void sntp_reinitialize();
char *localUpTime();
char *localTimeStr();
void set_time_zone();

#endif /* MAIN_INCLUDE_WM_TIME_H_ */
