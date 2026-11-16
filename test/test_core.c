/*
 * test_core.c - Unit tests for the portable core.
 *
 * Build and run:  run_tests.bat   (Windows)  |  ./run_tests.sh  (POSIX)
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>

#include "ekt_can_codec.h"
#include "ekt_frame.h"
#include "ekt_sensor.h"
#include "ekt_soft_timer.h"
#include "ekt_test.h"

/* Test hook implemented in the POSIX port. */
extern void ekt_test_set_dht_script(const uint8_t *bytes, size_t len,
                                    bool fail_next);

/* ------------------------------------------------------------ soft timer --- */

static void test_timer_basic(void)
{
    ekt_timer_t timers[4];

    ekt_timer_init(timers, 4U);
    ekt_timer_start(timers, 0U, 100U);

    ekt_timer_tick(timers, 4U, 50U);
    EKT_CHECK(!ekt_timer_expired(timers, 0U));

    ekt_timer_tick(timers, 4U, 50U);
    EKT_CHECK(ekt_timer_expired(timers, 0U));
    /* A pending expiry must be reported exactly once. */
    EKT_CHECK(!ekt_timer_expired(timers, 0U));

    ekt_timer_stop(timers, 0U);
    ekt_timer_tick(timers, 4U, 500U);
    EKT_CHECK(!ekt_timer_expired(timers, 0U));
}

static void test_timer_catches_up_after_gap(void)
{
    ekt_timer_t timers[2];

    ekt_timer_init(timers, 2U);
    ekt_timer_start(timers, 1U, 10U);

    /* Simulates the scheduler being starved for 35 ms: the timer must still be
     * considered due rather than silently skipping the interval. */
    ekt_timer_tick(timers, 2U, 35U);
    EKT_CHECK(ekt_timer_expired(timers, 1U));
}

static void test_timer_rejects_bad_args(void)
{
    ekt_timer_t timers[2];

    ekt_timer_init(timers, 2U);
    ekt_timer_start(timers, 0U, 0U); /* zero period is refused */
    ekt_timer_tick(timers, 2U, 1000U);
    EKT_CHECK(!ekt_timer_expired(timers, 0U));

    ekt_timer_start(timers, 9U, 100U); /* out of range index */
    ekt_timer_tick(timers, 2U, 1000U);
    EKT_CHECK(!ekt_timer_expired(timers, 9U));
}

/* ----------------------------------------------------------------- frame --- */

/* Push a whole NUL-terminated string through the parser. */
static ekt_frame_status_t feed_str(ekt_frame_parser_t *p, const char *s,
                                   ekt_frame_t *out)
{
    size_t i;

    for (i = 0U; s[i] != '\0'; i++) {
        ekt_frame_status_t st = ekt_frame_feed(p, s[i], out);
        if (st != EKT_FRAME_EMPTY) {
            return st;
        }
    }
    return EKT_FRAME_EMPTY;
}

static void test_frame_roundtrip(void)
{
    ekt_frame_parser_t p;
    ekt_frame_t out;
    char wire[128];
    const char *fields[3];
    ekt_frame_status_t st;

    ekt_frame_parser_init(&p);
    memset(&out, 0, sizeof(out));

    fields[0] = "28.5";
    fields[1] = "65";
    fields[2] = "NORMAL";
    EKT_CHECK(ekt_frame_format(wire, sizeof(wire), "DATA", fields, 3U) > 0);

    /* Exactly what goes on the wire: STX + payload + checksum + ETX. */
    st = feed_str(&p, "\x02", &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_EMPTY);
    st = feed_str(&p, wire, &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_EMPTY);
    st = feed_str(&p, "\x03", &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_OK);
    EKT_CHECK_EQ_STR(out.type, "DATA");
    EKT_CHECK_EQ_U32(out.field_count, 3U);
    EKT_CHECK_EQ_STR(out.fields[0], "28.5");
    EKT_CHECK_EQ_STR(out.fields[1], "65");
    EKT_CHECK_EQ_STR(out.fields[2], "NORMAL");
}

