#!/bin/bash -eux
# -e: Exit immediately if a command exits with a non-zero status.
# -u: Treat unset variables as an error when substituting.
# -x: Display expanded script commands

mkdir -p build-release
cd build-release

PLATFORM=m68k-atari-mintelf
FASTCALL=false
export ASFLAGS="-m68020-60"
export CXXFLAGS="-m68020-60 -DUSE_MOVE16 -DUSE_SUPERVIDEL -DUSE_SV_BLITTER -DDISABLE_LAUNCHERDISPLAY_GRID"
export LDFLAGS="-m68020-60"

export PKG_CONFIG_LIBDIR="$(${PLATFORM}-gcc -print-sysroot)/usr/lib/m68020-60/pkgconfig"

if $FASTCALL
then
	ASFLAGS="$ASFLAGS -mfastcall"
	CXXFLAGS="$CXXFLAGS -mfastcall"
	LDFLAGS="$LDFLAGS -mfastcall"
fi

BASE_BRANCH=release-2026.2.0
BRANCH=atari-patched

if [ -d ../.git/rebase-apply ]; then
	set +e
	git -C .. am --abort
	git -C .. checkout "$BASE_BRANCH"
	git -C .. branch -D "$BRANCH"
	set -e
fi

if git -C .. rev-parse --verify "$BRANCH" >/dev/null 2>&1
then
	git -C .. checkout "$BRANCH"
else
	git -C .. checkout -b "$BRANCH" "$BASE_BRANCH"
	for p in ../backends/platform/atari/patches/*.patch; do
		if ! git -C .. am "${p#../}"; then
			git -C .. am --abort
			git -C .. checkout "$BASE_BRANCH"
			git -C .. branch -D "$BRANCH"
			exit 1
		fi
	done
fi

if [ ! -f config.log ]
then
../configure \
	--backend=atari \
	--host=${PLATFORM} \
	--enable-release \
	--enable-verbose-build \
	--disable-engine=hugo,director,cine,ultima
fi

make -j$(getconf _NPROCESSORS_CONF) atarifulldist
