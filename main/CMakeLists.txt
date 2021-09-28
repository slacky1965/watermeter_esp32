# Edit following two lines to set component requirements (see docs)
idf_component_register(SRCS "main.c"
							"wm_utils.c"
							"wm_config.c"
							"wm_wifi.c"
							"wm_httpd.c"
							"wm_time.c"
							"wm_mqtt.c"
							"wm_pulse.c"
							"wm_filesys.c"
							"wm_log.c"
                INCLUDE_DIRS "." "include")

# you can comment after the first flash                               
spiffs_create_partition_image(storage ../storage FLASH_IN_PROJECT)

