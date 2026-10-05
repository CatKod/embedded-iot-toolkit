# embedded-iot-toolkit

Reusable, portable C building blocks for embedded + IoT firmware.

Every module here was written for a real project, then generalised so the next
project inherits it instead of rewriting it. The core is **pure C99 with no
vendor headers**, which means it compiles and unit-tests on a PC in under a
second, and drops into STM32, ESP32, or anything else with a thin port layer.

```
┌──────────────────────────────────────────────────────────┐
│  Your application        (App / FSM / business logic)    │
├──────────────────────────────────────────────────────────┤
│  embedded-iot-toolkit    frame · can · timer · sensors   │
├──────────────────────────────────────────────────────────┤
│  Port layer              ekt_port.h → one impl per MCU   │
├──────────────────────────────────────────────────────────┤
│  HAL / SDK               STM32 HAL · ESP-IDF · POSIX     │
└──────────────────────────────────────────────────────────┘
```

## Why this exists

Most firmware projects start by solving the same four problems again:

1. *How do I schedule periodic work without `HAL_Delay()` blocking everything?*
2. *How do I frame a message between two microcontrollers and detect corruption?*
3. *How do I pack a 29-bit CAN identifier from named fields?*
4. *How do I read a sensor and know the reading is fresh rather than stale?*

Those four answers live here. They are small, tested, and portable.

## Modules

| Module | What it does |
|---|---|
| [`ekt_soft_timer`](include/ekt/ekt_soft_timer.h) | Non-blocking periodic timer bank driven by a 1 kHz tick. Catches up correctly if the scheduler is starved. |
| [`ekt_frame`](include/ekt/ekt_frame.h) | ASCII framed serial protocol with XOR checksum, byte-at-a-time ISR-safe parsing, and automatic resync after line noise. |
| [`ekt_can_codec`](include/ekt/ekt_can_codec.h) | CAN 2.0B 29-bit identifier pack/unpack with field-range validation, plus a bit-timing solver (prescaler / TSEG1 / TSEG2) for a target bitrate and sample point. |
| [`ekt_sensor`](include/ekt/ekt_sensor.h) | DHT11 decode with checksum + range validation and a minimum sampling interval, RC522 UID comparison, and a 74HC595 shadow register. |
| [`ekt_port`](include/ekt/ekt_port.h) | The only header that hardware is allowed behind. |

### Design rules this library follows

- **Never block.** If an operation can take time, it returns a status and the
  caller decides. There is not a single `delay` in the core.
- **Validate every input at the boundary.** A field that does not fit its width,
  a checksum that does not match, a humidity reading outside the sensor's
  range — all rejected loudly, none silently accepted.
- **Never return stale data.** Sensor reads carry a status; a missed deadline
  yields an error, not the previous value.
- **Zero dynamic allocation.** Fixed buffers only, so nothing allocates from a
  heap you may not have.
- **Warnings are errors.** The test suite builds under
  `-Wall -Wextra -Wpedantic -Werror`.

## Quick start

```c
#include "ekt/ekt_soft_timer.h"
#include "ekt/ekt_frame.h"

static ekt_timer_t timers[4];
static ekt_frame_parser_t rx;

/* 1 kHz hardware tick */
void SysTick_Handler(void)
{
    ekt_timer_tick(timers, 4U, 1U);
}

void app_init(void)
{
    ekt_timer_init(timers, 4U);
    ekt_timer_start(timers, 0U, 1000U);   /* heartbeat every second */
    ekt_frame_parser_init(&rx);
}

void app_loop(void)
{
    if (ekt_timer_expired(timers, 0U)) {
        /* publish state */
    }
}

/* Call for every byte received; safe from a UART ISR. */
void on_uart_rx(char byte)
{
    ekt_frame_t frame;
    switch (ekt_frame_feed(&rx, byte, &frame)) {
        case EKT_FRAME_OK:    handle(&frame); break;
        case EKT_FRAME_BAD_CRC: /* drop it, resyncs automatically */ break;
        default: break;
    }
}
```

## Wire format

```
<STX>TYPE,field1,field2,...*CS<ETX>
```

- `STX = 0x02`, `ETX = 0x03`
- `CS` = XOR of every byte of `TYPE,field1,...`, as two uppercase hex digits
- Human-readable on a logic analyser, trivially parsed in Python on the far side

```
0x02 44 41 54 41 2C 32 38 2E 35 2C 36 35 2A 33 44 0x03
 STX  D  A  T  A  ,  2  8  .  5  ,  6  5  *  3  D ETX
      └──────────── "DATA,28.5,65" ───────────┘ └─ XOR ─┘
```

## Running the tests

```bat
:: Windows
run_tests.bat
```

```sh
# Linux / macOS
./run_tests.sh
```

89 assertions across 6 groups, no external dependencies:

```
[soft timer]      basic · catch-up-after-starvation · argument validation
[frame protocol]  roundtrip · delimiters · bad checksum · resync ·
                  empty type · oversized · overflow
[CAN codec]       id roundtrip · out-of-range fields · standard-frame
                  rejection · 125 kbit/s timing · 500 kbit/s timing ·
                  invalid input
[sensors]         DHT11 happy path · negative temperature · minimum
                  interval · bad checksum · no response · implausible
                  reading · RFID compare · shift-register shadow
```

## Porting to a new target

1. Copy `port/posix/ekt_port_posix.c` as a starting point.
2. Implement the four functions in `ekt_port.h` using your HAL.
3. Build `src/*.c` plus your port file; add `include/` to the include path.

The core never needs to change.

## Integration

Drop the folder into your project, or add it as a submodule:

```bash
git submodule add https://github.com/CatKod/embedded-iot-toolkit 3rdparty/ekt
```

Then in CMake:

```cmake
target_include_directories(app PRIVATE 3rdparty/ekt/include)
target_sources(app PRIVATE
    3rdparty/ekt/src/ekt_soft_timer.c
    3rdparty/ekt/src/ekt_frame.c
    3rdparty/ekt/src/ekt_can_codec.c
    3rdparty/ekt/src/ekt_sensor.c
)
```

## License

MIT — use it in commercial firmware without attribution obligations beyond
keeping the notice.
