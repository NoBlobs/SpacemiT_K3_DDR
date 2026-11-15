#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
#
# Stage an FSBL through FDL (USB OTG) and capture the serial console.
#   bench.sh <FSBL.bin> <run-name> [timeout_s] [until_regex]
# Output: build/bench/<date>-<run-name>/{serial.log,mbox.txt,FSBL.sha256}
# Copy the directory into logsRefs/NNN-<name>/ when the run is a reference.
set -eu
FSBL=$1
NAME=$2
TMO=${3:-60}
UNTIL=${4:-"PMU experiment finished|DDR init aborted|login:|DDR training consume"}
PORT=${SERIAL_PORT:-/dev/ttyUSB1}
DIR=build/bench/$(date +%Y%m%d-%H%M%S)-$NAME
mkdir -p "$DIR"
sha256sum "$FSBL" > "$DIR/FSBL.sha256"

fastboot devices | grep -q . || { echo "no FDL/fastboot device (power-cycle the board?)" >&2; exit 1; }
python3 -I tools/serial_log.py --port "$PORT" --timeout "$TMO" --until "$UNTIL" \
	"$DIR/serial.log" > /dev/null &
LOGPID=$!
sleep 0.5
if ! timeout 20 fastboot stage "$FSBL" || ! timeout 10 fastboot continue; then
	kill $LOGPID 2>/dev/null || true
	echo "FDL staging failed: the board is not in a fresh FDL state; power-cycle it" >&2
	exit 1
fi
wait $LOGPID || true
python3 -I tools/mbox_decode.py "$DIR/serial.log" > "$DIR/mbox.txt" || true
echo "log: $DIR"
tail -5 "$DIR/serial.log"
