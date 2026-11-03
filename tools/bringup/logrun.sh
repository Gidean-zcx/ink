#!/bin/bash
# Run a command and append a timestamp, the command line, its full output and
# its exit status to a log file. The output is also shown on the terminal.
#
#   logrun.sh <log> <command> [args...]

log=${1:?usage: logrun.sh <log> <command> [args...]}
shift
{
	printf '\n===== %s =====\n$' "$(date '+%Y-%m-%d %H:%M:%S %z')"
	printf ' %q' "$@"
	printf '\n'
} >>"$log"
"$@" 2>&1 | tee -a "$log"
rc=${PIPESTATUS[0]}
printf -- '----- exit %d -----\n' "$rc" >>"$log"
exit "$rc"
