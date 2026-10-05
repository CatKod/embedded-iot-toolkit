/*
 * ekt_port.h - Platform abstraction the library depends on.
 *
 * The core of this toolkit is pure C with no vendor headers, so it compiles and
 * unit-tests on a PC. Anything that genuinely needs hardware is declared here
 * and implemented once per platform:
 *
 *   port/stm32/   STM32 HAL-based implementations
 *   port/esp32/   ESP-IDF-based implementations
 *   port/posix/   Host stubs used by the test suite
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef EKT_PORT_H
#define EKT_PORT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Perform a full DHT11 start-pulse + 40-bit read on `pin`.
 *
 * Must be bounded by a timeout and return false rather than block forever.
 *
 * @param pin  Platform GPIO number.
 * @param out  Receives the 5 raw bytes (humidity hi/lo, temp hi/lo, checksum).
 */
bool ekt_port_dht11_exchange(uint8_t pin, uint8_t *out);

/** Clock `length` bytes out of a 74HC595 chain, MSB first, then latch. */
void ekt_port_shift74hc595_write(uint8_t clock_pin, uint8_t data_pin,
                                uint8_t latch_pin, const uint8_t *data,
                                uint8_t length);

/** Milliseconds since boot. Wraps every ~49 days; only differences are used. */
uint32_t ekt_port_millis(void);

/** Spin for a bounded number of microseconds (bus recovery / bit-banging only). */
void ekt_port_delay_us(uint32_t microseconds);

#ifdef __cplusplus
}
#endif

#endif /* EKT_PORT_H */
