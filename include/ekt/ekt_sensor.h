/*
 * ekt_sensor.h - Blocking-free sensor abstractions.
 *
 * Each driver polls its bus, returns a reading immediately, and reports its own
 * health. A caller that misses a deadline sees `ekt_sample_t::ok == false`
 * rather than a stale value, so upstream logic never acts on old data.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef EKT_SENSOR_H
#define EKT_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EKT_SENSOR_OK = 0,
    EKT_SENSOR_TIMEOUT,  /**< Bus transaction did not complete. */
    EKT_SENSOR_CRC_FAIL,  /**< Sensor reported a bad checksum. */
    EKT_SENSOR_NO_ANSWER, /**< Sensor did not pull the line low (DHT11 family). */
    EKT_SENSOR_RANGE,     /**< Reading outside the sensor's plausible range. */
} ekt_sensor_status_t;

typedef struct {
    int32_t              value;    /**< Raw or scaled reading. */
    ekt_sensor_status_t  status;
    uint16_t             sequence; /**< Increments on every successful read. */
} ekt_sample_t;

/* ---------------------------------------------------------------- DHT11 --- */

typedef struct {
    uint8_t  pin;                 /**< GPIO the sensor data line is wired to. */
    uint32_t min_interval_ms;     /**< Sensor minimum sampling interval. */
    uint32_t last_read_ms;        /**< Timestamp of the previous accepted read. */
    uint8_t  consecutive_failures;
} ekt_dht11_t;

void ekt_dht11_init(ekt_dht11_t *dev, uint8_t pin, uint32_t min_interval_ms);

/** Read temperature (°C) and humidity (%RH). Non-blocking. */
ekt_sensor_status_t ekt_dht11_read(ekt_dht11_t *dev, uint32_t now_ms,
                                   int16_t *temperature_c,
                                   uint8_t *humidity_pct);

/* ------------------------------------------------------------- RC522 RFID --- */

typedef struct {
    uint8_t  uid[10];
    uint8_t  uid_len;
    uint8_t  last_error;
    uint32_t last_seen_ms;
} ekt_rfid_state_t;

/** True when a card is present and the UID has been read. */
bool ekt_rfid_present(const ekt_rfid_state_t *st);

/** Stable-compare UIDs; constant time on the length check. */
bool ekt_rfid_uid_equal(const ekt_rfid_state_t *st, const uint8_t *uid, uint8_t len);

/* ------------------------------------------------------- 74HC595 shift reg --- */

typedef struct {
    uint8_t  chain_len;  /**< Number of chips daisy-chained. */
    uint16_t shadow;     /**< Desired output state. */
    uint16_t latched;    /**< Last value actually clocked out. */
} ekt_shift74hc595_t;

void ekt_shift74hc595_init(ekt_shift74hc595_t *dev, uint8_t chain_len);

/** Set one output bit in the shadow register (does not clock it out yet). */
void ekt_shift74hc595_set(ekt_shift74hc595_t *dev, uint8_t index, bool on);

/** Clear one output bit in the shadow register. */
void ekt_shift74hc595_clear(ekt_shift74hc595_t *dev, uint8_t index);

/** True when the shadow and the latched hardware state differ. */
bool ekt_shift74hc595_dirty(const ekt_shift74hc595_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* EKT_SENSOR_H */
