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
                EMBED_TXTFILES "certs/cacert.pem"
                               "certs/prvtkey.pem"
                INCLUDE_DIRS "." "include")
                               
spiffs_create_partition_image(storage ../storage FLASH_IN_PROJECT)
