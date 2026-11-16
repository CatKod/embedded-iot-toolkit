/*
 * ekt_port_esp32.c - ESP-IDF implementation of the toolkit port layer.
 *
 * Requires ESP-IDF v5.0 or newer.
 *
 * Note on timing: the DHT11 read is microsecond-sensitive. On ESP32 the
 * esp_timer-backed delay is accurate to a few microseconds, which is well
 * inside the DHT11's 26..70 us high window. The read itself runs at task
 * priority, so avoid calling it from a low-priority task that can be
 * preempted mid-bit; rely on the DHT11 checksum to reject corrupted samples.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ekt_port.h"

#include "ekt_port_esp32.h"

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t ekt_port_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

void ekt_port_delay_us(uint32_t microseconds)
{
    /* esp_timer's busy-wait is accurate to about a microsecond, which is
     * finer than the DHT11 timing windows need. */
    esp_rom_delay_us((uint32_t)microseconds);
}

static void dht11_pin_output(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EKT_ESP32_DHT_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&io_conf);
}

static void dht11_pin_input(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EKT_ESP32_DHT_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&io_conf);
}

bool ekt_port_dht11_exchange(uint8_t pin, uint8_t *out)
{
    uint32_t i;
    uint8_t  byte = 0U;

    (void)pin;

    /* --- Host start signal: pull low for >= 18 ms --- */
    dht11_pin_output();
    (void)gpio_set_level(EKT_ESP32_DHT_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    (void)gpio_set_level(EKT_ESP32_DHT_GPIO, 1);
    ekt_port_delay_us(20U);

    /* --- Sensor response: pull low 80 us, then high 80 us --- */
    dht11_pin_input();
    ekt_port_delay_us(10U);
    if (gpio_get_level(EKT_ESP32_DHT_GPIO) != 0) {
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
        while (gpio_get_level(EKT_ESP32_DHT_GPIO) != 0) {
        }
        ekt_port_delay_us(45U); /* past the 50 us low period */

        /* Measure how long it stays high: that duration is the bit value. */
        while (gpio_get_level(EKT_ESP32_DHT_GPIO) != 0) {
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
    uint32_t clock_gpio = (clock_pin != 0U) ? clock_pin
                                             : EKT_ESP32_SR_CLK_GPIO;
    uint32_t data_gpio  = (data_pin != 0U) ? data_pin
                                            : EKT_ESP32_SR_DATA_GPIO;
    uint32_t latch_gpio = (latch_pin != 0U) ? latch_pin
                                             : EKT_ESP32_SR_LATCH_GPIO;

    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << clock_gpio) | (1ULL << data_gpio) |
                            (1ULL << latch_gpio),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        (void)gpio_config(&io_conf);
    }

    /* Latch low for the whole shift-out, raise it once at the end. */
    (void)gpio_set_level(latch_gpio, 0);

    for (i = 0U; i < (uint16_t)length; i++) {
        byte = data[i];
        for (bit = 0U; bit < 8U; bit++) {
            /* Present the bit on SER, then pulse SHCLK. */
            (void)gpio_set_level(data_gpio, (byte & 0x80U) ? 1U : 0U);
            ekt_port_delay_us(1U);
            (void)gpio_set_level(clock_gpio, 1U);
            ekt_port_delay_us(1U);
            (void)gpio_set_level(clock_gpio, 0U);
            byte = (uint8_t)(byte << 1);
        }
    }

    /* Rising edge on LATCH transfers the shift register to the outputs. */
    ekt_port_delay_us(1U);
    (void)gpio_set_level(latch_gpio, 1U);
}
