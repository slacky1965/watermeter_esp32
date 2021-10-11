#ifndef MAIN_INCLUDE_WM_HTTPD_H_
#define MAIN_INCLUDE_WM_HTTPD_H_

#define HTML_PATH MOUNT_POINT_SPIFFS DELIM "html"

void webserver_init(const char *html_path);
void webserver_restart();

#endif /* MAIN_INCLUDE_WM_HTTPD_H_ */
