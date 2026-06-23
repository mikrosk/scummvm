#!/bin/bash -eux
# -e: Exit immediately if a command exits with a non-zero status.
# -u: Treat unset variables as an error when substituting.
# -x: Display expanded script commands

mkdir -p build-release030
cd build-release030

PLATFORM=m68k-atari-mintelf
FASTCALL=false
PLUGINS=true
export ASFLAGS="-m68030"
export CXXFLAGS="-m68030 -DDISABLE_FANCY_THEMES -DDISABLE_DOSBOX_OPL -DDISABLE_MAME_OPL"
export LDFLAGS="-m68030"

export PKG_CONFIG_LIBDIR="$(${PLATFORM}-gcc -print-sysroot)/usr/lib/m68020-60/pkgconfig"

if $FASTCALL
then
	ASFLAGS="$ASFLAGS -mfastcall"
	CXXFLAGS="$CXXFLAGS -mfastcall"
	LDFLAGS="$LDFLAGS -mfastcall"
fi

if $PLUGINS
then
	PLUGINS_FLAGS="--enable-plugins --default-dynamic --enable-detection-dynamic"
else
	PLUGINS_FLAGS=""
fi

BASE_BRANCH=release-2026.3.0
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
	--disable-highres \
	--disable-bink \
	--enable-verbose-build \
	--disable-engine=hugo,director,cine,ultima \
	${PLUGINS_FLAGS}
fi

make -j$(getconf _NPROCESSORS_CONF) atarilitedist
