# ============================================================
#  embedded-iot-toolkit - ESP-IDF component registration
#
#  The core is pure C99 with no vendor headers, so it compiles
#  anywhere. The ESP32 port is the platform implementation of
#  the four ekt_port.h hooks, so it is built as part of this
#  component when the target is an ESP32.
# ============================================================

set(srcs
    "src/ekt_soft_timer.c"
    "src/ekt_frame.c"
    "src/ekt_can_codec.c"
    "src/ekt_sensor.c")

set(public_requires "")

if(IDF_TARGET MATCHES "^esp32")
    list(APPEND srcs "port/esp32/ekt_port_esp32.c")
    set(public_requires "esp_driver_gpio" "esp_timer" "esp_rom" "freertos")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "src"
                       REQUIRES ${public_requires})
