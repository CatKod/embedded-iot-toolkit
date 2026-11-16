/*
 * ekt_frame.c - ASCII framed serial protocol.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ekt_frame.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define EKT_STX ((char)0x02)
#define EKT_ETX ((char)0x03)
#define EKT_CRC_SEP '*'

uint8_t ekt_frame_checksum(const char *payload)
{
    uint8_t cs = 0U;
    size_t i;

    for (i = 0U; payload[i] != '\0'; i++) {
        cs = (uint8_t)(cs ^ (uint8_t)payload[i]);
    }
    return cs;
}

void ekt_frame_parser_init(ekt_frame_parser_t *p)
{
    if (p == NULL) {
        return;
    }
    p->len      = 0U;
    p->overflow = false;
    p->buf[0]   = '\0';
}

int ekt_frame_format(char *out, size_t out_len, const char *type,
                     const char *const *fields, uint8_t n_fields)
{
    char payload[EKT_FRAME_MAX_LEN];
    size_t used;
    uint8_t i;
    int written;

    if ((out == NULL) || (out_len == 0U) || (type == NULL) || (type[0] == '\0')) {
        return -1;
    }
    if ((fields == NULL) && (n_fields > 0U)) {
        return -1;
    }

    used = (size_t)snprintf(payload, sizeof(payload), "%s", type);
    for (i = 0U; i < n_fields; i++) {
        int n = snprintf(&payload[used], sizeof(payload) - used, ",%s",
                         (fields[i] != NULL) ? fields[i] : "");
        if ((n < 0) || ((size_t)n >= (sizeof(payload) - used))) {
            return -1;
        }
        used += (size_t)n;
    }
    payload[used] = '\0';

    written = snprintf(out, out_len, "%s%c%02X", payload, EKT_CRC_SEP,
                       ekt_frame_checksum(payload));
    if ((written < 0) || ((size_t)written >= out_len)) {
        return -1;
    }
    return written;
}

static bool ekt_is_hex(char c)
{
    return ((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'F')) ||
           ((c >= 'a') && (c <= 'f'));
}

static uint8_t ekt_hexval(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return (uint8_t)(c - '0');
    }
    if ((c >= 'A') && (c <= 'F')) {
        return (uint8_t)(c - 'A' + 10);
    }
    return (uint8_t)(c - 'a' + 10);
}

/* In-place split of "TYPE,a,b" into the frame struct. */
static ekt_frame_status_t ekt_split(char *body, ekt_frame_t *out)
{
    char *sep;
    char *cursor;

    out->type[0]     = '\0';
    out->field_count = 0U;

    /* Terminate the type token. `sep` is left pointing at the NUL we just
     * wrote, so step over it to reach the first field. */
    sep = body;
    while ((*sep != '\0') && (*sep != ',')) {
        sep++;
    }
    if (sep == body) {
        return EKT_FRAME_BAD_TYPE;
    }
    *sep = '\0';
    (void)strncpy(out->type, body, sizeof(out->type) - 1U);
    out->type[sizeof(out->type) - 1U] = '\0';

    /* Walk the fields, terminating each one in turn. Each iteration copies one
     * field out, then steps over the separator that ended it. */
    cursor = sep + 1;
    while (out->field_count < 16U) {
        char  *start = cursor;
        size_t len;

        while ((*cursor != '\0') && (*cursor != ',')) {
            cursor++;
        }
        len = (size_t)(cursor - start);

        if (len >= sizeof(out->fields[0])) {
            len = sizeof(out->fields[0]) - 1U;
        }
        (void)memcpy(out->fields[out->field_count], start, len);
        out->fields[out->field_count][len] = '\0';
        out->field_count++;

        if (*cursor != ',') {
            break; /* end of payload */
        }
        *cursor = '\0';
        cursor++;
    }
    return EKT_FRAME_OK;
}

ekt_frame_status_t ekt_frame_feed(ekt_frame_parser_t *p, char byte, ekt_frame_t *out)
{
    size_t sep;
    size_t body_len;
    uint8_t expected;

    if ((p == NULL) || (out == NULL)) {
        return EKT_FRAME_EMPTY;
    }

    /* Clear the output on every entry so a rejected frame can never leave
     * stale fields behind for the caller to act on. */
    (void)memset(out, 0, sizeof(*out));

    /* --- Sync: ignore everything outside a frame. --- */
    if (p->len == 0U) {
        if (byte != EKT_STX) {
            return EKT_FRAME_EMPTY;
        }
        p->len = 1U;
        return EKT_FRAME_EMPTY;
    }

    if (p->overflow) {
        /* Resync: drop until the next STX after a discarded frame. */
        p->overflow = false;
        p->len      = (byte == EKT_STX) ? 1U : 0U;
        return EKT_FRAME_EMPTY;
    }

    if (byte == EKT_ETX) {
        p->buf[p->len] = '\0';

        /* Layout so far: [STX] body... [*] HH [ETX], where body occupies
         * buf[1 .. sep-1] and the two hex digits sit at sep+1 and sep+2. */
        sep = 1U;
        while ((sep < p->len) && (p->buf[sep] != EKT_CRC_SEP)) {
            sep++;
        }
        if ((sep >= p->len) || ((p->len - sep) != 3U) ||
            (ekt_is_hex(p->buf[sep + 1U]) == false) ||
            (ekt_is_hex(p->buf[sep + 2U]) == false)) {
            p->len = 0U;
            return EKT_FRAME_BAD_CRC;
        }

        /* Read the checksum digits before shifting, because the memmove below
         * overwrites the '*' position. */
        expected = (uint8_t)((ekt_hexval(p->buf[sep + 1U]) << 4) |
                             ekt_hexval(p->buf[sep + 2U]));

        /* Slide the body down over the STX so it starts at buf[0]. */
        body_len = sep - 1U;
        (void)memmove(p->buf, &p->buf[1], body_len);
        p->buf[body_len] = '\0';

        p->len = 0U;

        if (ekt_frame_checksum(p->buf) != expected) {
            return EKT_FRAME_BAD_CRC;
        }
        return ekt_split(p->buf, out);
    }

    if (p->len >= (EKT_FRAME_MAX_LEN - 1U)) {
        p->len      = 0U;
        p->overflow = true;
        return EKT_FRAME_TOO_LONG;
    }

    p->buf[p->len++] = byte;
    return EKT_FRAME_EMPTY;
}
