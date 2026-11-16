/*
 * SoftTimerBank - Non-blocking periodic scheduling with the ekt_soft_timer
 * bank.
 *
 * The library never calls delay(). A hardware tick advances the timer bank and
 * the sketch asks the bank what is due, so one slow task cannot stall the
 * rest of the system.
 *
 * SPDX-License-Identifier: MIT
 */
#include <ekt_soft_timer.h>

/* One slot per periodic job. */
static ekt_timer_t timers[3];

/* Called from the 1 kHz hardware tick (Timer1 on AVR, a timer task on ESP32). */
void tick_1ms(void)
{
    ekt_timer_tick(timers, 3U, 1U);
}

void setup()
{
    Serial.begin(115200);

    ekt_timer_init(timers, 3U);

    ekt_timer_start(timers, 0U, 100U);  /* fast  : every 100 ms  */
    ekt_timer_start(timers, 1U, 1000U); /* slow  : every 1 s    */
    ekt_timer_start(timers, 2U, 5000U); /* blink : every 5 s    */

    Serial.println("ekt_soft_timer: three timers armed (100 ms, 1 s, 5 s)");
}

void loop()
{
    tick_1ms();

    if (ekt_timer_expired(timers, 0U)) {
        Serial.print("fast  t=");
        Serial.print(millis());
        Serial.println(" ms");
    }

    if (ekt_timer_expired(timers, 1U)) {
        Serial.print("slow  t=");
        Serial.print(millis());
        Serial.println(" ms");
    }

    if (ekt_timer_expired(timers, 2U)) {
        Serial.print("blink t=");
        Serial.print(millis());
        Serial.println(" ms");
    }
}
