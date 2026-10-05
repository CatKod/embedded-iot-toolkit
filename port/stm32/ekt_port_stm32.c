/*
 * ekt_port_stm32.c - STM32 HAL implementation of the toolkit port layer.
 *
 * Note on timing: the DHT11 read is inherently microsecond-sensitive, so it
 * must NOT run from a preemptive RTOS thread that can be descheduled mid-bit.
 * Either call it from a high-priority task with interrupts enabled, or accept
 * that the sampling may be corrupted and rely on the checksum to reject it.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ekt/ekt_port.h"

#include "ekt_port_stm32.h"

/* SysTick / TIM6 fire at 1 kHz and bump this counter. */
volatile uint32_t g_ekt_millis = 0U;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        g_ekt_millis++;
    }
}

uint32_t ekt_port_millis(void)
{
    /* Single 32-bit read on a 32-bit core is atomic, so no critical section. */
    return g_ekt_millis;
}

void ekt_port_delay_us(uint32_t microseconds)
{
    /* Busy-wait on the DWT cycle counter: ~1 cycle resolution and no
     * interrupt latency, which matters for bit-banged protocols. */
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = (microseconds * (HAL_RCC_GetHCLKFreq() / 1000000U));

    while ((DWT->CYCCNT - start) < ticks) {
        /* spin */
    }
}

static void dht11_pin_output(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin   = EKT_STM32_DHT_GPIO_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(EKT_STM32_DHT_GPIO_PORT, &gpio);
}

static void dht11_pin_input(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin   = EKT_STM32_DHT_GPIO_PIN;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = EKT_STM32_DHT_PULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(EKT_STM32_DHT_GPIO_PORT, &gpio);
}

bool ekt_port_dht11_exchange(uint8_t pin, uint8_t *out)
{
    uint32_t i;
    uint8_t  byte;

    (void)pin;

    /* --- Host start signal: pull low for >= 18 ms --- */
    dht11_pin_output();
    HAL_GPIO_WritePin(EKT_STM32_DHT_GPIO_PORT, EKT_STM32_DHT_GPIO_PIN,
                      GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(EKT_STM32_DHT_GPIO_PORT, EKT_STM32_DHT_GPIO_PIN,
                      GPIO_PIN_SET);
    ekt_port_delay_us(20U);

    /* --- Sensor response: pull low 80 us, then high 80 us --- */
    dht11_pin_input();
    ekt_port_delay_us(10U);
    if (HAL_GPIO_ReadPin(EKT_STM32_DHT_GPIO_PORT,
                         EKT_STM32_DHT_GPIO_PIN) != GPIO_PIN_RESET) {
        return false; /* no sensor responded */
    }
    ekt_port_delay_us(50U);

    /* --- 40 data bits, MSB first ---
     * Each bit: 50 us low, then 26-28 us high for '0' or 70 us for '1'. */
    for (i = 0U; i < 5U; i++) {
        out[i] = 0U;
    }

    for (i = 0U; i < 40U; i++) {
        uint32_t high_us;

        /* Wait for the line to go low (start of the next bit). */
        while (HAL_GPIO_ReadPin(EKT_STM32_DHT_GPIO_PORT,
                                EKT_STM32_DHT_GPIO_PIN) != GPIO_PIN_RESET) {
        }
        ekt_port_delay_us(45U); /* past the 50 us low period */

        /* Measure how long it stays high: that duration is the bit value. */
        high_us = 0U;
        while (HAL_GPIO_ReadPin(EKT_STM32_DHT_GPIO_PORT,
                                EKT_STM32_DHT_GPIO_PIN) == GPIO_PIN_SET) {
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
    HAL_GPIO_WritePin(EKT_STM32_SR_GPIO_PORT, EKT_STM32_SR_LATCH_PIN,
                      GPIO_PIN_RESET);

    for (i = 0U; i < (uint16_t)length; i++) {
        byte = data[i];
        for (bit = 0U; bit < 8U; bit++) {
            /* Present the bit on SER, then pulse SHCLK. */
            if ((byte & 0x80U) != 0U) {
                HAL_GPIO_WritePin(EKT_STM32_SR_GPIO_PORT,
                                  EKT_STM32_SR_DATA_PIN, GPIO_PIN_SET);
            } else {
                HAL_GPIO_WritePin(EKT_STM32_SR_GPIO_PORT,
                                  EKT_STM32_SR_DATA_PIN, GPIO_PIN_RESET);
            }
            ekt_port_delay_us(1U);
            HAL_GPIO_WritePin(EKT_STM32_SR_GPIO_PORT, EKT_STM32_SR_CLK_PIN,
                              GPIO_PIN_SET);
            ekt_port_delay_us(1U);
            HAL_GPIO_WritePin(EKT_STM32_SR_GPIO_PORT, EKT_STM32_SR_CLK_PIN,
                              GPIO_PIN_RESET);
            byte = (uint8_t)(byte << 1);
        }
    }

    /* Rising edge on LATCH transfers the shift register to the outputs. */
    ekt_port_delay_us(1U);
    HAL_GPIO_WritePin(EKT_STM32_SR_GPIO_PORT, EKT_STM32_SR_LATCH_PIN,
                      GPIO_PIN_SET);
}
