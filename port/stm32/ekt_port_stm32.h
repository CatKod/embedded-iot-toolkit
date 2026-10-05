/*
 * ekt_port_stm32.h - Pin/peripheral configuration for the STM32 port.
 *
 * Adjust the defines below to match your board, or replace them with a
 * generated header from CubeMX. Nothing in the toolkit core needs to change.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef EKT_PORT_STM32_H
#define EKT_PORT_STM32_H

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- DHT11 ---- */
#define EKT_STM32_DHT_GPIO_PORT   GPIOB
#define EKT_STM32_DHT_GPIO_PIN    GPIO_PIN_0
#define EKT_STM32_DHT_PULL        GPIO_PULLUP

/* ---- 74HC595 shift register ---- */
#define EKT_STM32_SR_GPIO_PORT    GPIOB
#define EKT_STM32_SR_CLK_PIN      GPIO_PIN_3   /* SHIFT / SRCLK */
#define EKT_STM32_SR_DATA_PIN     GPIO_PIN_5   /* SER / SERIAL DATA IN */
#define EKT_STM32_SR_LATCH_PIN    GPIO_PIN_4   /* LATCH / RCLK */

/* ---- 1 kHz time base ----
 * Prefer a hardware timer interrupt over SysTick so the tick keeps running
 * while the RTOS is in a critical section. */
extern TIM_HandleTypeDef htim_1ms;

#ifdef __cplusplus
}
#endif

#endif /* EKT_PORT_STM32_H */
