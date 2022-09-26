cmake_minimum_required(VERSION 3.19)
idf_component_register(SRCS "main.c" "app_main.c" "sound.c" "wifi.c" "alert_api.c" "sntp_time.c" "version.h" "ota.c" "ota_https_ssl.h" "led_pattern.c" "settings.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "res/alarm.wav" "res/connected.wav" "res/disconnected.wav" "res/safe.wav")

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

set_property(DIRECTORY "${COMPONENT_DIR}" APPEND PROPERTY
     ADDITIONAL_MAKE_CLEAN_FILES version.h ota_https_ssl.h)

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
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../version.txt ${CMAKE_CURRENT_SOURCE_DIR}/../embed_txt.py
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
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../ota_https.pem ${CMAKE_CURRENT_SOURCE_DIR}/../embed_txt.py
)

add_custom_target(spiffs_ver ALL
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs/version.txt
    VERBATIM
)

add_dependencies(${COMPONENT_LIB} spiffs_ver)

add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs/version.txt
    COMMAND
        cp ${CMAKE_CURRENT_SOURCE_DIR}/../version.txt ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs/
    DEPENDS  ${CMAKE_CURRENT_SOURCE_DIR}/../version.txt
)

