#!/usr/bin/env bash
# Build matrix for pico-i2s-usb: builds every supported configuration, and checks
# that each config check stops a bad configuration with its message. A
# configuration is a set of src/audio_config.h settings passed through CMake's
# AUDIO_CONFIG_DEFINES; the source tree is never edited.
#
# Usage: scripts/build-matrix.sh            run every case
#        scripts/build-matrix.sh NAME ...   run only the named cases
#        scripts/build-matrix.sh --list     list the cases
#
# Each case gets a fresh build folder, build/matrix/NAME, and a log,
# build/matrix/NAME.log. The exit status is 1 if any case doesn't match its
# expectation.

set -u

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT="$ROOT/build/matrix"
JOBS=$(nproc 2>/dev/null || echo 4)

# --- The cases ----------------------------------------------------------------
# build_case NAME PLATFORM SETTINGS [TEXT]
# fail_case  NAME PLATFORM SETTINGS EXPECTED-MESSAGE [MESSAGE-THAT-MUST-NOT-APPEAR]
# SETTINGS are NAME=VALUE pairs separated by ';'. TEXT must appear in the built
# firmware (pico-i2s-usb.elf), or, starting with '!', must not: it proves that
# an #if around that code chose the right branch.

NAMES=() PLATFORMS=() SETTINGS=() EXPECTED=() FORBIDDEN=() TEXT=()
build_case() { NAMES+=("$1") PLATFORMS+=("$2") SETTINGS+=("$3") EXPECTED+=("") FORBIDDEN+=("") TEXT+=("${4:-}"); }
fail_case() { NAMES+=("$1") PLATFORMS+=("$2") SETTINGS+=("$3") EXPECTED+=("$4") FORBIDDEN+=("${5:-}") TEXT+=(""); }

# Configurations that must build.
build_case default rp2350 "" "!Generating MCLK"
build_case default-riscv rp2350-riscv ""
build_case debug rp2350 "AUDIO_DEBUG_LOGGING=1" "[Audio Data]"
build_case mclk-pico rp2350 "MCLK_SOURCE=MCLK_SOURCE_PICO" "Generating MCLK"
build_case rate-96k rp2350 "AUDIO_RATES=AUDIO_RATE_96000"
build_case rate-44k1 rp2350 "AUDIO_RATES=AUDIO_RATE_44100"
build_case tin-can-pins rp2350 "PIN_I2S_DIN=18;PIN_I2S_BCLK=19;PIN_I2S_LRCK=20;MCLK_SOURCE=MCLK_SOURCE_PICO;PIN_MCLK=21;PIN_I2C_SDA=16;PIN_I2C_SCL=17"
build_case loose-pins rp2350 "PIN_I2S_BCLK=6;PIN_I2S_LRCK=9;MCLK_SOURCE=MCLK_SOURCE_PICO;PIN_MCLK=15"
build_case console-moved rp2350 "PICO_DEFAULT_UART=1;PICO_DEFAULT_UART_TX_PIN=4;PICO_DEFAULT_UART_RX_PIN=5;PIN_I2S_DIN=0"
build_case buffer-min rp2350 "AUDIO_BUFFER_FRAMES=48"
build_case buffer-max rp2350 "AUDIO_BUFFER_FRAMES=414"

# Configurations that must fail: one case per branch of each check.
fail_case role-typo rp2350 "I2S_ROLE=I2S_ROLE_CONTROLER" \
    "I2S_ROLE must be I2S_ROLE_TARGET or I2S_ROLE_CONTROLLER"
fail_case mclk-typo rp2350 "MCLK_SOURCE=MCLK_SOURCE_PCIO" \
    "MCLK_SOURCE must be MCLK_SOURCE_EXTERNAL or MCLK_SOURCE_PICO"
fail_case codec-typo rp2350 "CODEC=CODEC_TAA5221" \
    "CODEC must be CODEC_NONE, CODEC_SGTL5000 or CODEC_TAA5212"
fail_case no-rate rp2350 "AUDIO_RATES=0" \
    "AUDIO_RATES must list at least one AUDIO_RATE_* value"
fail_case bad-channels rp2350 "AUDIO_CHANNELS=3" \
    "AUDIO_CHANNELS must be 1 or 2"
fail_case zero-frames rp2350 "AUDIO_BUFFER_FRAMES=0" \
    "AUDIO_BUFFER_FRAMES must be at least 1"
fail_case bad-flag rp2350 "MUTE_SWITCH_CUTS_AUDIO=2" \
    "MUTE_SWITCH_CUTS_AUDIO must be 0 or 1"
fail_case pin-range rp2350 "PIN_I2S_DIN=30" \
    "PIN_I2S_DIN must be a GPIO number of this chip"
fail_case pin-range-other rp2350 "PIN_MUTE_BUTTON=30" \
    "PIN_MUTE_BUTTON must be a GPIO number of this chip"
