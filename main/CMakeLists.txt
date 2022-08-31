cmake_minimum_required(VERSION 3.19)
idf_component_register(SRCS "app_main.c" "sound.c" "wifi.c"
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
