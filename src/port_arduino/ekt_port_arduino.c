/*
 * ekt_port_arduino.c - Arduino implementation of the toolkit port layer.
 *
 * Uses the Arduino core API only, so the port builds on AVR, SAM, and ESP32
 * without conditional compilation.
 *
 * Note on timing: the DHT11 read is microsecond-sensitive. On AVR the timing
 * comes from busy-waiting on the micros() counter, which is coarse but works
 * for the DHT11's generous 26..70 us high window. On 32-bit cores the
 * digitalWrite() call overhead is small enough to keep the same margins.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ekt_port.h"

#include "ekt_port_arduino.h"

uint32_t ekt_port_millis(void)
{
    return (uint32_t)millis();
}

void ekt_port_delay_us(uint32_t microseconds)
{
    delayMicroseconds(microseconds);
}

static void dht11_pin_output(void)
{
    pinMode(EKT_ARDUINO_DHT_PIN, OUTPUT);
    digitalWrite(EKT_ARDUINO_DHT_PIN, HIGH);
}

static void dht11_pin_input(void)
{
    pinMode(EKT_ARDUINO_DHT_PIN, INPUT_PULLUP);
}

/*
 * Read the level of the DHT11 data line without the overhead of a full pin
 * read. On AVR this uses the port registers directly; elsewhere the generic
 * digitalRead() is used.
 */
#if defined(ARDUINO_ARCH_AVR)
static inline uint8_t dht11_read_raw(void)
{
    uint8_t port = digitalPinToPort(EKT_ARDUINO_DHT_PIN);
    uint8_t bit  = digitalPinToBitMask(EKT_ARDUINO_DHT_PIN);

    if (port == NOT_A_PIN) {
        return (uint8_t)(digitalRead(EKT_ARDUINO_DHT_PIN) ? 1U : 0U);
    }
    return (uint8_t)((*portInputRegister(port) & bit) ? 1U : 0U);
}
#else
static inline uint8_t dht11_read_raw(void)
{
    return (uint8_t)(digitalRead(EKT_ARDUINO_DHT_PIN) ? 1U : 0U);
}
#endif

bool ekt_port_dht11_exchange(uint8_t pin, uint8_t *out)
{
    uint32_t i;
    uint8_t  byte = 0U;

    (void)pin;

    /* --- Host start signal: pull low for >= 18 ms --- */
    dht11_pin_output();
    digitalWrite(EKT_ARDUINO_DHT_PIN, LOW);
    delay(20);
    digitalWrite(EKT_ARDUINO_DHT_PIN, HIGH);
    ekt_port_delay_us(20U);

    /* --- Sensor response: pull low 80 us, then high 80 us --- */
    dht11_pin_input();
    ekt_port_delay_us(10U);
    if (dht11_read_raw() != 0U) {
        return false; /* no sensor responded */
    }
    ekt_port_delay_us(50U);

    /* --- 40 data bits, MSB first ---
     * Each bit: 50 us low, then 26-28 us high for '0' or 70 us for '1'. */
    for (i = 0U; i < 5U; i++) {
        out[i] = 0U;
    }

    for (i = 0U; i < 40U; i++) {
        uint32_t high_us = 0U;

        /* Wait for the line to go low (start of the next bit). */
        while (dht11_read_raw() != 0U) {
        }
        ekt_port_delay_us(45U); /* past the 50 us low period */

        /* Measure how long it stays high: that duration is the bit value. */
        while (dht11_read_raw() != 0U) {
            ekt_port_delay_us(2U);
            high_us += 2U;
            if (high_us > 120U) {
                break; /* stuck high, bail out */
            }
        }
        if (high_us > 40U) {
            byte = (uint8_t)(byte | (1U << (7U - (i % 8U))));
        }
        if ((i % 8U) == 7U) {
            out[i / 8U] = byte;
            byte = 0U;
        }
    }
    return true;
}

void ekt_port_shift74hc595_write(uint8_t clock_pin, uint8_t data_pin,
                                uint8_t latch_pin, const uint8_t *data,
                                uint8_t length)
{
    uint8_t  byte;
    uint8_t  bit;
    uint16_t i;

    (void)clock_pin;
    (void)data_pin;
    (void)latch_pin;

    /* Latch low for the whole shift-out, raise it once at the end. */
    digitalWrite(EKT_ARDUINO_SR_LATCH_PIN, LOW);

    for (i = 0U; i < (uint16_t)length; i++) {
        byte = data[i];
        for (bit = 0U; bit < 8U; bit++) {
            /* Present the bit on SER, then pulse SHCLK. */
            digitalWrite(EKT_ARDUINO_SR_DATA_PIN,
                         (byte & 0x80U) ? HIGH : LOW);
            ekt_port_delay_us(1U);
            digitalWrite(EKT_ARDUINO_SR_CLK_PIN, HIGH);
            ekt_port_delay_us(1U);
            digitalWrite(EKT_ARDUINO_SR_CLK_PIN, LOW);
            byte = (uint8_t)(byte << 1);
        }
    }

    /* Rising edge on LATCH transfers the shift register to the outputs. */
    ekt_port_delay_us(1U);
    digitalWrite(EKT_ARDUINO_SR_LATCH_PIN, HIGH);
}
