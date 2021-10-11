#ifndef MAIN_INCLUDE_WM_FILESYS_H_
#define MAIN_INCLUDE_WM_FILESYS_H_

/* Defined mount point for file system */
#define MOUNT_POINT_SPIFFS  "/spiffs"
#define MOUNT_POINT_SDCARD  "/sdcard"
#define DELIM               "/"

/* Certs file name */
#define CERTS_PATH          MOUNT_POINT_SPIFFS DELIM "certs" DELIM
#define CACERT_NAME         CERTS_PATH "cacert.pem"         /* for https server */
#define PRVTKEY_NAME        CERTS_PATH "prvtkey.pem"        /* for https server */
#define MQTTCERT_NAME       CERTS_PATH "mqttcert.pem"       /* for mqtts client */
#define CERT_START          "-----BEGIN CERTIFICATE-----"
#define CERT_END            "-----END CERTIFICATE-----"

#define MAX_BUFF_RW 2048

#define SPI_DMA_CHAN    1
#define PIN_NUM_MISO    19
#define PIN_NUM_MOSI    23
#define PIN_NUM_CLK     18
#define PIN_NUM_CS      4

void init_spiffs();
void init_sdcard();
size_t get_fs_free_space();

#endif /* MAIN_INCLUDE_WM_FILESYS_H_ */
