/*
 * ekt_can_codec.c - Portable CAN 2.0B identifier codec + bit-timing solver.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ekt_can_codec.h"

#include <stddef.h>

#define EKT_EFF_FLAG 0x80000000U /* Frame format: extended ID */

static uint32_t ekt_field_mask(uint8_t width)
{
    if (width >= 32U) {
        return 0xFFFFFFFFU;
    }
    return (1UL << width) - 1UL;
}

static bool ekt_field_fits(uint8_t value, uint8_t width)
{
    if (width >= 32U) {
        return true;
    }
    return (uint32_t)value <= ekt_field_mask(width);
}

uint32_t ekt_can_id_pack(const ekt_can_id_t *fields)
{
    uint32_t id;

    if (fields == NULL) {
        return 0xFFFFFFFFU;
    }
    if ((ekt_field_fits(fields->priority, 2U) == false) ||
        (ekt_field_fits(fields->command, 6U) == false) ||
        (ekt_field_fits(fields->target_type, 3U) == false) ||
        (ekt_field_fits(fields->target_id, 7U) == false) ||
        (ekt_field_fits(fields->source_id, 5U) == false) ||
        (ekt_field_fits(fields->reserved, 6U) == false)) {
        return 0xFFFFFFFFU;
    }

    id  = (uint32_t)fields->priority    << 27U;
    id |= (uint32_t)fields->command     << 21U;
    id |= (uint32_t)fields->target_type << 18U;
    id |= (uint32_t)fields->target_id   << 11U;
    id |= (uint32_t)fields->source_id   << 6U;
    id |= (uint32_t)fields->reserved;

    return id | EKT_EFF_FLAG;
}

bool ekt_can_id_unpack(uint32_t id, ekt_can_id_t *out)
{
    if (out == NULL) {
        return false;
    }
    if ((id & EKT_EFF_FLAG) == 0U) {
        return false;
    }

    out->priority    = (uint8_t)((id >> 27U) & 0x3U);
    out->command     = (uint8_t)((id >> 21U) & 0x3FU);
    out->target_type = (uint8_t)((id >> 18U) & 0x7U);
    out->target_id   = (uint8_t)((id >> 11U) & 0x7FU);
    out->source_id   = (uint8_t)((id >> 6U) & 0x1FU);
    out->reserved    = (uint8_t)(id & 0x3FU);
    return true;
}

uint32_t ekt_can_id_std(uint8_t node_id, uint8_t command)
{
    return ((uint32_t)node_id << 3U) | (uint32_t)(command & 0x07U);
}

/* Time quantum per bit = 1 + (TSEG1 + 1) + (TSEG2 + 1) */
bool ekt_can_bit_timing(uint32_t can_clock_hz, uint32_t bitrate_bps,
                        float sample_point, uint16_t *out_prescaler,
                        uint16_t *out_tseg1, uint16_t *out_tseg2)
{
    uint32_t prescaler;
    uint32_t best_prescaler = 0U;
    uint32_t best_tseg1    = 0U;
    uint32_t best_tseg2    = 0U;
    uint32_t best_error    = 0xFFFFFFFFU;
    float    tq_total_f;
    uint32_t tq_total;
    uint32_t tseg1_actual;
    uint32_t tseg2_actual;
    uint32_t tseg1;
    uint32_t tseg2;
    uint32_t error;

    if ((can_clock_hz == 0U) || (bitrate_bps == 0U) ||
        (sample_point <= 0.0f) || (sample_point >= 1.0f) ||
        (out_prescaler == NULL) || (out_tseg1 == NULL) || (out_tseg2 == NULL)) {
        return false;
    }

    for (prescaler = 1U; prescaler <= 1024U; prescaler++) {
        if ((can_clock_hz % (prescaler * bitrate_bps)) != 0U) {
            continue;
        }
        tq_total_f = (float)can_clock_hz / ((float)prescaler * (float)bitrate_bps);
        tq_total   = (uint32_t)(tq_total_f + 0.5f);

        /* Legal range for the arbitration phase. */
        if ((tq_total < 8U) || (tq_total > 25U)) {
            continue;
        }
        tseg1_actual = (uint32_t)((float)tq_total * sample_point) - 1U;
        if (tseg1_actual < 1U) {
            tseg1_actual = 1U;
        }
        tseg2_actual = tq_total - 1U - tseg1_actual;
        if (tseg2_actual < 1U) {
            tseg2_actual = 1U;
        }
        /* Registers hold (count - 1) for both segments. */
        tseg1 = tseg1_actual - 1U;
        tseg2 = tseg2_actual - 1U;
        if (tseg1 > 255U) {
            tseg1 = 255U;
        }
        if (tseg2 > 127U) {
            tseg2 = 127U;
        }

        error = (tq_total_f > (float)tq_total)
                    ? (tq_total - (uint32_t)tq_total_f)
                    : ((uint32_t)tq_total_f - tq_total);
        if (error < best_error) {
            best_error    = error;
            best_prescaler = prescaler;
            best_tseg1    = tseg1;
            best_tseg2    = tseg2;
        }
        if (error == 0U) {
            break;
        }
    }

    if (best_prescaler == 0U) {
        return false;
    }
    *out_prescaler = (uint16_t)best_prescaler;
    *out_tseg1     = (uint16_t)best_tseg1;
    *out_tseg2     = (uint16_t)best_tseg2;
    return true;
}
