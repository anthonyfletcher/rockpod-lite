#!/bin/sh
# Bundle Themify_2 -- this fork's only shipped theme -- into a rockbox.zip
# produced by `make zip`, and make it the first-boot default via a
# pre-populated config.cfg (applied before any compiled DEFAULT_WPSNAME /
# DEFAULT_SBSNAME fallback takes effect).
#
# This lives here rather than in tools/buildzip.pl so that file stays
# byte-identical to upstream. `make zip` alone produces a themeless zip;
# every path that ships a build must run this afterwards.
#
# Usage: run from inside a build dir, or pass the build dir as an argument.
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILDDIR="$(cd "${1:-.}" && pwd)"
ZIP="$BUILDDIR/rockbox.zip"

if [ ! -f "$ZIP" ]; then
    echo "bundle-theme.sh: no rockbox.zip in $BUILDDIR -- run 'make zip' first" >&2
    exit 1
fi

# themes/Themify_2/.rockbox is already laid out with the on-device directory
# structure, so it drops straight in.
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/.rockbox"
cp -R "$ROOT/themes/Themify_2/.rockbox/." "$STAGE/.rockbox/"
cp "$ROOT/themes/Themify_2/default-config.cfg" "$STAGE/.rockbox/config.cfg"
(cd "$STAGE" && zip -qr "$ZIP" .rockbox)

# Upstream buildzip.pl still mkdir's a classic_statusbar theme dir; that
# theme's source files are gone from this fork, so it lands in the zip empty.
zip -qd "$ZIP" '.rockbox/wps/classic_statusbar/*' >/dev/null 2>&1 || true

echo "bundled Themify_2 + config.cfg into $ZIP"
