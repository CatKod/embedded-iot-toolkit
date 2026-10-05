/*
 * ekt_soft_timer.h - Non-blocking software timer bank.
 *
 * A 1 kHz hardware tick (TIM/Tasks) calls ekt_timer_tick() and every periodic
 * job in the firmware asks this bank whether it is due. Nothing in the library
 * ever blocks, so no code path can stall the rest of the system.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef EKT_SOFT_TIMER_H
#define EKT_SOFT_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EKT_TIMER_MAX_SLOTS
#define EKT_TIMER_MAX_SLOTS 16U
#endif

/** A single non-blocking periodic timer. */
typedef struct {
    uint32_t period_ms; /**< Interval; 0 disables the slot. */
    uint32_t elapsed_ms; /**< Time accumulated since the last expiry. */
    bool active;         /**< True while the slot is armed. */
    bool pending;        /**< Set on expiry, cleared by the consumer. */
} ekt_timer_t;

void ekt_timer_init(ekt_timer_t *timers, uint8_t count);

/** Start (or restart) a timer slot with the given period. */
void ekt_timer_start(ekt_timer_t *timers, uint8_t index, uint32_t period_ms);

/** Stop a timer slot and clear any pending expiry. */
void ekt_timer_stop(ekt_timer_t *timers, uint8_t index);

/** Advance the bank by `elapsed_ms`. Call once per hardware tick. */
void ekt_timer_tick(ekt_timer_t *timers, uint8_t count, uint32_t elapsed_ms);

/** Non-blocking poll: true exactly once per completed period. */
bool ekt_timer_expired(ekt_timer_t *timers, uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* EKT_SOFT_TIMER_H */
