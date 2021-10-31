#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include <esp_https_server.h>
#include "esp_http_server.h"
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

/* Defined upload path */
#define PATH_HTML   "/html/"
#define PATH_CERTS  "/certs/"
#define PATH_IMAGE  "/image/"
#define PATH_UPLOAD "/upload/"

/* Legal URL web server */
#define	URL 		"/*"
#define ROOT        "/"
#define INDEX       "/index.html"
#define CONFIG      "/config.html"
#define SETTINGS    "/settings.html"
#define LOG         "/log.html"
#define UPLOAD      "/upload/*"

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
static esp_err_t webserver_upload(httpd_req_t *req);
static esp_err_t webserver_log(httpd_req_t *req);

static const httpd_uri_t uri_html = {
        .uri = URL,
        .method = HTTP_GET,
        .handler = webserver_response };

static const httpd_uri_t upload_html = {
        .uri = UPLOAD,
        .method = HTTP_POST,
        .handler = webserver_upload };

static const httpd_uri_t log_html = {
        .uri = LOG,
        .method = HTTP_GET,
        .handler = webserver_log };

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
        strcpy(buff, localDateTimeStr());
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

    char *buff = NULL;

    dontSleep = true;

    if (!spiffs) {
        dontSleep = false;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Spiffs not mount");
        return ESP_FAIL;
    }

    if (req->content_len > MAX_BUFF_RW*2) {
        dontSleep = false;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Certificate file too large");
        return ESP_FAIL;
    }

    buff = malloc(req->content_len + 1);
    if (!buff) {
        dontSleep = false;
        WM_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error allocation memory.");
        return ESP_FAIL;
    }

    memset(buff, 0, sizeof(req->content_len + 1));

    size_t len = httpd_req_recv(req, buff, req->content_len);
    if (len != req->content_len) {
        dontSleep = false;
        free(buff);
        WM_LOGE(TAG, "Error upload certificate. (%s:%u)", __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error upload certificate.");
        return ESP_FAIL;
    }

    if (!mqtt_new_cert_from_webserver(buff)) {
        dontSleep = false;
        free(buff);
        WM_LOGE(TAG, "Uploaded file not sertificate. (%s:%u)", __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Uploaded file not sertificate.");
        return ESP_FAIL;
    }

    dontSleep = false;
    free(buff);

    httpd_resp_sendstr(req, "<a>Success. New certificate MQTT is loaded</a><br/><br/><a href=\"/upload.html\">Return</a>");

    return ESP_OK;
}



static esp_err_t webserver_upload_html(httpd_req_t *req, const char *full_name) {

    FILE *fp = NULL;
    size_t global_cont_len, recorded_len = 0;
    int received;
    char *tmpname, *newname;
    char buf[MAX_BUFF_RW];

    dontSleep = true;

    global_cont_len = req->content_len;

    if (!spiffs) {
        dontSleep = false;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Spiffs not mount");
        return ESP_FAIL;
    }

    if (get_fs_free_space() < req->content_len) {
        dontSleep = false;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload file too large");
        return ESP_FAIL;
    }

    tmpname = malloc(strlen(MOUNT_POINT_SPIFFS) + 1 + strlen(full_name) + 5);

    if (!tmpname) {
        dontSleep = false;
        WM_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error allocation memory.");
        return ESP_FAIL;
    }

    newname = malloc(strlen(MOUNT_POINT_SPIFFS) + 1 + strlen(full_name) + 1);
    if (!newname) {
        dontSleep = false;
        free(tmpname);
        WM_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error allocation memory.");
        return ESP_FAIL;
    }

    sprintf(newname, "%s%s", MOUNT_POINT_SPIFFS, full_name);

    sprintf(tmpname, "%s%s", newname, ".tmp");

    fp = fopen(tmpname, "wb");

    if (!fp) {
        dontSleep = false;
        WM_LOGE(TAG, "Failed to create file \"%s\" (%s:%u)", tmpname, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    fseek(fp, 0, SEEK_SET);

    PRINT("Loading \"%s\" file\n", full_name);
    PRINT("Please wait");

    while(global_cont_len) {
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(global_cont_len, MAX_BUFF_RW))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fp);
            unlink(tmpname);

            WM_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            free(newname);
            free(tmpname);
            dontSleep = false;
            return ESP_FAIL;
        }

        recorded_len += received;

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fp))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fp);
            unlink(tmpname);
            free(newname);
            free(tmpname);
            dontSleep = false;

            ESP_LOGE(TAG, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        global_cont_len -= received;

        printf(".");
        fflush(stdout);
    }

    PRINT("\n");
    PRINT("File transferred finished: %d bytes\n", recorded_len);

    fclose(fp);

    sprintf(buf, "<a>Success. File uploaded.</a><br/><br/><a href=\"/upload.html\">Return</a>");
    httpd_resp_sendstr(req, buf);

    struct stat st;
    if (stat(newname, &st) == 0) {
        unlink(newname);
    }

    if (rename(tmpname, newname) != 0) {
        WM_LOGE(TAG, "File rename \"%s\" to \"%s\" failed. (%s:%u)", tmpname, newname, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to rename file");
        free(newname);
        free(tmpname);
        return ESP_FAIL;
    }

    free(tmpname);
    free(newname);

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
    char *err;

    dontSleep = true;

    global_cont_len = req->content_len;

    partition = esp_ota_get_next_update_partition(NULL);

    if (partition) {
        if (partition->size < req->content_len) {
            err = "Firmware image too large";
            WM_LOGE(TAG, err);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            dontSleep = false;
            return ESP_FAIL;
        }

        ret = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &ota_handle);
        if (ret == ESP_OK) {
            bool begin = true;
            while(global_cont_len) {
                len = httpd_req_recv(req, buf, MIN(global_cont_len, OTA_BUF_LEN));
                if (len) {
                    if (begin) {
                        begin = false;
                        if (buf[0] != 233) {
                            err = "Invalid flash image type";
                            WM_LOGE(TAG, err);
                            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
                            dontSleep = false;
                            return ESP_FAIL;
                        }
                        PRINT("Writing to partition name \"%s\" subtype %d at offset 0x%x\n",
                              partition->label, partition->subtype, partition->address);
                        PRINT("Please wait");
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                    }
                    ret = esp_ota_write(ota_handle, (void*)buf, len);
                    if (ret != ESP_OK) {
                        err = "esp_ota_write() return error";
                        WM_LOGE(TAG, err);
                        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
                        dontSleep = false;
                        return ESP_FAIL;
                    }
                    global_recv_len += len;
                    global_cont_len -= len;
                    printf(".");
                    fflush(stdout);
                }
            }
            PRINT("\n");
            PRINT("Binary transferred finished: %d bytes\n", global_recv_len);

            ret = esp_ota_end(ota_handle);
            if (ret != ESP_OK) {
                err = "esp_ota_end() return error";
                WM_LOGE(TAG, err);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
                dontSleep = false;
                return ESP_FAIL;
            }
            ret = esp_ota_set_boot_partition(partition);
            if (ret != ESP_OK) {
                err = "Set boot partition is error";
                WM_LOGE(TAG, err);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
                dontSleep = false;
                return ESP_FAIL;
            }
            sprintf(buf,
                    "<a>Success. Next boot partition is %s. Restart system...</a><br/><br/><a href=\"javascript:history.go(-1)\">Return</a>",
                    partition->label);
            httpd_resp_sendstr(req, buf);

            mqtt_stop();
            esp_wifi_disconnect();

            const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
            PRINT("Next boot partition \"%s\" name subtype %d at offset 0x%x\n",
                  boot_partition->label, boot_partition->subtype, boot_partition->address);
            PRINT("Prepare to restart system!\n\n\n\n");

            vTaskDelay(2000 / portTICK_PERIOD_MS);

            esp_restart();

        } else {
            err = "Partition error";
            WM_LOGE(TAG, err);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            dontSleep = false;
            return ESP_FAIL;

        }


    } else {
        err = "No partiton";
        WM_LOGE(TAG, err);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        dontSleep = false;
        return ESP_FAIL;
    }

    dontSleep = false;

    return ESP_OK;
}


