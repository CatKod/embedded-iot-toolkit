#!/usr/bin/env sh
# ============================================================
#  embedded-iot-toolkit - build and run the host unit tests
# ============================================================
set -e

CC="${CC:-gcc}"
mkdir -p build

echo "Building test_core..."
"$CC" -std=c99 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itest \
  src/ekt_soft_timer.c \
  src/ekt_frame.c \
  src/ekt_can_codec.c \
  src/ekt_sensor.c \
  port/posix/ekt_port_posix.c \
  test/test_core.c \
  -o build/test_core

echo "Running..."
echo
./build/test_core
