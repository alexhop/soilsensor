#!/bin/bash
# Polls for the ESP32C6 USB port and immediately uploads firmware using esptool
# This is faster than 'pio run -t upload' because it skips the build check.
# Usage: ./flash-on-wake.sh [env]     (env defaults to soil-sensor)

ENV="${1:-soil-sensor}"
ESPTOOL="$HOME/.platformio/penv/bin/esptool.py"
FIRMWARE=".pio/build/$ENV/firmware.bin"
BOOTLOADER=".pio/build/$ENV/bootloader.bin"
PARTITIONS=".pio/build/$ENV/partitions.bin"
PORT_PATTERN="/dev/cu.usbmodem*"

if [ ! -f "$FIRMWARE" ]; then
    echo "ERROR: $FIRMWARE not found — run 'pio run -e $ENV' first"
    exit 1
fi

echo "Watching for ESP32C6 USB port..."
echo "Try pressing reset, or wait for the next wake cycle."
echo ""

while true; do
    PORT=$(ls $PORT_PATTERN 2>/dev/null | head -1)
    if [ -n "$PORT" ]; then
        echo ">>> Port found: $PORT"
        echo ">>> Flashing immediately with esptool..."
        $ESPTOOL --chip esp32c6 --port "$PORT" --baud 921600 \
            --before default_reset --after hard_reset \
            write_flash -z \
            0x0 "$BOOTLOADER" \
            0x8000 "$PARTITIONS" \
            0x10000 "$FIRMWARE"
        EXIT=$?
        if [ $EXIT -eq 0 ]; then
            echo ""
            echo ">>> Upload succeeded! Deep sleep is now disabled when USB is connected."
            echo ">>> You can run 'pio device monitor' to see serial output."
            break
        else
            echo ""
            echo ">>> Upload failed (exit $EXIT). Will keep watching..."
            sleep 2
            continue
        fi
    fi
    sleep 0.1
done
