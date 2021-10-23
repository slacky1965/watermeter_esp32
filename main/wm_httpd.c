#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include <esp_https_server.h>
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

#include "wm_config.h"
#include "wm_utils.h"
#include "wm_filesys.h"
#include "wm_wifi.h"
#include "wm_time.h"
#include "wm_mqtt.h"
#include "wm_httpd.h"
#include "wm_log.h"

/* Buffer for OTA and another load or read from spiffs */
#define OTA_BUF_LEN	 1024

/* Legal URL web server */
#define	URL 		"/*"
#define ROOT        "/"
#define INDEX       "/index.html"
#define CONFIG      "/config.html"
#define UPLOAD      "/upload.html"
#define SETTINGS    "/settings.html"
#define LOG         "/log.html"
#define	UPLHTML 	"/uploadhtml"
#define	UPDATE	 	"/update"
#if MQTT_SSL_ENABLE
#define	UPDATECERT 	"/updatecertmqtt"
#endif

/* Out log color */
#define COLOR_E "<font color=#FF0000>"	/* error msg    - red               */
#define COLOR_W "<font color=#D2691E>"	/* warning msg  - chocolate         */
#define COLOR_I "<font color=#008000>"	/* info msg     - green             */
#define COLOR_D "<font color=#FFFF00>"	/* debug msg    - yellow            */
#define COLOR_V "<font color=#EE82EE>"	/* verbose msg  - violet, bad idea  */

static char *TAG = "watermeter_httpd";

static char *webserver_html_path = NULL;
static char *cacert = NULL;
static char *prvtkey = NULL;

static bool rebootNow = false;
static bool restartWiFi = false;
static bool defaultConfig = false;
static bool saveNewConfig = false;
static bool mqttRestart = false;
static bool sntpReInit = false;
static bool restartWebServer = false;

static esp_err_t webserver_response(httpd_req_t *req);
static esp_err_t webserver_update(httpd_req_t *req);
static esp_err_t webserver_upload_html(httpd_req_t *req);
static esp_err_t webserver_update_cert_mqtt(httpd_req_t *req);
static esp_err_t webserver_log(httpd_req_t *req);

static const httpd_uri_t uri_html = {
        .uri = URL,
        .method = HTTP_GET,
        .handler = webserver_response };

static const httpd_uri_t update_html = {
        .uri = UPDATE,
        .method = HTTP_POST,
        .handler = webserver_update };

static const httpd_uri_t uploadhtml_html = {
        .uri = UPLHTML,
        .method = HTTP_POST,
        .handler = webserver_upload_html };

static const httpd_uri_t log_html = {
        .uri = LOG,
        .method = HTTP_GET,
        .handler = webserver_log };

#if MQTT_SSL_ENABLE
static const httpd_uri_t updatecertmqtt_html = {
        .uri = UPDATECERT,
        .method = HTTP_POST,
        .handler = webserver_update_cert_mqtt };
#endif

static char* http_content_type(char *path) {
    char *ext = strrchr(path, '.');
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".tpl") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "text/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
    return "text/plain";
}

static esp_err_t webserver_redirect(httpd_req_t *req, const char *url) {

    char *h = "HTTP/1.1 302 Found\r\n\
Server: ESP32 " MODULE_NAME "\r\n\
Content-Type: text/html; charset=UTF-8\r\n\
Location: ";

    char buf[256];

    sprintf(buf, "%s%s\r\n\r\n", h, url);

    return httpd_send(req, buf, strlen(buf));
}

