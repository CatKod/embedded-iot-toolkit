/*
 * ekt_sensor.c - Sensor helpers that carry no hardware dependency of their own.
 *
 * The bus transactions live in the port layer (see port/stm32, port/esp32).
 * Everything here is pure logic, so it is directly unit-testable on a PC.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ekt_port.h"
#include "ekt_sensor.h"

#include <stddef.h>
#include <string.h>

/* ---------------------------------------------------------------- DHT11 --- */

void ekt_dht11_init(ekt_dht11_t *dev, uint8_t pin, uint32_t min_interval_ms)
{
    if (dev == NULL) {
        return;
    }
    dev->pin                 = pin;
    dev->min_interval_ms     = min_interval_ms;
    dev->last_read_ms        = 0U;
    dev->consecutive_failures = 0U;
}

ekt_sensor_status_t ekt_dht11_read(ekt_dht11_t *dev, uint32_t now_ms,
                                   int16_t *temperature_c,
                                   uint8_t *humidity_pct)
{
    uint8_t raw[5];
    uint8_t checksum;
    uint8_t humidity_int;
    uint8_t humidity_dec;
    int16_t temperature;

    if (dev == NULL) {
        return EKT_SENSOR_NO_ANSWER;
    }

    /* The DHT11 needs ~1 s between conversions. Polling faster than that both
     * wastes bus time and returns stale data, so refuse early. */
    if ((uint32_t)(now_ms - dev->last_read_ms) < dev->min_interval_ms) {
        return EKT_SENSOR_TIMEOUT;
    }
    dev->last_read_ms = now_ms;

    /* Port hook: performs the start pulse, reads 40 bits, returns false on a
     * bus timeout. Implemented per platform. */
    if (ekt_port_dht11_exchange(dev->pin, raw) == false) {
        dev->consecutive_failures++;
        return EKT_SENSOR_TIMEOUT;
    }

    checksum = (uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]);
    if (checksum != raw[4]) {
        dev->consecutive_failures++;
        return EKT_SENSOR_CRC_FAIL;
    }
    dev->consecutive_failures = 0U;

    /* DHT11 payload layout: RH integer, RH decimal, T integer, T decimal.
     * The decimal byte is always 0 on a DHT11 and 0 or 1 on a DHT22. */
    humidity_int = raw[0];
    humidity_dec = raw[1];

    if ((humidity_int < 20U) || (humidity_int > 90U) || (humidity_dec > 9U)) {
        /* DHT11 reports 20..90 %RH; anything else is implausible. */
        return EKT_SENSOR_RANGE;
    }

    /* Negative temperatures set bit 7 of the temperature MSB and store the
     * absolute value in the remaining 15 bits, so strip the sign, scale, then
     * negate. Note the order: the *10 scaling must happen before the negate. */
    if ((raw[2] & 0x80U) != 0U) {
        uint16_t magnitude = (uint16_t)((((uint16_t)(raw[2] & 0x7FU)) << 8) |
                                        (uint16_t)raw[3]);
        temperature = (int16_t)(-((int32_t)magnitude * 10));
    } else {
        temperature = (int16_t)(((int32_t)raw[2] * 10) + (int32_t)raw[3]);
    }

    if (humidity_pct != NULL) {
        *humidity_pct = (uint8_t)(((uint32_t)humidity_int * 10U + humidity_dec) /
                                 10U);
    }
    if (temperature_c != NULL) {
        *temperature_c = temperature;
    }
    return EKT_SENSOR_OK;
}

/* ------------------------------------------------------------- RC522 RFID --- */

bool ekt_rfid_present(const ekt_rfid_state_t *st)
{
    if (st == NULL) {
        return false;
    }
    return (st->uid_len > 0U) ? true : false;
}

bool ekt_rfid_uid_equal(const ekt_rfid_state_t *st, const uint8_t *uid, uint8_t len)
{
    uint8_t diff = 0U;
    uint8_t i;

    if ((st == NULL) || (uid == NULL) || (st->uid_len == 0U) ||
        (st->uid_len != len)) {
        return false;
    }
    for (i = 0U; i < len; i++) {
        diff = (uint8_t)(diff | (uint8_t)(st->uid[i] ^ uid[i]));
    }
    return (diff == 0U) ? true : false;
}

/* ------------------------------------------------------- 74HC595 shift reg --- */

void ekt_shift74hc595_init(ekt_shift74hc595_t *dev, uint8_t chain_len)
{
    if (dev == NULL) {
        return;
    }
    dev->chain_len = (chain_len == 0U) ? 1U : chain_len;
    dev->shadow    = 0U;
    dev->latched   = 0U;
}

void ekt_shift74hc595_set(ekt_shift74hc595_t *dev, uint8_t index, bool on)
{
    if ((dev == NULL) || (dev->chain_len == 0U) || (index >= dev->chain_len)) {
        return;
    }
    if (on) {
        dev->shadow = (uint16_t)(dev->shadow | (1U << index));
    } else {
        dev->shadow = (uint16_t)(dev->shadow & ~(1U << index));
    }
}

void ekt_shift74hc595_clear(ekt_shift74hc595_t *dev, uint8_t index)
{
    ekt_shift74hc595_set(dev, index, false);
}

bool ekt_shift74hc595_dirty(const ekt_shift74hc595_t *dev)
{
    if (dev == NULL) {
        return false;
    }
    return (dev->shadow != dev->latched) ? true : false;
}
