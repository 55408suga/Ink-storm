#!/bin/sh
set -u
if [ "$#" -lt 1 ]; then
    printf 'usage: expect_usage.sh <program> [args...]\n' >&2
    exit 2
fi
program=$1
shift
output=$("$program" "$@" 2>&1)
status=$?
if [ "$status" -ne 1 ]; then
    printf 'expected exit status 1, got %s\n%s\n' "$status" "$output"
    exit 1
fi
if ! printf '%s\n' "$output" | grep -q '^Usage: '; then
    printf 'missing Usage line\n%s\n' "$output"
    exit 1
fi
