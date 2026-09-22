#!/bin/bash

set -e

PREFIX="$HOME/mxe/usr"
TARGET="x86_64-w64-mingw32.static"

TOPDIR="$(cd "$(dirname "$0")" && pwd)"
BUILDBASE="$TOPDIR/build"

usage()
{
	echo "usage: $0 linux|mxe [--clean] [--install]"
	exit 1
}

if [ -f "$TOPDIR/config.status" ]; then
	echo "Error: source tree contains an in-tree configuration."
	echo "Run 'make distclean' in the source tree before using this script."
	exit 1
fi

if [ $# -lt 1 ]; then
	usage
fi

MODE="$1"
shift

CLEAN=0
INSTALL=0

while [ $# -gt 0 ]; do
	case "$1" in
	--clean)
		CLEAN=1
		;;
	--install)
		INSTALL=1
		;;
	*)
		usage
		;;
	esac
	shift
done

if [ $# -eq 2 ]; then
	if [ "$2" != "--clean" ]; then
		usage
	fi
	CLEAN=1
fi

case "$MODE" in
linux)
	BUILDDIR="$BUILDBASE/autotools-linux"
	;;

mxe)
	BUILDDIR="$BUILDBASE/autotools-mxe"
	;;

*)
	usage
	;;
esac

if [ "$MODE" = "mxe" ] && [ "$INSTALL" -eq 1 ]; then
	echo "Error: --install is only supported for the native Linux build."
	exit 1
fi

cd "$TOPDIR"

autoreconf -fi

if [ "$CLEAN" -eq 1 ]; then
	rm -rf "$BUILDDIR"
fi

mkdir -p "$BUILDDIR"
cd "$BUILDDIR"

case "$MODE" in
linux)
	unset CC CXX PKG_CONFIG FLTK_CONFIG

	if [ ! -f Makefile ]; then
		"$TOPDIR/configure"
	fi

make -j"$(nproc)"

if [ "$INSTALL" -eq 1 ]; then
	if [ "$(id -u)" -eq 0 ]; then
		make install
	elif command -v sudo >/dev/null 2>&1; then
		echo
		echo "Installing fldigi to /usr/local requires administrator privileges."
		sudo make install
	else
		echo "Error: installation requires administrator privileges, but sudo was not found."
		exit 1
	fi
fi
	;;

mxe)
	export PATH="$PREFIX/bin:$PATH"
	export CC="$TARGET-gcc"
	export CXX="$TARGET-g++"
	export PKG_CONFIG="$PREFIX/bin/$TARGET-pkg-config"
	export FLTK_CONFIG="$PREFIX/$TARGET/bin/fltk-config"

	if [ ! -f Makefile ]; then
		"$TOPDIR/configure" --host="$TARGET"
	fi

	make -j"$(nproc)"

	"$PREFIX/bin/$TARGET-strip" src/fldigi.exe

	make nsisinst

	mv src/*setup.exe "$TOPDIR/"
	ls -l "$TOPDIR/"*setup.exe
	;;
esac

