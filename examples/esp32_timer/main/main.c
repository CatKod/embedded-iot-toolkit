/*
 * ekt_esp32_timer - ESP-IDF example for the embedded-iot-toolkit component.
 *
 * Drives the non-blocking timer bank from a FreeRTOS task that emulates the
 * 1 kHz hardware tick, and prints what is due.
 *
 * SPDX-License-Identifier: MIT
 */
#include <inttypes.h>
#include <stdio.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ekt_soft_timer.h"

/* One slot per periodic job. */
static ekt_timer_t timers[2];

static void tick_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        /* Advance the bank by however many milliseconds have passed. */
        TickType_t now  = xTaskGetTickCount();
        uint32_t elapsed = (uint32_t)(now - last) * portTICK_PERIOD_MS;
        last             = now;

        ekt_timer_tick(timers, 2U, elapsed);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_main(void)
{
    ekt_timer_init(timers, 2U);
    ekt_timer_start(timers, 0U, 100U);  /* fast: every 100 ms */
    ekt_timer_start(timers, 1U, 1000U); /* slow: every 1 s    */

    xTaskCreate(tick_task, "ekt_tick", 2048, NULL, 5, NULL);

    while (true) {
        if (ekt_timer_expired(timers, 0U)) {
            printf("fast t=%" PRIu32 " ms\n",
                   (uint32_t)(esp_timer_get_time() / 1000LL));
        }
        if (ekt_timer_expired(timers, 1U)) {
            printf("slow t=%" PRIu32 " ms\n",
                   (uint32_t)(esp_timer_get_time() / 1000LL));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
