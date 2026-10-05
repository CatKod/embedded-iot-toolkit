/*
 * ekt_port_posix.c - Host implementation used by the test suite and the CLI
 * example. Timing comes from the host clock; bus functions are scripted by the
 * test so hardware behaviour can be replayed deterministically.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ekt/ekt_port.h"

#include <stddef.h>
#include <time.h>

/* ---- Scripted DHT11 responses, consumed one per exchange. ---- */
static const uint8_t *g_dht_script = NULL;
static size_t        g_dht_len     = 0U;
static size_t        g_dht_pos     = 0U;
static bool          g_dht_fail    = false;

void ekt_test_set_dht_script(const uint8_t *bytes, size_t len, bool fail_next)
{
    g_dht_script = bytes;
    g_dht_len    = len;
    g_dht_pos    = 0U;
    g_dht_fail   = fail_next;
}

bool ekt_port_dht11_exchange(uint8_t pin, uint8_t *out)
{
    size_t i;

    (void)pin;

    if ((g_dht_fail == true) || (g_dht_script == NULL) ||
        ((g_dht_pos + 5U) > g_dht_len)) {
        return false;
    }
    for (i = 0U; i < 5U; i++) {
        out[i] = g_dht_script[g_dht_pos + i];
    }
    g_dht_pos += 5U;
    return true;
}

void ekt_port_shift74hc595_write(uint8_t clock_pin, uint8_t data_pin,
                                uint8_t latch_pin, const uint8_t *data,
                                uint8_t length)
{
    (void)clock_pin;
    (void)data_pin;
    (void)latch_pin;
    (void)data;
    (void)length;
    /* No hardware on the host; the state machine is asserted from the shadow
     * register instead. */
}

uint32_t ekt_port_millis(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }
    return (uint32_t)(((uint32_t)ts.tv_sec * 1000U) +
                      ((uint32_t)ts.tv_nsec / 1000000U));
}

void ekt_port_delay_us(uint32_t microseconds)
{
    struct timespec req;
    struct timespec rem;

    req.tv_sec  = (time_t)(microseconds / 1000000U);
    req.tv_nsec = (long)((microseconds % 1000000U) * 1000U);
    (void)nanosleep(&req, &rem);
}
