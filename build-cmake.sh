#!/bin/bash

set -e

PREFIX="$HOME/mxe/usr"
TARGET="x86_64-w64-mingw32.static"

TOPDIR="$(cd "$(dirname "$0")" && pwd)"
BUILDBASE="$TOPDIR/build"

usage()
{
	echo "usage: $0 linux|mxe [--clean]"
	exit 1
}

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
	usage
fi

MODE="$1"
CLEAN=0

if [ $# -eq 2 ]; then
	if [ "$2" != "--clean" ]; then
		usage
	fi
	CLEAN=1
fi

case "$MODE" in
linux)
	BUILDDIR="$BUILDBASE/cmake-linux"
	;;

mxe)
	BUILDDIR="$BUILDBASE/cmake-mxe"
	;;

*)
	usage
	;;
esac

if [ "$CLEAN" -eq 1 ]; then
	rm -rf "$BUILDDIR"
fi

mkdir -p "$BUILDDIR"

case "$MODE" in
linux)
	unset CC CXX PKG_CONFIG FLTK_CONFIG

	cmake -S "$TOPDIR" -B "$BUILDDIR"

	cmake --build "$BUILDDIR" -j"$(nproc)"
	;;

mxe)
	export PATH="$PREFIX/bin:$PATH"

	TOOLCHAIN=""
	for f in "$TOPDIR"/cmake/toolchains/*.cmake; do
		[ -f "$f" ] || continue
		if grep -q 'x86_64-w64-mingw32.static' "$f"; then
			if [ -n "$TOOLCHAIN" ]; then
				echo "Error: more than one MXE toolchain file found:"
				echo "  $TOOLCHAIN"
				echo "  $f"
				exit 1
			fi
			TOOLCHAIN="$f"
		fi
	done

	if [ -z "$TOOLCHAIN" ]; then
		echo "Error: unable to locate the x86_64-w64-mingw32.static"
		echo "CMake toolchain file under:"
		echo "  $TOPDIR/cmake/toolchains"
		exit 1
	fi

	echo "Using MXE toolchain:"
	echo "  $TOOLCHAIN"

	cmake -S "$TOPDIR" -B "$BUILDDIR" \
		-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN"

	cmake --build "$BUILDDIR" -j"$(nproc)"

	"$PREFIX/bin/$TARGET-strip" "$BUILDDIR/src/fldigi.exe"

	(
		cd "$BUILDDIR"
		cpack -G NSIS
	)

	find "$BUILDDIR" -maxdepth 2 -type f -name '*setup.exe' \
		-exec cp -f {} "$TOPDIR/" \;

	ls -l "$TOPDIR/"*setup.exe
	;;
esac