static void test_frame_full_delimiters(void)
{
    ekt_frame_parser_t p;
    ekt_frame_t out;
    char wire[128];
    const char *fields[2];
    ekt_frame_status_t st;

    ekt_frame_parser_init(&p);
    fields[0] = "1";
    fields[1] = "2";
    (void)ekt_frame_format(wire, sizeof(wire), "EVT", fields, 2U);

    /* Wrapped in the real STX/ETX delimiters, as they appear on the wire. */
    st = feed_str(&p, "\x02", &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_EMPTY);
    st = feed_str(&p, wire, &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_EMPTY);
    st = feed_str(&p, "\x03", &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_OK);
    EKT_CHECK_EQ_STR(out.type, "EVT");
    EKT_CHECK_EQ_U32(out.field_count, 2U);
}

static void test_frame_rejects_bad_checksum(void)
{
    ekt_frame_parser_t p;
    ekt_frame_t out;
    char wire[128];
    const char *fields[1];
    ekt_frame_status_t st;

    ekt_frame_parser_init(&p);
    fields[0] = "1";
    (void)ekt_frame_format(wire, sizeof(wire), "CMD", fields, 1U);

    /* Corrupt the checksum nibble; the frame must be dropped, not acted on. */
    wire[strlen(wire) - 1U] = (wire[strlen(wire) - 1U] == '0') ? '1' : '0';

    st = feed_str(&p, "\x02", &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_EMPTY);
    st = feed_str(&p, wire, &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_EMPTY);
    st = feed_str(&p, "\x03", &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_BAD_CRC);
    EKT_CHECK_EQ_U32(out.field_count, 0U);
}

static void test_frame_resyncs_after_garbage(void)
{
    ekt_frame_parser_t p;
    ekt_frame_t out;
    char wire[128];
    const char *fields[1];
    ekt_frame_status_t st;

    ekt_frame_parser_init(&p);
    fields[0] = "OK";
    (void)ekt_frame_format(wire, sizeof(wire), "ACK", fields, 1U);

    /* Line noise before the real frame must not break parsing. */
    st = feed_str(&p, "garbage", &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_EMPTY);

    (void)feed_str(&p, "\x02", &out);
    (void)feed_str(&p, wire, &out);
    st = feed_str(&p, "\x03", &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_OK);
    EKT_CHECK_EQ_STR(out.type, "ACK");
}

static void test_frame_rejects_empty_type(void)
{
    ekt_frame_parser_t p;
    ekt_frame_t out;
    char wire[128];
    char payload[8];
    ekt_frame_status_t st;

    /* ",5*XX" - checksum is valid but there is no type token. */
    (void)snprintf(payload, sizeof(payload), ",5");
    (void)snprintf(wire, sizeof(wire), "%s*%02X", payload,
                   ekt_frame_checksum(payload));

    ekt_frame_parser_init(&p);
    (void)feed_str(&p, "\x02", &out);
    (void)feed_str(&p, wire, &out);
    st = feed_str(&p, "\x03", &out);
    EKT_CHECK_EQ_U32(st, EKT_FRAME_BAD_TYPE);
}

static void test_frame_drops_oversized(void)
{
    ekt_frame_parser_t p;
    ekt_frame_t out;
    size_t i;
    ekt_frame_status_t st = EKT_FRAME_EMPTY;

    ekt_frame_parser_init(&p);
    (void)feed_str(&p, "\x02", &out);
    for (i = 0U; i < (EKT_FRAME_MAX_LEN + 32U); i++) {
        st = ekt_frame_feed(&p, 'X', &out);
        if (st != EKT_FRAME_EMPTY) {
            break;
        }
    }
    EKT_CHECK_EQ_U32(st, EKT_FRAME_TOO_LONG);
    EKT_CHECK(p.overflow);
}

static void test_frame_rejects_overflow(void)
{
    char buf[16];

    EKT_CHECK(ekt_frame_format(buf, sizeof(buf), "TYPE_WITH_A_VERY_LONG_NAME",
                               NULL, 0U) < 0);
    EKT_CHECK(ekt_frame_format(buf, sizeof(buf), "", NULL, 0U) < 0);
    EKT_CHECK(ekt_frame_format(buf, sizeof(buf), NULL, NULL, 0U) < 0);
}

/* -------------------------------------------------------------- CAN codec --- */

