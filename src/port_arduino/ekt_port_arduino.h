/*
 * ekt_port_arduino.h - Pin/peripheral configuration for the Arduino port.
 *
 * Adjust the defines below to match your board. Nothing in the toolkit core
 * needs to change.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef EKT_PORT_ARDUINO_H
#define EKT_PORT_ARDUINO_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- DHT11 ---- */
#ifndef EKT_ARDUINO_DHT_PIN
#define EKT_ARDUINO_DHT_PIN 2
#endif

/* ---- 74HC595 shift register ---- */
#ifndef EKT_ARDUINO_SR_CLK_PIN
#define EKT_ARDUINO_SR_CLK_PIN 3
#endif
#ifndef EKT_ARDUINO_SR_DATA_PIN
#define EKT_ARDUINO_SR_DATA_PIN 4
#endif
#ifndef EKT_ARDUINO_SR_LATCH_PIN
#define EKT_ARDUINO_SR_LATCH_PIN 5
#endif

#ifdef __cplusplus
}
#endif

#endif /* EKT_PORT_ARDUINO_H */
