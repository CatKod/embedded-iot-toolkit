/*
 * ekt_can_codec.h - Portable CAN 2.0B identifier codec.
 *
 * Every message on an industrial bus is identified by a 29-bit extended
 * identifier built from a small number of packed fields. This codec encodes and
 * decodes that layout without depending on any vendor HAL, so the same table
 * can be unit-tested on a PC and reused across STM32H7, STM32G4, and ESP32.
 *
 * A typical layout used on a multi-gun charging network:
 *
 *   bits 28..27  priority   (2 bits)   lower number = higher priority
 *   bits 26..21  command    (6 bits)
 *   bits 20..18  target type(3 bits)   station / power module / contactor ...
 *   bits 17..11  target id  (7 bits)
 *   bits 10..6   source id  (5 bits)
 *   bits  5..0   reserved   (6 bits)
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef EKT_CAN_CODEC_H
#define EKT_CAN_CODEC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Standard 11-bit identifier. */
#define EKT_CAN_ID_STD_MASK 0x000007FFU

/** Flag OR-ed into the payload so a receiver can ignore padding. */
#define EKT_CAN_PAYLOAD_EXT 0x80000000U

typedef struct {
    uint8_t  priority;    /**< 0 = most urgent. */
    uint8_t  command;     /**< Command code. */
    uint8_t  target_type; /**< Addressable entity class. */
    uint8_t  target_id;   /**< Entity instance, 0-based. */
    uint8_t  source_id;   /**< Originating entity instance. */
    uint8_t  reserved;    /**< Reserved / future use. */
} ekt_can_id_t;

/**
 * Pack the fields into a 29-bit extended identifier.
 *
 * @return The 29-bit identifier with the EFF flag bit in the top position, or
 *         0xFFFFFFFF when a field does not fit its allotted width.
 */
uint32_t ekt_can_id_pack(const ekt_can_id_t *fields);

/** Unpack a 29-bit extended identifier. Returns false if `id` has EFF clear. */
bool ekt_can_id_unpack(uint32_t id, ekt_can_id_t *out);

/** Build a standard 11-bit identifier from a node id and a command. */
uint32_t ekt_can_id_std(uint8_t node_id, uint8_t command);

/**
 * Compute a CAN 2.0 bit-timing register triple for a given clock, sample
 * point, and target bitrate.
 *
 * Covers the arithmetic shared by bxCAN and bxCAN FD controllers, and matches
 * what the FDCAN message RAM expects for the data-phase prescaler.
 *
 * @param can_clock_hz  Controller kernel clock in Hz.
 * @param bitrate_bps  Target bitrate in bit/s.
 * @param sample_point  Sample point as a fraction, e.g. 0.75 for 75 %.
 * @param out_prescaler    Receives the BRP-1 prescaler value (1..1024).
 * @param out_tseg1        Receives TSEG1-1 (1..255).
 * @param out_tseg2        Receives TSEG2-1 (1..127).
 * @return true when an exact integer solution was found.
 */
bool ekt_can_bit_timing(uint32_t can_clock_hz, uint32_t bitrate_bps,
                        float sample_point, uint16_t *out_prescaler,
                        uint16_t *out_tseg1, uint16_t *out_tseg2);

#ifdef __cplusplus
}
#endif

#endif /* EKT_CAN_CODEC_H */
