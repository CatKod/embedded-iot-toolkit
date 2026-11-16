/*
 * CanCodec - Pack and unpack a CAN 2.0B extended identifier.
 *
 * The codec is pure logic with no HAL dependency, so the same identifier
 * layout can be unit-tested on a PC and reused across STM32H7, STM32G4 and
 * ESP32.
 *
 * SPDX-License-Identifier: MIT
 */
#include <ekt_can_codec.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("ekt_can_codec: identifier pack / unpack / bit timing");
    Serial.println();

    /* --- Pack a 29-bit identifier from named fields --- */
    ekt_can_id_t tx = {
        .priority    = 1U,
        .command     = 0x2AU,
        .target_type = 3U,
        .target_id   = 17U,
        .source_id   = 5U,
        .reserved    = 0U,
    };

    uint32_t id = ekt_can_id_pack(&tx);
    Serial.print("packed id = 0x");
    Serial.println(id, HEX);

    /* --- Unpack it back --- */
    ekt_can_id_t rx;
    if (ekt_can_id_unpack(id, &rx)) {
        Serial.print("unpacked: prio=");
        Serial.print(rx.priority);
        Serial.print(" cmd=0x");
        Serial.print(rx.command, HEX);
        Serial.print(" type=");
        Serial.print(rx.target_type);
        Serial.print(" target=");
        Serial.print(rx.target_id);
        Serial.print(" source=");
        Serial.println(rx.source_id);
    } else {
        Serial.println("unpack failed: not an extended frame");
    }

    /* --- A standard 11-bit identifier --- */
    Serial.print("standard id = 0x");
    Serial.println(ekt_can_id_std(5U, 0x2AU), HEX);

    /* --- Bit timing solver for 500 kbit/s at 75 % sample point --- */
    uint16_t prescaler;
    uint16_t tseg1;
    uint16_t tseg2;
    if (ekt_can_bit_timing(8000000U, 500000U, 0.75F, &prescaler, &tseg1, &tseg2)) {
        Serial.print("bit timing 500 kbit/s: BRP-1=");
        Serial.print(prescaler);
        Serial.print(" TSEG1-1=");
        Serial.print(tseg1);
        Serial.print(" TSEG2-1=");
        Serial.println(tseg2);
    } else {
        Serial.println("bit timing: no exact integer solution");
    }
}

void loop()
{
}
