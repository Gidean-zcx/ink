#!/bin/bash
# Read-only front end to esptool/espefuse for hardware bring-up.
#
# Only subcommands that cannot change the device are let through, so a typo
# cannot turn into a write. Every call and its full output go to $LOG via
# logrun.sh. By default the chip is hard-reset back into its firmware afterwards.
#
#   PORT=/dev/cu.usbmodem1101 LOG=private/commands.log esp_ro.sh chip-id
#   esp_ro.sh read-flash 0 ALL private/flash_full_1.bin
#   esp_ro.sh verify-flash 0 private/flash_full_1.bin
#   esp_ro.sh efuse-summary
#
# Environment: PORT, LOG (required); VENV (esptool virtualenv), CHIP
# (default esp32s31), BEFORE (default default-reset; no-reset when the chip
# is already in download mode), AFTER (default hard-reset; no-reset-stub
# keeps the chip in the flasher stub so the next call can follow on without
# the firmware running in between).

set -uo pipefail
: "${PORT:?set PORT to the serial port}" "${LOG:?set LOG to the command log}"
VENV=${VENV:-$HOME/Documents/Workspace/ink-hw/venv}
CHIP=${CHIP:-esp32s31}
BEFORE=${BEFORE:-default-reset}
AFTER=${AFTER:-hard-reset}
case "$AFTER" in
hard-reset | no-reset-stub) ;;
*)
	echo "esp_ro.sh: AFTER must be hard-reset or no-reset-stub" >&2
	exit 2
	;;
esac

sub=${1:?usage: esp_ro.sh <subcommand> [args...]}
shift
case "$sub" in
chip-id | flash-id | read-mac | get-security-info | read-flash | verify-flash)
	cmd=("$VENV/bin/esptool" --chip "$CHIP" -p "$PORT" --before "$BEFORE"
		--after "$AFTER" "$sub" "$@")
	;;
efuse-summary)
	# espefuse has no no-reset-stub; plain no-reset leaves the stub running too
	cmd=("$VENV/bin/espefuse" --chip "$CHIP" -p "$PORT" --before "$BEFORE"
		--after "${AFTER/no-reset-stub/no-reset}" summary "$@")
	;;
*)
	echo "esp_ro.sh: '$sub' is not on the read-only list" >&2
	exit 2
	;;
esac
exec "$(dirname "$0")/logrun.sh" "$LOG" "${cmd[@]}"
