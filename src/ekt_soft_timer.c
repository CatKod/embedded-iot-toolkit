/*
 * ekt_soft_timer.c - Non-blocking software timer bank.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ekt/ekt_soft_timer.h"

#include <string.h>

void ekt_timer_init(ekt_timer_t *timers, uint8_t count)
{
    if (timers == NULL) {
        return;
    }
    (void)memset(timers, 0, (size_t)count * sizeof(ekt_timer_t));
}

void ekt_timer_start(ekt_timer_t *timers, uint8_t index, uint32_t period_ms)
{
    if ((timers == NULL) || (index >= EKT_TIMER_MAX_SLOTS) || (period_ms == 0U)) {
        return;
    }
    timers[index].period_ms = period_ms;
    timers[index].elapsed_ms = 0U;
    timers[index].active    = true;
    timers[index].pending   = false;
}

void ekt_timer_stop(ekt_timer_t *timers, uint8_t index)
{
    if ((timers == NULL) || (index >= EKT_TIMER_MAX_SLOTS)) {
        return;
    }
    timers[index].active  = false;
    timers[index].pending = false;
}

void ekt_timer_tick(ekt_timer_t *timers, uint8_t count, uint32_t elapsed_ms)
{
    uint8_t i;

    if (timers == NULL) {
        return;
    }

    for (i = 0U; i < count; i++) {
        if (timers[i].active == false) {
            continue;
        }
        timers[i].elapsed_ms += elapsed_ms;
        /* Loop instead of a single comparison so a long gap between ticks
         * still produces the right number of expiries. */
        while (timers[i].elapsed_ms >= timers[i].period_ms) {
            timers[i].elapsed_ms -= timers[i].period_ms;
            timers[i].pending    = true;
        }
    }
}

bool ekt_timer_expired(ekt_timer_t *timers, uint8_t index)
{
    if ((timers == NULL) || (index >= EKT_TIMER_MAX_SLOTS)) {
        return false;
    }
    if (timers[index].pending == false) {
        return false;
    }
    timers[index].pending = false;
    return true;
}