fail_case mclk-wrong-pin rp2350 "MCLK_SOURCE=MCLK_SOURCE_PICO;PIN_MCLK=22" \
    "PIN_MCLK must be a clock-output pin"
fail_case sda-wrong-pin rp2350 "CODEC=CODEC_TAA5212;PIN_I2C_SDA=17;PIN_I2C_SCL=18" \
    "PIN_I2C_SDA must be an I2C SDA pin"
fail_case scl-wrong-block rp2350 "CODEC=CODEC_TAA5212;PIN_I2C_SDA=16;PIN_I2C_SCL=19" \
    "PIN_I2C_SCL must be an SCL pin of the same I2C block as PIN_I2C_SDA"
fail_case pot-not-adc rp2350 "PIN_POT=22" \
    "PIN_POT must be an ADC pin"
fail_case board-pin rp2350 "MCLK_SOURCE=MCLK_SOURCE_PICO;PIN_MCLK=23" \
    "PIN_MCLK is on a GPIO the board itself uses"
fail_case console-pin rp2350 "PIN_I2S_DIN=0" \
    "PIN_I2S_DIN is on the debug console's UART pins"
fail_case shared-pin rp2350 "PIN_I2S_LRCK=10" \
    "PIN_I2S_LRCK uses a GPIO that an earlier pin setting already uses"
fail_case two-rates rp2350 "AUDIO_RATES=AUDIO_RATE_48000|AUDIO_RATE_96000" \
    "CODEC_NONE (a pin-strapped ADC) runs at one rate"
fail_case 44k1-pico-mclk rp2350 "MCLK_SOURCE=MCLK_SOURCE_PICO;AUDIO_RATES=AUDIO_RATE_44100" \
    "AUDIO_RATE_44100 needs MCLK_SOURCE_EXTERNAL"
fail_case async-mclk rp2350 "I2S_ROLE=I2S_ROLE_CONTROLLER" \
    "With I2S_ROLE_CONTROLLER, MCLK_SOURCE can't be MCLK_SOURCE_EXTERNAL"
fail_case mclk-pin-unset rp2350 "MCLK_SOURCE=MCLK_SOURCE_PICO;PIN_MCLK=PIN_NONE" \
    "MCLK_SOURCE_PICO needs PIN_MCLK"
fail_case i2c-pins-unset rp2350 "CODEC=CODEC_TAA5212" \
    "CODEC needs PIN_I2C_SDA and PIN_I2C_SCL"
fail_case buffer-too-large rp2350 "AUDIO_BUFFER_FRAMES=415" \
    "AUDIO_BUFFER_FRAMES is too large for the USB FIFO"
fail_case buffer-too-small rp2350 "AUDIO_BUFFER_FRAMES=47" \
    "AUDIO_BUFFER_FRAMES is too small"
fail_case mono rp2350 "AUDIO_CHANNELS=1" \
    "AUDIO_CHANNELS 1 (mono) is not implemented yet"
fail_case controller rp2350 "I2S_ROLE=I2S_ROLE_CONTROLLER;MCLK_SOURCE=MCLK_SOURCE_PICO" \
    "I2S_ROLE_CONTROLLER is not implemented yet"
fail_case sgtl5000 rp2350 "CODEC=CODEC_SGTL5000;PIN_I2C_SDA=16;PIN_I2C_SCL=17" \
    "CODEC_SGTL5000 is not implemented yet"
fail_case taa5212 rp2350 "CODEC=CODEC_TAA5212;PIN_I2C_SDA=16;PIN_I2C_SCL=17" \
    "CODEC_TAA5212 is not implemented yet"
fail_case avdd-en rp2350 "PIN_AVDD_EN=3" \
    "PIN_AVDD_EN is set, but analog power control is not implemented yet"
fail_case mute-button rp2350 "PIN_MUTE_BUTTON=22" \
    "PIN_MUTE_BUTTON is set, but the mute button is not implemented yet"
fail_case led-on-board-led rp2350 "PIN_MUTE_LED=25" \
    "PIN_MUTE_LED is set, but the mute LED is not implemented yet" \
    "PIN_MUTE_LED is on a GPIO the board itself uses"
fail_case usb-suspend rp2350 "USB_SUSPEND_POWER_DOWN=1" \
    "USB_SUSPEND_POWER_DOWN is not implemented yet"

# --- Running a case -----------------------------------------------------------

# Prints the extra CMake arguments the RISC-V compiler needs, or fails if none is
# installed. The SDK looks for riscv32-unknown-elf or riscv32-corev-elf; Arch
# Linux names its compiler riscv64-elf.
riscv_args() {
    if command -v riscv32-unknown-elf-gcc >/dev/null || command -v riscv32-corev-elf-gcc >/dev/null; then
        return 0
    elif command -v riscv64-elf-gcc >/dev/null; then
        echo "-DPICO_GCC_TRIPLE=riscv64-elf"
    elif command -v riscv64-unknown-elf-gcc >/dev/null; then
        echo "-DPICO_GCC_TRIPLE=riscv64-unknown-elf"
    else
        return 1
    fi
}