static char* webserver_subst_token_to_response(const char *token) {
    static char buff[128];
    if (token == NULL)
        return NULL;

    strcpy(buff, "Unknown");
    if (strcmp(token, "firstname") == 0) {
        strcpy(buff, WEB_WATERMETER_FIRST_NAME);
    } else if (strcmp(token, "lastname") == 0) {
        strcpy(buff, WEB_WATERMETER_LAST_NAME);
    } else if (strcmp(token, "platform") == 0) {
        strcpy(buff, PLATFORM);
    } else if (strcmp(token, "modulename") == 0) {
        strcpy(buff, MODULE_NAME);
    } else if (strcmp(token, "version") == 0) {
        strcpy(buff, MODULE_NAME);
    } else if (strcmp(token, "heap") == 0) {
        sprintf(buff, "%d", esp_get_free_heap_size());
    } else if (strcmp(token, "uptime") == 0) {
        strcpy(buff, localUpTime());
    } else if (strcmp(token, "volt") == 0) {
        strcpy(buff, getVoltage());
    } else if (strcmp(token, "rssi") == 0) {
        strcpy(buff, getRssi());
    } else if (strcmp(token, "localtime") == 0) {
        strcpy(buff, localTimeStr());
    } else if (strcmp(token, "adminlogin") == 0) {
        strcpy(buff, config_get_webAdminLogin());
    } else if (strcmp(token, "adminpassword") == 0) {
        strcpy(buff, config_get_webAdminPassword());
    } else if (strcmp(token, "fullsecurity") == 0) {
        if (config_get_fullSecurity()) strcpy(buff, "checked");
    } else if (strcmp(token, "configsecurity") == 0) {
        if (config_get_configSecurity()) strcpy(buff, "checked");
    } else if (strcmp(token, "wifistamode") == 0) {
        if (!config_get_apMode()) strcpy(buff, "checked");
    } else if (strcmp(token, "wifiapmode") == 0) {
        if (config_get_apMode()) strcpy(buff, "checked");
    } else if (strcmp(token, "wifiapname") == 0) {
        strcpy(buff, config_get_apSsid());
    } else if (strcmp(token, "wifiappassword") == 0) {
        strcpy(buff, config_get_apPassword());
    } else if (strcmp(token, "wifistaname") == 0) {
        strcpy(buff, config_get_staSsid());
    } else if (strcmp(token, "wifistapassword") == 0) {
        strcpy(buff, config_get_staPassword());
    } else if (strcmp(token, "mqttuser") == 0) {
        strcpy(buff, config_get_mqttUser());
    } else if (strcmp(token, "mqttpassword") == 0) {
        strcpy(buff, config_get_mqttPassword());
    } else if (strcmp(token, "mqttbroker") == 0) {
        strcpy(buff, config_get_mqttBroker());
    } else if (strcmp(token, "mqtttopic") == 0) {
        strcpy(buff, config_get_mqttTopic());
    } else if (strcmp(token, "ntpserver") == 0) {
        strcpy(buff, config_get_ntpServerName());
    } else if (strcmp(token, "timezone") == 0) {
        sprintf(buff, "%d", config_get_timeZone());
    } else if (strcmp(token, "hotwater") == 0) {
        sprintf(buff, "%u", config_get_hotWater());
    } else if (strcmp(token, "coldwater") == 0) {
        sprintf(buff, "%u", config_get_coldWater());
    } else if (strcmp(token, "litersperpulse") == 0) {
        sprintf(buff, "%d", config_get_litersPerPulse());
    } else if (strcmp(token, "hotwhole") == 0) {
        sprintf(buff, "%u", config_get_hotWater() / 1000);
    } else if (strcmp(token, "hotfract") == 0) {
        sprintf(buff, "%u", config_get_hotWater() % 1000);
    } else if (strcmp(token, "coldwhole") == 0) {
        sprintf(buff, "%u", config_get_coldWater() / 1000);
    } else if (strcmp(token, "coldfract") == 0) {
        sprintf(buff, "%u", config_get_coldWater() % 1000);
    }

    return buff;
}

static void webserver_requestAuthentication(httpd_req_t *req) {

    char *auth = "HTTP/1.1 401 Unauthorized\r\n\
Server: ESP32 " MODULE_NAME "\r\n\
Content-Type: text/html; charset=UTF-8\r\n\
WWW-Authenticate: Basic realm=\"Login Required\"\r\n\
Content-Length: 97\r\n\
Access-Control-Allow-Origin: *\r\n\
\r\n\
<a>You need use login and password.</a><br/><br/><a href=\"javascript:history.go(-1)\">Return</a>\r\n";

    httpd_send(req, auth, strlen(auth));
    return;
}

static bool webserver_authenticate(httpd_req_t *req) {

    const char *auth = "Authorization";
    const char *method = "Basic ";
    char *buf = NULL, *pos, *login, *password, *cfg_login, *cfg_password;
    char decode[MAX_WEBADMINLOGIN + MAX_WEBADMINPASSWORD + 2] = { 0 };
    size_t buf_len, olen;
    bool login_auth = false;

    buf_len = httpd_req_get_hdr_value_len(req, auth) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (!buf) {
            WM_LOGE(TAG, "Allocation memory error. (%s:%u)", __FILE__, __LINE__);
            return login_auth;
        }
        if (httpd_req_get_hdr_value_str(req, auth, buf, buf_len) == ESP_OK) {
            if (strncmp(buf, method, strlen(method)) == 0) {
                pos = buf + strlen(method);
                if (mbedtls_base64_decode((unsigned char*) &decode, sizeof(decode),
                        &olen, (unsigned char*) pos, strlen(pos)) == 0) {
                    password = strchr(decode, ':');
                    if (password) {
                        login = decode;
                        password[0] = 0;
                        password++;
                        cfg_login = config_get_webAdminLogin();
                        cfg_password = config_get_webAdminPassword();
                        if ((strcmp(login, cfg_login) == 0) & (strcmp(password, cfg_password) == 0)) {
                            ESP_LOGI(TAG, "Basic authenticate is true");
                            login_auth = true;
                        }
                    }
                }
            } else {
                ESP_LOGI(TAG, "Authenticated is not Basic");
            }
        }
        free(buf);
    }
    return login_auth;
}

