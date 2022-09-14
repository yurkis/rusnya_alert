cmake_minimum_required(VERSION 3.19)
idf_component_register(SRCS "app_main.c" "sound.c" "wifi.c" "alert_api.c" "sntp_time.c" "version.h" "ota.c" "ota_https_ssl.h"
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

add_custom_target(ver_target ALL
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/version.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/version.h"
    COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/../embed_txt.py ${CMAKE_CURRENT_SOURCE_DIR}/../version.txt APP_VERSION_STR "${CMAKE_CURRENT_SOURCE_DIR}/version.h"
)

add_custom_target(ota_https_pem_target ALL
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/ota_https_ssl.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/ota_https_ssl.h"
    COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/../embed_txt.py ${CMAKE_CURRENT_SOURCE_DIR}/../ota_https.pem OTA_HTTPS_CERT "${CMAKE_CURRENT_SOURCE_DIR}/ota_https_ssl.h"
)