static void test_can_id_roundtrip(void)
{
    ekt_can_id_t in;
    ekt_can_id_t out;

    in.priority    = 1U;
    in.command     = 42U;
    in.target_type = 5U;
    in.target_id   = 123U;
    in.source_id   = 17U;
    in.reserved    = 9U;

    EKT_CHECK(ekt_can_id_pack(&in) != 0xFFFFFFFFU);
    EKT_CHECK(ekt_can_id_unpack(ekt_can_id_pack(&in), &out));

    EKT_CHECK_EQ_U32(out.priority, in.priority);
    EKT_CHECK_EQ_U32(out.command, in.command);
    EKT_CHECK_EQ_U32(out.target_type, in.target_type);
    EKT_CHECK_EQ_U32(out.target_id, in.target_id);
    EKT_CHECK_EQ_U32(out.source_id, in.source_id);
    EKT_CHECK_EQ_U32(out.reserved, in.reserved);
}

static void test_can_id_rejects_out_of_range(void)
{
    ekt_can_id_t in;

    in.priority    = 0U;
    in.command     = 0U;
    in.target_type = 0U;
    in.target_id   = 0U;
    in.source_id   = 0U;
    in.reserved    = 0U;

    in.command = 64U; /* 6-bit field, max 63 */
    EKT_CHECK_EQ_U32(ekt_can_id_pack(&in), 0xFFFFFFFFU);

    in.command    = 0U;
    in.target_id  = 128U; /* 7-bit field, max 127 */
    EKT_CHECK_EQ_U32(ekt_can_id_pack(&in), 0xFFFFFFFFU);

    in.target_id  = 0U;
    in.priority   = 4U; /* 2-bit field, max 3 */
    EKT_CHECK_EQ_U32(ekt_can_id_pack(&in), 0xFFFFFFFFU);
}

static void test_can_id_rejects_standard_frame(void)
{
    ekt_can_id_t out;

    /* EFF flag (bit 31) clear => standard 11-bit frame, not ours. */
    EKT_CHECK(!ekt_can_id_unpack(0x123U, &out));
    EKT_CHECK(!ekt_can_id_unpack(0x7FFFFFFFU, &out));
    /* EFF set => a valid extended ID, all fields at maximum. */
    EKT_CHECK(ekt_can_id_unpack(0xFFFFFFFFU, &out));
}

static void test_can_bit_timing_125k(void)
{
    uint16_t brp, tseg1, tseg2;
    uint32_t tq_per_bit;
    uint32_t effective_bitrate;

    /* 80 MHz FDCAN kernel clock, 125 kbit/s, 75 % sample point. */
    EKT_CHECK(ekt_can_bit_timing(80000000U, 125000U, 0.75f, &brp, &tseg1, &tseg2));

    /* Registers hold (TQ count - 1) for both segments, plus the implicit
     * SYNC bit, so time quanta per bit = 1 + (TSEG1+1) + (TSEG2+1). */
    tq_per_bit = 1U + ((uint32_t)tseg1 + 1U) + ((uint32_t)tseg2 + 1U);

    EKT_CHECK_EQ_U32(80000000U % (tq_per_bit * (uint32_t)brp), 0U);
    effective_bitrate = 80000000U / (tq_per_bit * (uint32_t)brp);
    EKT_CHECK_EQ_U32(effective_bitrate, 125000U);

    /* 80 MHz / (20 tq * 4) = 1 MHz tq, 20 tq per bit => 50 kbit/s... verify
     * the solver actually landed on a legal arbitration-phase width. */
    EKT_CHECK(tq_per_bit >= 8U && tq_per_bit <= 25U);
    EKT_CHECK(brp >= 1U && brp <= 1024U);
}

static void test_can_bit_timing_500k(void)
{
    uint16_t brp, tseg1, tseg2;

    EKT_CHECK(ekt_can_bit_timing(48000000U, 500000U, 0.80f, &brp, &tseg1, &tseg2));
    EKT_CHECK(brp >= 1U && brp <= 1024U);
    EKT_CHECK(tseg1 >= 1U);
    EKT_CHECK(tseg2 >= 1U);
}

static void test_can_bit_timing_rejects_bad_input(void)
{
    uint16_t brp, tseg1, tseg2;

    EKT_CHECK(!ekt_can_bit_timing(0U, 125000U, 0.75f, &brp, &tseg1, &tseg2));
    EKT_CHECK(!ekt_can_bit_timing(80000000U, 0U, 0.75f, &brp, &tseg1, &tseg2));
    EKT_CHECK(!ekt_can_bit_timing(80000000U, 125000U, 0.0f, &brp, &tseg1, &tseg2));
    EKT_CHECK(!ekt_can_bit_timing(80000000U, 125000U, 1.0f, &brp, &tseg1, &tseg2));
    EKT_CHECK(!ekt_can_bit_timing(80000000U, 125000U, 0.75f, NULL, &tseg1, &tseg2));
}

