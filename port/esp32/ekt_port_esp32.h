/*
 * ekt_port_esp32.h - Pin/peripheral configuration for the ESP32 ESP-IDF port.
 *
 * Adjust the defines below to match your board. Nothing in the toolkit core
 * needs to change.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef EKT_PORT_ESP32_H
#define EKT_PORT_ESP32_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- DHT11 ---- */
#ifndef EKT_ESP32_DHT_GPIO
#define EKT_ESP32_DHT_GPIO 4
#endif

/* ---- 74HC595 shift register ---- */
#ifndef EKT_ESP32_SR_CLK_GPIO
#define EKT_ESP32_SR_CLK_GPIO 5
#endif
#ifndef EKT_ESP32_SR_DATA_GPIO
#define EKT_ESP32_SR_DATA_GPIO 6
#endif
#ifndef EKT_ESP32_SR_LATCH_GPIO
#define EKT_ESP32_SR_LATCH_GPIO 7
#endif

#ifdef __cplusplus
}
#endif

#endif /* EKT_PORT_ESP32_H */