PASSED=0 FAILED=0 SKIPPED=0

report() { # NAME EXPECTATION RESULT SECONDS NOTE
    printf '%-18s %-7s %-8s %4ss  %s\n' "$1" "$2" "$3" "$4" "$5"
}

run_case() {
    local i=$1
    local name=${NAMES[$i]} platform=${PLATFORMS[$i]} settings=${SETTINGS[$i]}
    local expected=${EXPECTED[$i]} forbidden=${FORBIDDEN[$i]} text=${TEXT[$i]}
    local dir="$OUT/$name" log="$OUT/$name.log" kind=builds extra=() start=$SECONDS
    [ -n "$expected" ] && kind=fails

    if [ "$platform" = rp2350-riscv ]; then
        local arg
        if ! arg=$(riscv_args); then
            report "$name" "$kind" SKIPPED 0 "no RISC-V compiler on PATH"
            SKIPPED=$((SKIPPED + 1))
            return
        fi
        [ -n "$arg" ] && extra+=("$arg")
    fi

    rm -rf "$dir"
    # -Wundef: an #if that reads an undefined name (a renamed setting) warns.
    {
        CFLAGS="-Wall -Wextra -Wundef" cmake -S "$ROOT" -B "$dir" -DPICO_PLATFORM="$platform" \
            "-DAUDIO_CONFIG_DEFINES=$settings" "${extra[@]}" &&
            cmake --build "$dir" -j"$JOBS"
    } >"$log" 2>&1
    local status=$?
    local seconds=$((SECONDS - start)) result=PASS note=""
    local first_error
    first_error=$(grep -m1 -E 'error:|Error' "$log" | sed 's/^.*error: //')
    local undef
    undef=$(grep -F "$ROOT/src/" "$log" | grep -m1 -F -- '-Wundef')

    if [ "$kind" = builds ]; then
        if [ $status -ne 0 ] || [ ! -f "$dir/pico-i2s-usb.uf2" ]; then
            result=FAIL note="build failed: ${first_error:-see the log}"
        elif [ -n "$undef" ]; then
            result=FAIL note="an #if reads an undefined name: ${undef#"$ROOT/"}"
        elif [ -n "$text" ] && [ "${text:0:1}" != "!" ] &&
            ! grep -qaF -- "$text" "$dir/pico-i2s-usb.elf"; then
            result=FAIL note="the firmware lacks \"$text\": an #if left that code out"
        elif [ "${text:0:1}" = "!" ] && grep -qaF -- "${text:1}" "$dir/pico-i2s-usb.elf"; then
            result=FAIL note="the firmware contains \"${text:1}\": an #if kept that code in"
        fi
    else
        if [ $status -eq 0 ]; then
            result=FAIL note="built, but should have stopped with: $expected"
        elif ! grep -qF -- "$expected" "$log"; then
            result=FAIL note="stopped without the expected message; first error: ${first_error:-see the log}"
        elif [ -n "$forbidden" ] && grep -qF -- "$forbidden" "$log"; then
            result=FAIL note="also stopped with: $forbidden"
        fi
    fi

    if [ "$result" = PASS ]; then
        PASSED=$((PASSED + 1))
        local warnings
        warnings=$(grep -F "$ROOT/src/" "$log" | grep -c 'warning:')
        [ "$warnings" -gt 0 ] && note="$warnings warning(s) in src/, see the log"
    else
        FAILED=$((FAILED + 1))
    fi
    report "$name" "$kind" "$result" "$seconds" "$note"
}

# --- Main ---------------------------------------------------------------------

if [ "${1:-}" = --list ]; then
    printf '%s\n' "${NAMES[@]}"
    exit 0
fi

SELECTED=()
if [ $# -eq 0 ]; then
    SELECTED=("${!NAMES[@]}")
else
    for want in "$@"; do
        found=""
        for i in "${!NAMES[@]}"; do
            [ "${NAMES[$i]}" = "$want" ] && found=$i
        done
        if [ -z "$found" ]; then
            echo "unknown case: $want (scripts/build-matrix.sh --list shows them)" >&2
            exit 2
        fi
        SELECTED+=("$found")
    done
fi

mkdir -p "$OUT"
echo "Logs: $OUT/<case>.log"
printf '%-18s %-7s %-8s %5s  %s\n' case expect result time note
for i in "${SELECTED[@]}"; do
    run_case "$i"
done
echo "$PASSED passed, $FAILED failed, $SKIPPED skipped"
[ $FAILED -eq 0 ]