/* ----------------------------------------------------------------- sensor --- */

static void test_dht11_happy_path(void)
{
    ekt_dht11_t dev;
    int16_t temperature = 0;
    uint8_t humidity    = 0;
    /* RH int = 65, RH dec = 0, T int = 28, T dec = 5, checksum = 98. */
    const uint8_t script[5] = {65U, 0U, 28U, 5U, 98U};

    ekt_test_set_dht_script(script, sizeof(script), false);
    ekt_dht11_init(&dev, 1U, 1000U);

    EKT_CHECK_EQ_U32(ekt_dht11_read(&dev, 1000U, &temperature, &humidity),
                     EKT_SENSOR_OK);
    EKT_CHECK_EQ_U32(temperature, 285); /* 28.5 C, reported in tenths */
    EKT_CHECK_EQ_U32(humidity, 65);
    EKT_CHECK_EQ_U32(dev.consecutive_failures, 0U);
}

static void test_dht11_negative_temperature(void)
{
    ekt_dht11_t dev;
    int16_t temperature = 0;
    uint8_t humidity    = 0;
    /* Sign bit set on the temperature MSB, magnitude 0x0005 => -5.0 C.
     * Checksum = 40 + 0 + 0x80 + 5 = 173. */
    const uint8_t script[5] = {40U, 0U, 0x80U, 5U, 173U};

    ekt_test_set_dht_script(script, sizeof(script), false);
    ekt_dht11_init(&dev, 1U, 1000U);

    EKT_CHECK_EQ_U32(ekt_dht11_read(&dev, 1000U, &temperature, &humidity),
                     EKT_SENSOR_OK);
    /* The sign bit must be stripped before scaling. */
    EKT_CHECK_EQ_U32(temperature, -50);
}

static void test_dht11_respects_min_interval(void)
{
    ekt_dht11_t dev;
    int16_t temperature = 0;
    uint8_t humidity    = 0;
    const uint8_t script[10] = {65U, 0U, 28U, 5U, 98U,
                                65U, 0U, 28U, 5U, 98U};

    ekt_test_set_dht_script(script, sizeof(script), false);
    ekt_dht11_init(&dev, 1U, 1000U);

    EKT_CHECK_EQ_U32(ekt_dht11_read(&dev, 1000U, &temperature, &humidity),
                     EKT_SENSOR_OK);
    EKT_CHECK_EQ_U32(temperature, 285);
    /* Polling again before the conversion completes must be refused. */
    EKT_CHECK_EQ_U32(ekt_dht11_read(&dev, 1100U, &temperature, &humidity),
                     EKT_SENSOR_TIMEOUT);
    EKT_CHECK_EQ_U32(ekt_dht11_read(&dev, 2200U, &temperature, &humidity),
                     EKT_SENSOR_OK);
}

static void test_dht11_detects_bad_checksum(void)
{
    ekt_dht11_t dev;
    int16_t temperature = 0;
    uint8_t humidity    = 0;
    /* Deliberately wrong checksum. */
    const uint8_t script[5] = {65U, 0U, 28U, 5U, 0x00U};

    ekt_test_set_dht_script(script, sizeof(script), false);
    ekt_dht11_init(&dev, 1U, 1000U);

    EKT_CHECK_EQ_U32(ekt_dht11_read(&dev, 1000U, &temperature, &humidity),
                     EKT_SENSOR_CRC_FAIL);
    EKT_CHECK_EQ_U32(dev.consecutive_failures, 1U);
}

static void test_dht11_detects_no_response(void)
{
    ekt_dht11_t dev;
    int16_t temperature = 0;
    uint8_t humidity    = 0;
    const uint8_t script[5] = {65U, 0U, 28U, 5U, 98U};

    ekt_test_set_dht_script(script, sizeof(script), true);
    ekt_dht11_init(&dev, 1U, 1000U);

    EKT_CHECK_EQ_U32(ekt_dht11_read(&dev, 1000U, &temperature, &humidity),
                     EKT_SENSOR_TIMEOUT);
    EKT_CHECK_EQ_U32(dev.consecutive_failures, 1U);
}

