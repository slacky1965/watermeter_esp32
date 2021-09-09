#ifndef MAIN_INCLUDE_WM_FILESYS_H_
#define MAIN_INCLUDE_WM_FILESYS_H_

/* Defined mount point for file system */
#define MOUNT_POINT_SPIFFS "/spiffs"
#define MOUNT_POINT_SDCARD "/sdcard"

#define DELIM "/"

#define SPI_DMA_CHAN 1
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   4

void init_spiffs();
void init_sdcard();

#endif /* MAIN_INCLUDE_WM_FILESYS_H_ */