static esp_err_t webserver_upload(httpd_req_t *req) {

    const char *full_path;

    full_path = req->uri+strlen(PATH_UPLOAD)-1;

    if (strncmp(full_path, PATH_HTML, strlen(PATH_HTML)) == 0) {
        if (strlen(full_path+strlen(PATH_HTML)) >= CONFIG_FATFS_MAX_LFN) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
            return ESP_FAIL;
        }

        return webserver_upload_html(req, full_path);

    } else if (strncmp(full_path, PATH_CERTS, strlen(PATH_CERTS)) == 0) {
        if (strlen(full_path+strlen(PATH_CERTS)) >= CONFIG_FATFS_MAX_LFN) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
            return ESP_FAIL;
        }

        return webserver_update_cert_mqtt(req);

    } else if (strncmp(full_path, PATH_IMAGE, strlen(PATH_IMAGE)) == 0) {
        if (strlen(full_path+strlen(PATH_IMAGE)) >= CONFIG_FATFS_MAX_LFN) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
            return ESP_FAIL;
        }

        return webserver_update(req);

    } else {
        WM_LOGE(TAG, "Invalid path: %s", req->uri);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
        return ESP_FAIL;
    }


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
            if (pos-pstr > 0) httpd_resp_send_chunk(req, pstr, strlen(pstr)-strlen(pos));
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
        ret = httpd_register_uri_handler(server, &upload_html);
        if (ret != ESP_OK) WM_LOGE(TAG, "URL \"%s\" not registered", upload_html.uri);
        ret = httpd_register_uri_handler(server, &log_html);
        if (ret != ESP_OK) WM_LOGE(TAG, "URL \"%s\" not registered", log_html.uri);
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