static void reboot_task(void *pvParameter) {

    PRINT("Rebooting ...\n");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
    vTaskDelete(NULL);
}

static void webserver_parse_settings_uri(httpd_req_t *req) {
    char *p, *arg, *uri = NULL;
    const char sep[] = "?&";
    watermeter_config_t config;

    bool newSave = false;

    clearConfig(&config);

    uri = malloc(strlen(req->uri)+1);

    if (uri == NULL) {
        WM_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
        return;
    }

    strcpy(uri, req->uri);

    p = (char*) strtok(uri, sep);

    while (p) {
        arg = (char*) strstr(p, "login");
        if (arg) {
            arg += 6;
            strcpy(config.webAdminLogin, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "password");
        if (arg) {
            arg += 9;
            strcpy(config.webAdminPassword, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "fullsecurity");
        if (arg) {
            if (strstr(arg, "true")) {
                config.fullSecurity = true;
            }
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "configsecurity");
        if (arg) {
            if (strstr(arg, "true")) {
                config.configSecurity = true;
            }
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "wifi-mode");
        if (arg) {
            if (strstr(arg, "station")) config.apMode = false;
            if (strstr(arg, "ap")) config.apMode = true;
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "wifi-ap-name");
        if (arg) {
            arg += 13;
            strcpy(config.apSsid, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "wifi-ap-pass");
        if (arg) {
            arg += 13;
            strcpy(config.apPassword, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "wifi-sta-name");
        if (arg) {
            arg += 14;
            strcpy(config.staSsid, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "wifi-sta-pass");
        if (arg) {
            arg += 14;
            strcpy(config.staPassword, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "mqtt-user");
        if (arg) {
            arg += 10;
            strcpy(config.mqttUser, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "mqtt-pass");
        if (arg) {
            arg += 10;
            strcpy(config.mqttPassword, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "mqtt-broker");
        if (arg) {
            arg += 12;
            strcpy(config.mqttBroker, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "mqtt-topic");
        if (arg) {
            arg += 11;
            strcpy(config.mqttTopic, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "serv-ntp");
        if (arg) {
            arg += 9;
            strcpy(config.ntpServerName, arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "gmt-zone");
        if (arg) {
            arg += 9;
            config.timeZone = atoi(arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "hotw");
        if (arg) {
            arg += 5;
            config.hotTime = time(NULL);
            config.hotWater = strtoul(arg, 0, 10);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "coldw");
        if (arg) {
            arg += 6;
            config.coldTime = time(NULL);
            config.coldWater = strtoul(arg, 0, 10);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "countw");
        if (arg) {
            arg += 7;
            config.litersPerPulse = atoi(arg);
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "reboot");
        if (arg) {
            rebootNow = true;
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "defconfig");
        if (arg) {
            defaultConfig = true;
            p = (char*) strtok(NULL, sep);
            continue;
        }

        arg = (char*) strstr(p, "set-conf");
        if (arg) {
            if (strstr(arg, "set")) {
                saveNewConfig = true;
            }
            p = (char*) strtok(NULL, sep);
            continue;
        }

        p = (char*) strtok(NULL, sep);

    }

    if (defaultConfig) {
        defaultConfig = false;
        removeConfig();
        if (rebootNow) {
            xTaskCreate(&reboot_task, "reboot_task", 2048, NULL, 0, NULL);
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            for(;;);
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);

        firstStart = true;
        initDefConfig();

        startWiFiAP();
        free(uri);

        return;
    }

    if (saveNewConfig) {

        if (strcmp(config_get_webAdminLogin(), config.webAdminLogin) != 0
                || strcmp(config_get_webAdminPassword(), config.webAdminPassword) != 0
                || config_get_fullSecurity() != config.fullSecurity
                || config_get_configSecurity() != config.configSecurity)
            newSave = true;

        if (apModeNow || staApModeNow) {
            if (!config.apMode
                    || strcmp(config_get_apSsid(), config.apSsid) != 0
                    || strcmp(config_get_apPassword(), config.apPassword)
                            != 0) {
                restartWiFi = true;
                newSave = true;
            }
        }

        if (staModeNow || staApModeNow) {
            if (config.apMode
                    || strcmp(config_get_staSsid(), config.staSsid) != 0
                    || strcmp(config_get_staPassword(), config.staPassword)
                            != 0) {
                restartWiFi = true;
                staConfigure = true;
                newSave = true;
            }
        }

        if (strcmp(config_get_ntpServerName(), config.ntpServerName) != 0) {
            sntpReInit = true;
            newSave = true;
        }

        if (config_get_timeZone() != config.timeZone) {
            newSave = true;
        }

        if (strcmp(config_get_mqttBroker(), config.mqttBroker) != 0
                || strcmp(config_get_mqttUser(), config.mqttUser) != 0
                || strcmp(config_get_mqttPassword(), config.mqttPassword) != 0
                || strcmp(config_get_mqttTopic(), config.mqttTopic) != 0) {
            mqttRestart = true;
            newSave = true;
        }

        if (config_get_hotWater() != config.hotWater) {
            subsHotWater = true;
            newSave = true;
        }
        if (config_get_coldWater() != config.coldWater) {
            subsColdWater = true;
            newSave = true;
        }

        if (newSave) {
            setConfig(&config);
            saveConfig();
            firstStart = false;
        }

        if (restartWiFi) {
            restartWiFi = false;
            if (config_get_apMode() || (config_get_staSsid())[0] == '0') {
                staConfigure = false;
                startWiFiAP();
            } else {
                startWiFiSTA();
            }
        }

        if (mqttRestart) {
            mqttRestart = false;
            mqtt_restart();
        }

        if (sntpReInit) {
            sntpReInit = false;
            sntp_reinitialize();
        }

        saveNewConfig = false;
    }

    if (rebootNow) {
        xTaskCreate(&reboot_task, "reboot_task", 2048, NULL, 0, NULL);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        for(;;);
    }

    free(uri);
    return;
}

static esp_err_t webserver_read_file(httpd_req_t *req) {

    char *send_buff = NULL, *subst_token = NULL, token[32], buff[OTA_BUF_LEN + 1];
    size_t size;
    int i, pos_token, send_len = 0;

    sprintf(buff, "%s%s", webserver_html_path, req->uri);

    FILE *f = fopen(buff, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Cannot open file %s", buff);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
        return ESP_FAIL;
    }

    char *type = http_content_type(buff);
    httpd_resp_set_type(req, type);

    pos_token = -1;

    do {
        size = fread(buff, 1, sizeof(buff) - 1, f);
        if (size > 0) {
            send_len = 0;
            send_buff = buff;
            for (i = 0; i < size; i++) {
                if (pos_token == -1) {
                    if (buff[i] == '%') {
                        if (send_len != 0)
                            httpd_resp_send_chunk(req, send_buff, send_len);
                        send_len = 0;
                        pos_token = 0;
                    } else {
                        send_len++;
                    }
                } else {
                    if (buff[i] == '%') {
                        if (pos_token == 0) {
                            //This is the second % of a %% escape string.
                            //Send a single % and resume with the normal program flow.
                            httpd_resp_send_chunk(req, "%", 1);
                        } else {
                            //This is an actual token.
                            token[pos_token++] = 0; //zero-terminate token
                            // Call function check token
                            subst_token = webserver_subst_token_to_response(token);
                            if (strlen(subst_token)) {
                                httpd_resp_send_chunk(req, subst_token, strlen(subst_token));
                            }
                        }
                        //Go collect normal chars again.
                        send_buff = &buff[i + 1];
                        pos_token = -1;
                    } else {
                        if (pos_token < (sizeof(token) - 1))
                            token[pos_token++] = buff[i];
                    }
                }
            }
        }
        if (send_len != 0) {
            httpd_resp_send_chunk(req, send_buff, send_len);
        }
    } while (size == sizeof(buff) - 1);

    fclose(f);

    return ESP_OK;
}

static esp_err_t webserver_response(httpd_req_t *req) {

    ESP_LOGI(TAG, "Request URI \"%s\"", req->uri);

    if (strcmp(req->uri, ROOT) == 0) {
        strcpy((char*) req->uri, INDEX);
    }

    if (firstStart && strcmp(req->uri, INDEX) == 0)
        return webserver_redirect(req, CONFIG);

    if (config_get_fullSecurity() ||
            (config_get_configSecurity() && strcmp(req->uri, CONFIG) == 0) ||
            (config_get_configSecurity() && strcmp(req->uri, UPLOAD) == 0)) {
        if (!webserver_authenticate(req)) {
            webserver_requestAuthentication(req);
            return ESP_OK;
        }
    }

    if (strncmp(req->uri, SETTINGS, strlen(SETTINGS)) == 0) {
        webserver_parse_settings_uri(req);
//		strcpy((char*)req->uri, OK);
    }

    esp_err_t ret = webserver_read_file(req);

    httpd_resp_sendstr_chunk(req, NULL);

    return ret;
}

static esp_err_t webserver_update_cert_mqtt(httpd_req_t *req) {
    esp_err_t ret = ESP_FAIL;
    char *buff = NULL;
    char *err = NULL;
    const char *hdr_end = "\r\n\r\n";
    size_t len;

    if (req->content_len > MAX_BUFF_RW + 300) {
        err = "Certificate file too large";
    }

    if (!err) {
        buff = malloc(req->content_len + 1);
        if (!buff) {
            err = "Error allocation memory";
        } else {
            len = httpd_req_recv(req, buff, req->content_len);
            if (len != req->content_len) {
                err = "Eror upload certificate";
            } else {
                char *pos = strstr(buff, hdr_end) + strlen(hdr_end);
                if (!pos) {
                    err = "Invalid content type";
                } else {
                    strtrim(pos);
                    char *p = strstr(pos, CERT_END);
                    if (!p) {
                        err = "Uploaded file not sertificate";
                    } else {
                        p[strlen(CERT_END) + 1] = 0;
                        if (strlen(pos) >= MAX_BUFF_RW) {
                            err = "Certificate file too large";
                        } else if (!mqtt_new_cert_from_webserver(pos)) {
                            err = "Uploaded file not sertificate";
                        } else {
                            ret = ESP_OK;
                        }
                    }
                }
            }
        }
    }

    char err_buff[128];
    if (ret == ESP_OK && !err) {
        sprintf(err_buff,
                "<a>Success. New certificate MQTT is loaded</a><br/><br/><a href=\"javascript:history.go(-1)\">Return</a>");
        httpd_resp_sendstr(req, err_buff);
    } else {
        sprintf(err_buff,
                "<a>%s. Error code: 0x%x</a><br/><br/><a href=\"javascript:history.go(-1)\">Return</a>",
                err, ret);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_buff);
    }

    if (buff) free(buff);

    return ESP_OK;
}

static char* filename_from_disp(const char *str) {
    static char filename[256];
    const char *token = "filename=";
    char *pos;

    memset(filename, 0, 256);

    pos = strstr(str, token);

    if (!pos) {
        return NULL;
    }

    pos += strlen(token) + 1;

    for (int i = 0; *pos != '"'; i++, pos++) {
        if (*pos != '"') {
            filename[i] = *pos;
        }

    }

    return filename;
}

static esp_err_t webserver_upload_html(httpd_req_t *req) {

    FILE *fp = NULL;

    esp_err_t ret = ESP_OK;

    size_t global_cont_len;
    size_t len;
    size_t recorded_len = 0;

    char buf[OTA_BUF_LEN];
    char *tmpname = NULL;
    char *newname = NULL;
    char *filename = NULL;
    char *header = NULL;
    char *boundary = NULL;
    const char *hdr_end = "\r\n\r\n";
    char *err = NULL;
    char *pos;

    dontSleep = true;

    global_cont_len = req->content_len;

    bool begin = true;

    if (!spiffs) {
        err = "Spiffs not mount";
        ret = ESP_FAIL;
    }

    if (get_fs_free_space() < req->content_len) {
        err = "Upload file too large";
        ret = ESP_FAIL;
    }

    len = httpd_req_get_hdr_value_len(req, "Content-Type") + 1;

    if (len > 1) {
        header = malloc(len);
        if (!header) {
            WM_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
            err = "Error allocation memory.";
            ret = ESP_FAIL;
        } else {
            ret = httpd_req_get_hdr_value_str(req, "Content-Type", header, len);

            if (ret == ESP_OK) {
                pos = strstr(header, "boundary=");
                if (pos) {
                    boundary = pos + strlen("boundary=");
                } else {
                    err = "Invalid content type";
                    ret = ESP_FAIL;
                }
            } else {
                err = "Invalid content type";
                ret = ESP_FAIL;
            }

        }
    }

    if (!err) {

        while (global_cont_len) {
            len = httpd_req_recv(req, buf, OTA_BUF_LEN);
            if (global_cont_len > OTA_BUF_LEN || begin) {
                if (begin) {
                    begin = false;
                    pos = strstr(buf, "Content-Disposition:");
                    if (!pos) {
                        err = "Invalid content type";
                        ret = ESP_FAIL;
                        break;
                    }

                    filename = filename_from_disp(pos);

                    if (!filename) {
                        err = "Invalid file name";
                        ret = ESP_FAIL;
                        break;
                    }

                    tmpname = malloc(strlen(HTML_PATH) + 1 + strlen(filename) + 5);

                    if (!tmpname) {
                        WM_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
                        err = "Error allocation memory.";
                        ret = ESP_FAIL;
                        break;

                    }

                    newname = malloc(strlen(HTML_PATH) + 1 + strlen(filename) + 1);
                    if (!newname) {
                        WM_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
                        err = "Error allocation memory.";
                        ret = ESP_FAIL;
                        break;

                    }

                    sprintf(newname, "%s%s%s", HTML_PATH, DELIM, filename);

                    sprintf(tmpname, "%s%s", newname, ".tmp");

                    fp = fopen(tmpname, "wb");

                    if (!fp) {
                        WM_LOGE(TAG, "Cannot open file \"%s\" (%s:%u)", tmpname, __FILE__, __LINE__);
                        err = "Cannot open file";
                        ret = ESP_FAIL;
                        break;
                    }

                    fseek(fp, 0, SEEK_SET);

                    pos = strstr(buf, hdr_end);
                    if (!pos) {
                        err = "Invalid content type";
                        ret = ESP_FAIL;
                        break;
                    }
                    pos += strlen(hdr_end);

                    PRINT("Loading \"%s\" file\n", filename);
                    PRINT("Please wait");
                    vTaskDelay(1000 / portTICK_PERIOD_MS);

                    char *p = strstr(pos, boundary);
                    if (p) {
                        for (; *p == '-'; p--);
                        *(p - 1) = 0;
                        size_t l = strlen(pos);
                        if (fwrite(pos, sizeof(uint8_t), l, fp) != l) {
                            err = "Error wtite file";
                            ret = ESP_FAIL;
                            break;
                        }
                        recorded_len += l;

                    } else {
                        size_t plen = len - (pos - buf);
                        if (fwrite(pos, sizeof(uint8_t), plen, fp) != plen) {
                            err = "Error wtite file";
                            ret = ESP_FAIL;
                            break;
                        }
                        recorded_len += plen;
                    }

                } else {
                    pos = strstr(buf, boundary);
                    if (pos) {
                        for (; *pos == '-'; pos--);
                        *(pos - 1) = 0;
                        size_t l = strlen(buf);
                        if (fwrite(buf, sizeof(uint8_t), l, fp) != l) {
                            err = "Error wtite file";
                            ret = ESP_FAIL;
                            break;
                        }
                        recorded_len += l;
                    } else {
                        if (fwrite(buf, sizeof(uint8_t), len, fp) != len) {
                            err = "Error wtite file";
                            ret = ESP_FAIL;
                            break;
                        }
                        recorded_len += len;
                    }
                }
                global_cont_len -= len;
            } else {
                if (global_cont_len > 0) {
                    pos = strstr(buf, boundary);
                    if (pos) {
                        for (; *pos == '-'; pos--);
                        *(pos - 1) = 0;
                        size_t l = strlen(buf);
                        if (fwrite(buf, sizeof(uint8_t), l, fp) != l) {
                            err = "Error wtite file";
                            ret = ESP_FAIL;
                            break;
                        }
                        recorded_len += l;
                    } else {
                        if (fwrite(buf, sizeof(uint8_t), len, fp) != len) {
                            err = "Error wtite file";
                            ret = ESP_FAIL;
                            break;
                        }
                        recorded_len += len;
                    }
                    global_cont_len -= len;
                }
            }
            printf(".");
            fflush(stdout);
        }

        printf("\n");
        PRINT("File transferred finished: %d bytes\n", recorded_len);

        fclose(fp);
    }

    if (ret == ESP_OK && !err) {
        sprintf(buf, "<a>Success. File uploaded.</a><br/><br/><a href=\"javascript:history.go(-1)\">Return</a>");
        httpd_resp_sendstr(req, buf);

        struct stat st;
        if (stat(newname, &st) == 0) {
            unlink(newname);
        }

        if (rename(tmpname, newname) != 0) {
            WM_LOGE(TAG, "File rename \"%s\" to \"%s\" failed. (%s:%u)", tmpname, newname, __FILE__, __LINE__);
        }

//      DIR *dir;
//      struct dirent *de;
//
//      dir = opendir(HTML_PATH);
//
//
//      if (!dir) {
//          WM_LOGE(TAG, "open \"%s\"directory failed",  HTML_PATH);
//          return ESP_OK;
//      }
//
//      while ((de = readdir(dir))) {
//          printf("%s\n", de->d_name);
//      }
//      closedir(dir);

    } else {
        sprintf(buf, "Failure. Error code: 0x%x\r\n", ret);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, buf);
    }

    dontSleep = false;

    if (tmpname) free(tmpname);
    if (newname) free(newname);
    if (header)  free(header);

    return ESP_OK;
}

static esp_err_t webserver_update(httpd_req_t *req) {

    const esp_partition_t *partition;
    esp_ota_handle_t ota_handle;
    esp_err_t ret = ESP_OK;

    size_t global_cont_len;
    size_t len;
    size_t global_recv_len = 0;

    char buf[OTA_BUF_LEN];
    const char *hdr_end = "\r\n\r\n";
    char *err = NULL;

    dontSleep = true;

    global_cont_len = req->content_len;

//    ESP_LOGI(TAG, "Content length: %d", global_cont_len);

    partition = esp_ota_get_next_update_partition(NULL);

    bool begin = true;

    if (partition) {

        if (partition->size < req->content_len) {
            err = "Firmware image too large";
            ret = ESP_FAIL;
//			PRINT("part_size: %d, cont_len: %d\n", partition->size, req->content_len);
        }

        if (!err) {

            ret = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &ota_handle);
            if (ret == ESP_OK) {
                while (global_cont_len) {
                    len = httpd_req_recv(req, buf, OTA_BUF_LEN);
                    if (global_cont_len > OTA_BUF_LEN || begin) {
                        if (begin) {
                            begin = false;
                            char *pos = strstr(buf, hdr_end) + strlen(hdr_end);
                            if (!pos) {
                                err = "Invalid content type";
                                ret = ESP_FAIL;
                                break;
                            }

                            PRINT("Writing to partition name \"%s\" subtype %d at offset 0x%x\n",
                                  partition->label, partition->subtype, partition->address);
                            PRINT("Please wait");
                            vTaskDelay(1000 / portTICK_PERIOD_MS);

                            size_t plen = len - (pos - buf);
                            ret = esp_ota_write(ota_handle, (void*) pos, plen);
                            if (ret != ESP_OK) {
                                err = "esp_ota_write return error";
                                break;
                            }
                            global_recv_len += plen;
                        } else {
                            ret = esp_ota_write(ota_handle, (void*) buf, len);
                            if (ret != ESP_OK) {
                                err = "esp_ota_write return error";
                                break;
                            }
                            global_recv_len += len;
                        }
                        global_cont_len -= len;
                    } else {
                        if (global_cont_len > 0) {
                            ret = esp_ota_write(ota_handle, (void*) buf, len);
                            if (ret != ESP_OK) {
                                err = "esp_ota_write return error";
                                break;
                            }
                            global_recv_len += len;
                            global_cont_len -= len;
                        }
                    }
                    printf(".");
                    fflush(stdout);
                }

                printf("\n");
                PRINT("Binary transferred finished: %d bytes\n", global_recv_len);

                ret = esp_ota_end(ota_handle);
                if (ret != ESP_OK) {
                    err = "esp_ota_end return error";
                }
            }
        }
    } else {
        err = "No partiton";
        ret = ESP_FAIL;
    }

    if (!err) {
        ret = esp_ota_set_boot_partition(partition);
    } else {
        sprintf(buf, "%s 0x%x", err, ret);
        WM_LOGE(TAG, "%s. (%s:%u)", buf, __FILE__, __LINE__);
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK && !err) {
        sprintf(buf,
                "<a>Success. Next boot partition is %s. Restart system...</a><br/><br/>"
                "<a href=\"javascript:history.go(-1)\">Return</a>",
                partition->label);
        httpd_resp_sendstr(req, buf);
    } else {
        sprintf(buf, "Failure. Error code: 0x%x\r\n", ret);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, buf);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    if (ret == ESP_OK && !err) {

        mqtt_stop();
        esp_wifi_disconnect();

        const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
        PRINT("Next boot partition \"%s\" name subtype %d at offset 0x%x\n",
              boot_partition->label, boot_partition->subtype, boot_partition->address);
        PRINT("Prepare to restart system!\n--\n--\n\n");

        vTaskDelay(2000 / portTICK_PERIOD_MS);

        esp_restart();
    }

    dontSleep = false;

    return ESP_OK;
}

void send_str_log(httpd_req_t *req, char *str) {

    char lt = '<';
    char gt = '>';
    char key[] = "<>";
    char *pos, *pstr = str;

    pos = strpbrk(pstr, key);

    if (pos) {
    	while(pos) {
            if (pos-pstr != 1) httpd_resp_send_chunk(req, pstr, strlen(pstr)-strlen(pos));
            if (*pos == lt) {
                httpd_resp_send_chunk(req, "&lt", 3);
            } else if (*pos == gt) {
                httpd_resp_send_chunk(req, "&gt", 3);
            }
            pstr = pos+1;
            pos = strpbrk(pstr, key);
    	}
    }

    if (strlen(pstr)) {
    	httpd_resp_send_chunk(req, pstr, strlen(pstr));
    }
}


static esp_err_t webserver_log(httpd_req_t *req) {

    esp_err_t ret = ESP_FAIL;
    const char *end_doc = "</body></html>";
    const char *end_color = "</font>";
    const char *enter = "<br/>\n";

    ESP_LOGI(TAG, "Request URI \"%s\"", req->uri);

    if (config_get_fullSecurity() || config_get_configSecurity()) {
        if (!webserver_authenticate(req)) {
            webserver_requestAuthentication(req);
            return ESP_OK;
        }
    }

    lstack_elem_t *lstack = get_lstack();

    ret = webserver_read_file(req);

    if (ret == ESP_FAIL) {
        return ret;
    }

    if (!lstack) {
        const char resp[] = "Not log file<br/>\n";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    } else {
        bool color;
        while (lstack) {
            color = false;
            if (lstack->str) {
                switch (*(lstack->str)) {
                case 'E':
                    if (lstack->str[1] == ' ') {
                        color = true;
                        httpd_resp_send_chunk(req, COLOR_E, HTTPD_RESP_USE_STRLEN);
                    }
                    break;
                case 'W':
                    if (lstack->str[1] == ' ') {
                        color = true;
                        httpd_resp_send_chunk(req, COLOR_W, HTTPD_RESP_USE_STRLEN);
                    }
                    break;
                case 'I':
                    if (lstack->str[1] == ' ') {
                        color = true;
                        httpd_resp_send_chunk(req, COLOR_I, HTTPD_RESP_USE_STRLEN);
                    }
                    break;
                case 'D':
                    if (lstack->str[1] == ' ') {
                        color = true;
                        httpd_resp_send_chunk(req, COLOR_D, HTTPD_RESP_USE_STRLEN);
                    }
                    break;
                case 'V':
                    if (lstack->str[1] == ' ') {
                        color = true;
                        httpd_resp_send_chunk(req, COLOR_V, HTTPD_RESP_USE_STRLEN);
                    }
                    break;
                default:
                    break;
                }

                send_str_log(req, lstack->str);
//                httpd_resp_send_chunk(req, lstack->str, HTTPD_RESP_USE_STRLEN);
                if (color) {
                    httpd_resp_send_chunk(req, end_color, HTTPD_RESP_USE_STRLEN);
                }
                httpd_resp_send_chunk(req, enter, HTTPD_RESP_USE_STRLEN);
            }

            lstack = lstack->next;
        }
        httpd_resp_send_chunk(req, "<br/><a href=\"javascript:history.go(-1)\">Return</a>", HTTPD_RESP_USE_STRLEN);
        httpd_resp_send_chunk(req, end_doc, HTTPD_RESP_USE_STRLEN);
        httpd_resp_sendstr_chunk(req, NULL);
    }

    return ESP_OK;
}

void webserver_restart() {
    restartWebServer = true;
}

static httpd_handle_t webserver_start(void) {

    httpd_handle_t server = NULL;
    esp_err_t ret = ESP_FAIL;

    httpd_ssl_config_t https_config = HTTPD_SSL_CONFIG_DEFAULT();

    https_config.httpd.uri_match_fn = httpd_uri_match_wildcard;

    PRINT("Starting webserver\n");

    if (cacert) {
        https_config.cacert_pem = (uint8_t*) cacert;
        https_config.cacert_len = strlen(cacert) + 1;
    }

    if (prvtkey) {
        https_config.prvtkey_pem = (uint8_t*) prvtkey;
        https_config.prvtkey_len = strlen(prvtkey) + 1;
    }

    if (httpd_ssl_start(&server, &https_config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        ret = httpd_register_uri_handler(server, &update_html);
        if (ret != ESP_OK) WM_LOGE(TAG, "URL \"%s\" not registered", update_html.uri);
        ret = httpd_register_uri_handler(server, &uploadhtml_html);
        if (ret != ESP_OK) WM_LOGE(TAG, "URL \"%s\" not registered", uploadhtml_html.uri);
        ret = httpd_register_uri_handler(server, &log_html);
        if (ret != ESP_OK) WM_LOGE(TAG, "URL \"%s\" not registered", log_html.uri);
#if MQTT_SSL_ENABLE
        httpd_register_uri_handler(server, &updatecertmqtt_html);
        if (ret != ESP_OK) WM_LOGE(TAG, "URL \"%s\" not registered", updatecertmqtt_html.uri);
#endif
        ret = httpd_register_uri_handler(server, &uri_html);
        if (ret != ESP_OK) WM_LOGE(TAG, "URL \"%s\" not registered", uri_html.uri);
        return server;
    }

    WM_LOGE(TAG, "Error starting server! (%s:%u)", __FILE__, __LINE__);
    return NULL;
}

static void webserver_stop(httpd_handle_t server) {
    // Stop the httpd server
    httpd_ssl_stop(server);
}

static void webserver_disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    httpd_handle_t *server = (httpd_handle_t*) arg;
    if (*server && !staApModeNow) {
        PRINT("Stopping webserver\n");
        webserver_stop(*server);
        *server = NULL;
    }
}

static void webserver_connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    httpd_handle_t *server = (httpd_handle_t*) arg;
    if (*server == NULL) {
//        ESP_LOGI(TAG, "Starting webserver");
        *server = webserver_start();
    }
}

static void http_check_task(void *pvparameters) {

    static uint8_t count = 0;

    httpd_handle_t *server = (httpd_handle_t*) pvparameters;

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    for (;;) {
        if (staApModeNow || staModeNow || apModeNow) {
            if (restartWebServer) {
                PRINT("Restart webserver\n");
                if (*server) {
                    webserver_stop(*server);
                    *server = NULL;
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                }
                restartWebServer = false;
                *server = webserver_start();
            }
            if (*server == NULL) {
                count++;
                if (count == 20) {
                    *server = webserver_start();
                    count = 0;
                }
            } else {
                count = 0;
            }
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void webserver_init(const char *html_path) {

    static httpd_handle_t server = NULL;

    PRINT("Initializing webserver. Please use prefix https://...\n");

    webserver_html_path = malloc(strlen(html_path) + 1);

    if (webserver_html_path == NULL) {
        WM_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
        return;
    }

    strcpy(webserver_html_path, html_path);

    cacert = read_file(CACERT_NAME);
    prvtkey = read_file(PRVTKEY_NAME);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &webserver_connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &webserver_connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &webserver_disconnect_handler, &server));

    xTaskCreate(&http_check_task, "http_check_task", 2048, &server, 0, NULL);
}
