/*
 * FramedSerial - Send and receive the ekt_frame ASCII protocol over Serial.
 *
 * Wire format: <STX>TYPE,field1,field2*CS<ETX>
 * The parser is byte-at-a-time and ISR-safe, so it can run directly from
 * the serial interrupt.
 *
 * SPDX-License-Identifier: MIT
 */
#include <ekt_frame.h>

static ekt_frame_parser_t rx;
static char             tx[128];

void setup()
{
    Serial.begin(115200);

    ekt_frame_parser_init(&rx);

    Serial.println("ekt_frame: type 'hello,world' and press enter");
    Serial.println("        the sketch replies with a DATA frame");
}

void loop()
{
    while (Serial.available() > 0) {
        char byte = (char)Serial.read();

        ekt_frame_t frame;
        switch (ekt_frame_feed(&rx, byte, &frame)) {
            case EKT_FRAME_OK: {
                Serial.print("received type=");
                Serial.print(frame.type);
                Serial.print(" fields=");
                Serial.print(frame.field_count);
                for (uint8_t i = 0U; i < frame.field_count; i++) {
                    Serial.print(" [");
                    Serial.print(frame.fields[i]);
                    Serial.print("]");
                }
                Serial.println();

                /* Echo the payload back as a DATA frame. */
                const char *fields[2] = {frame.fields[0], frame.fields[1]};
                int n = ekt_frame_format(tx, sizeof(tx), "DATA", fields,
                                          (frame.field_count > 1U) ? 2U
                                                                  : frame.field_count);
                if (n > 0) {
                    Serial.write((uint8_t)0x02); /* STX */
                    Serial.write(tx);
                    Serial.write((uint8_t)0x03); /* ETX */
                    Serial.println();
                }
                break;
            }

            case EKT_FRAME_BAD_CRC:
                Serial.println("dropped: bad checksum");
                break;

            case EKT_FRAME_TOO_LONG:
                Serial.println("dropped: frame too long, resyncing");
                break;

            default:
                break; /* EKT_FRAME_EMPTY / EKT_FRAME_BAD_TYPE */
        }
    }
}
