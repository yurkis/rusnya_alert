cmake_minimum_required(VERSION 3.19)
idf_component_register(SRCS "app_main.c" "sound.c" "wifi.c" "alert_api.c" "sntp_time.c" "version.h"
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

add_custom_target(ver_target
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/version.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/version.h"
    COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/../gen_version.sh "${CMAKE_CURRENT_SOURCE_DIR}/version.h"
)

