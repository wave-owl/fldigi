#!/bin/bash

set -e

TARGET="x86_64-w64-mingw32.static"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MXE_DIR="${1:-$HOME/mxe}"

if [ ! -f "$MXE_DIR/Makefile" ] || [ ! -d "$MXE_DIR/src" ]; then
	echo "Error: MXE tree not found at:"
	echo "  $MXE_DIR"
	echo
	echo "usage: $0 [mxe-directory]"
	exit 1
fi

echo "Installing fldigi MXE support files in:"
echo "  $MXE_DIR"

cp "$SCRIPT_DIR/fltk.mk" \
   "$SCRIPT_DIR/hamlib.mk" \
   "$SCRIPT_DIR/hamlib-1-planB-v1a.patch" \
   "$SCRIPT_DIR/hamlib-test.c" \
   "$MXE_DIR/src/"

# Ensure replacement recipes are newer than any existing MXE
# installation markers.
touch "$MXE_DIR/src/fltk.mk" "$MXE_DIR/src/hamlib.mk"

echo
echo "Building fldigi MXE dependencies for $TARGET"
echo

make -C "$MXE_DIR" -j"$(nproc)" \
	MXE_TARGETS="$TARGET" \
	fltk hamlib libsamplerate libsndfile portaudio

echo
echo "MXE dependencies for fldigi completed successfully."

