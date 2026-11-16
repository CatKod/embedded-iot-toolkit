/*
 * ekt_frame.h - ASCII framed serial protocol for MCU <-> MCU links.
 *
 * Wire format (derived from a production smart-home STM32H7 <-> ESP32-S3 link):
 *
 *     <STX>TYPE,field1,field2,...*CS<ETX>
 *
 *   STX = 0x02   start of frame
 *   ETX = 0x03   end of frame
 *   CS  = XOR of every byte of "TYPE,field1,..." rendered as 2 uppercase hex
 *
 * The design goals are deliberately modest:
 *   - human-readable on a logic analyser or serial terminal
 *   - trivially parsable in Python / MicroPython on the far side
 *   - checksummed, so a corrupted line is dropped rather than acted on
 *   - no dynamic allocation, bounded state, safe to call from an ISR
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef EKT_FRAME_H
#define EKT_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EKT_FRAME_MAX_LEN
#define EKT_FRAME_MAX_LEN 128U
#endif

typedef enum {
    EKT_FRAME_OK = 0,   /**< A complete, checksum-valid frame was parsed. */
    EKT_FRAME_EMPTY,    /**< No complete frame yet; feed more bytes. */
    EKT_FRAME_TOO_LONG, /**< Buffer overflowed; frame discarded, resyncing. */
    EKT_FRAME_BAD_CRC,  /**< Checksum mismatch; frame discarded. */
    EKT_FRAME_BAD_TYPE, /**< Checksum ok but the type field is empty. */
} ekt_frame_status_t;

typedef struct {
    char type[12];                       /**< Frame type, e.g. "DATA", "CMD". */
    char fields[16][16];                 /**< Comma-separated payload fields. */
    uint8_t field_count;                 /**< Number of valid entries in fields. */
} ekt_frame_t;

typedef struct {
    char buf[EKT_FRAME_MAX_LEN];
    size_t len;
    bool overflow; /**< Set when a frame was discarded mid-parse. */
} ekt_frame_parser_t;

void ekt_frame_parser_init(ekt_frame_parser_t *p);

/**
 * Feed one received byte.
 *
 * Safe to call from a UART interrupt: no allocation, bounded work, and the
 * only state is a fixed-size buffer plus a length.
 *
 * @return EKT_FRAME_OK when `out` has been filled, otherwise a reason why not.
 */
ekt_frame_status_t ekt_frame_feed(ekt_frame_parser_t *p, char byte, ekt_frame_t *out);

/**
 * Build a frame into `out` (NUL-terminated, no STX/ETX included).
 *
 * @param type      Non-empty frame type token.
 * @param n_fields  Number of entries in `fields`.
 * @return number of characters written (excluding NUL), or -1 on bad input.
 */
int ekt_frame_format(char *out, size_t out_len, const char *type,
                     const char *const *fields, uint8_t n_fields);

/** XOR checksum of a NUL-terminated string, as used by the wire format. */
uint8_t ekt_frame_checksum(const char *payload);

#ifdef __cplusplus
}
#endif

#endif /* EKT_FRAME_H */