static void test_dht11_rejects_implausible_humidity(void)
{
    ekt_dht11_t dev;
    int16_t temperature = 0;
    uint8_t humidity    = 0;
    /* 5 %RH is below the DHT11's 20 %RH floor. */
    const uint8_t script[5] = {5U, 0U, 28U, 5U, 38U};
    ekt_test_set_dht_script(script, sizeof(script), false);
    ekt_dht11_init(&dev, 1U, 1000U);

    EKT_CHECK_EQ_U32(ekt_dht11_read(&dev, 1000U, &temperature, &humidity),
                     EKT_SENSOR_RANGE);
}

static void test_rfid_uid_compare(void)
{
    ekt_rfid_state_t st;
    const uint8_t good[4] = {0xDEU, 0xADU, 0xBEU, 0xEFU};
    const uint8_t bad[4]  = {0xDEU, 0xADU, 0xBEU, 0xEEU};

    memset(&st, 0, sizeof(st));
    EKT_CHECK(!ekt_rfid_present(&st));

    memcpy(st.uid, good, sizeof(good));
    st.uid_len = 4U;
    EKT_CHECK(ekt_rfid_present(&st));
    EKT_CHECK(ekt_rfid_uid_equal(&st, good, 4U));
    EKT_CHECK(!ekt_rfid_uid_equal(&st, bad, 4U));
    /* A length mismatch must not compare equal, whatever the bytes are. */
    EKT_CHECK(!ekt_rfid_uid_equal(&st, good, 7U));
}

static void test_shift_register_shadow(void)
{
    ekt_shift74hc595_t dev;

    ekt_shift74hc595_init(&dev, 8U);
    EKT_CHECK(!ekt_shift74hc595_dirty(&dev));

    ekt_shift74hc595_set(&dev, 0U, true);
    EKT_CHECK(ekt_shift74hc595_dirty(&dev));
    EKT_CHECK_EQ_U32(dev.shadow, 1U);

    ekt_shift74hc595_set(&dev, 7U, true);
    EKT_CHECK_EQ_U32(dev.shadow, 0x81U);

    ekt_shift74hc595_clear(&dev, 0U);
    EKT_CHECK_EQ_U32(dev.shadow, 0x80U);

    /* Out-of-range writes are ignored rather than corrupting other bits. */
    ekt_shift74hc595_set(&dev, 8U, true);
    EKT_CHECK_EQ_U32(dev.shadow, 0x80U);

    /* Once the hardware catches up, the register is clean again. */
    dev.latched = dev.shadow;
    EKT_CHECK(!ekt_shift74hc595_dirty(&dev));
}

/* -------------------------------------------------------------------- main --- */

int main(void)
{
    printf("embedded-iot-toolkit :: core unit tests\n\n");

    printf("[soft timer]\n");
    EKT_RUN(test_timer_basic);
    EKT_RUN(test_timer_catches_up_after_gap);
    EKT_RUN(test_timer_rejects_bad_args);

    printf("\n[frame protocol]\n");
    EKT_RUN(test_frame_roundtrip);
    EKT_RUN(test_frame_full_delimiters);
    EKT_RUN(test_frame_rejects_bad_checksum);
    EKT_RUN(test_frame_resyncs_after_garbage);
    EKT_RUN(test_frame_rejects_empty_type);
    EKT_RUN(test_frame_drops_oversized);
    EKT_RUN(test_frame_rejects_overflow);

    printf("\n[CAN codec]\n");
    EKT_RUN(test_can_id_roundtrip);
    EKT_RUN(test_can_id_rejects_out_of_range);
    EKT_RUN(test_can_id_rejects_standard_frame);
    EKT_RUN(test_can_bit_timing_125k);
    EKT_RUN(test_can_bit_timing_500k);
    EKT_RUN(test_can_bit_timing_rejects_bad_input);

    printf("\n[sensors]\n");
    EKT_RUN(test_dht11_happy_path);
    EKT_RUN(test_dht11_negative_temperature);
    EKT_RUN(test_dht11_respects_min_interval);
    EKT_RUN(test_dht11_detects_bad_checksum);
    EKT_RUN(test_dht11_detects_no_response);
    EKT_RUN(test_dht11_rejects_implausible_humidity);
    EKT_RUN(test_rfid_uid_compare);
    EKT_RUN(test_shift_register_shadow);

    printf("\n----------------------------------------\n");
    printf("%d checks, %d failure(s)\n", ekt_test_checks, ekt_test_failures);
    return (ekt_test_failures == 0) ? 0 : 1;
}
