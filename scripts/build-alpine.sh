#!/bin/sh
# scripts/build-alpine.sh — build APPLaunch + Calculator for Alpine Linux
# (musl, aarch64), natively inside an emulated linux/arm64 alpine:3.24
# container.
#
# Why native-in-container instead of the upstream cross profile: the
# upstream cross path links against the glibc `static_lib_vX` sysroot, which
# cannot produce a binary for a musl userland. A native build inside the
# same Alpine release as the target image gives an ABI-exact binary and
# exercises exactly the code path the device runs (the `config_defaults.mk`
# FBDEV+EVDEV profile selected automatically on aarch64 hosts).
#
# Requirements (Docker host): docker + binfmt arm64 emulation (qemu), the
# same prerequisite as the cardputer-zero-alpine image build.
#
# Usage:
#   ./scripts/build-alpine.sh [version]        # default: 1.0.0-alpine
#   REPO_HOST=/path/on/docker-host ./scripts/build-alpine.sh
#
# REPO_HOST is the repository path AS SEEN BY THE DOCKER DAEMON (when docker
# runs on a different host than this shell, mirroring cardputer-zero-alpine's
# build/20-rootfs.sh convention).
#
# Produces:
#   projects/APPLaunch/dist/M5CardputerZero-APPLaunch      aarch64 musl
#   projects/Calculator/dist/M5CardputerZero-Calculator   aarch64 musl
#   dist-alpine/applaunch-<version>-alpine.tar.gz          deployable tree
#
# The tarball contains a single `APPLaunch/` directory: unpack it at
# /usr/share/APPLaunch on the device (bin/, share/, applications/, lib/,
# adb/). OpenRC integration lives in deploy/alpine/.
set -eu

VER=${1:-1.0.0-alpine}
CT=${CPZ_LAUNCHER_CT:-cpz-launcher-build}
J=${J:-8}
REPO_HOST=${REPO_HOST:-}
[ -n "$REPO_HOST" ] || REPO_HOST=$(cd "$(dirname "$0")/.." && pwd)

# --- build deps for the native aarch64 Alpine profile ---
# build-base    gcc/g++/make for aarch64
# scons + python modules: the SDK SCons toolchain (parse/requests/tqdm)
# freetype-dev  LVGL FreeType integration (linked directly)
# curl-dev      libcurl.so.4 (linked directly, updater/http worker)
# libinput-dev libxkbcommon-dev eudev-dev dbus-dev glib-dev
#               the cp0_lvgl device backend (evdev via libinput+xkb, udev,
#               bluez over dbus, NetworkManager over gio)
BUILD_DEPS="build-base python3 scons py3-parse py3-requests py3-tqdm \
pkgconf freetype-dev curl-dev libinput-dev libxkbcommon-dev \
eudev-dev dbus-dev glib-dev unzip wget file"

docker rm -f "$CT" >/dev/null 2>&1 || true
docker run -d --platform linux/arm64 --name "$CT" \
    -v "$REPO_HOST:/repo" alpine:3.24 sleep infinity >/dev/null

echo "== installing build deps (linux/arm64 alpine:3.24) =="
docker exec "$CT" apk add --no-cache --quiet $BUILD_DEPS

echo "== pre-seeding pinned third-party sources (eventpp, C-Thread-Pool) =="
# CONFIG_REPO_AUTOMATION=y would fetch these mid-build; seeding first keeps
# the build deterministic and failure messages obvious. The pins mirror
# SDK/github_source/source-list.sh and the upstream CI pre-download step.
docker exec "$CT" sh -c '
set -e
cd /repo/SDK/github_source
if [ ! -d eventpp ]; then
  wget -q https://github.com/wqking/eventpp/archive/c472fb22e71ead0e58ff7df89e12c66b0bdfb533.zip -O /tmp/eventpp.zip
  unzip -q -o /tmp/eventpp.zip -d /tmp
  rm -rf eventpp && mv /tmp/eventpp-c472fb22e71ead0e58ff7df89e12c66b0bdfb533 eventpp
fi
if [ ! -d C-Thread-Pool ]; then
  wget -q https://github.com/Pithikos/C-Thread-Pool/archive/refs/heads/master.zip -O /tmp/ctp.zip
  unzip -q -o /tmp/ctp.zip -d /tmp
  rm -rf C-Thread-Pool && mv /tmp/C-Thread-Pool-master C-Thread-Pool
fi
ls eventpp/include/eventpp/callbacklist.h C-Thread-Pool/thpool.c >/dev/null
echo "sources pre-seeded OK"
'

echo "== building APPLaunch (aarch64 musl, version $VER) =="
docker exec "$CT" sh -c "
cd /repo/projects/APPLaunch
export CONFIG_REPO_AUTOMATION=y
export APPLAUNCH_VERSION=$VER
export APPLAUNCH_CHANNEL=alpine
scons -j$J
"

echo "== applying the Calculator native-build patch (see patches/README) =="
docker exec "$CT" sh -c '
set -e
PATCH=/repo/deploy/alpine/patches/calculator-native-fbdev.patch
cd /repo/projects/Calculator
patch -p1 --dry-run -f -s < "$PATCH" 2>/dev/null \
  && patch -p1 -s < "$PATCH" \
  || echo "patch already applied (or tree dirty) — continuing"
'

echo "== building Calculator (aarch64 musl) =="
docker exec "$CT" sh -c "
cd /repo/projects/Calculator
export CONFIG_REPO_AUTOMATION=y
# Calculator's SConstruct requires an explicit profile (unlike APPLaunch);
# config_defaults.mk is its native device profile (FBDEV).
export CONFIG_DEFAULT_FILE=config_defaults.mk
scons -j$J
"

echo "== staging + sanity checks =="
docker exec "$CT" sh -c '
set -e
cd /repo
LAUNCHER=projects/APPLaunch/dist/M5CardputerZero-APPLaunch
CALC=projects/Calculator/dist/M5CardputerZero-Calculator
[ -f "$CALC" ] || CALC=projects/Calculator/dist/Calculator
test -f "$LAUNCHER" || { echo "FAIL: launcher binary missing"; exit 1; }
test -f "$CALC" || { echo "FAIL: calculator binary missing"; exit 1; }

file "$LAUNCHER" "$CALC"
echo "$LAUNCHER: $(file "$LAUNCHER")" | grep -q "ARM aarch64" \
  || { echo "FAIL: launcher is not aarch64"; exit 1; }
file "$LAUNCHER" | grep -q "interpreter /lib/ld-musl-aarch64" \
  || { echo "FAIL: launcher is not linked against musl"; exit 1; }

STAGE=/repo/dist-alpine/stage
rm -rf /repo/dist-alpine "$STAGE"
mkdir -p "$STAGE/APPLaunch/bin"
cp -a projects/APPLaunch/dist/APPLaunch/. "$STAGE/APPLaunch/"
cp "$LAUNCHER" "$STAGE/APPLaunch/bin/M5CardputerZero-APPLaunch"
cp "$CALC" "$STAGE/APPLaunch/bin/M5CardputerZero-Calculator"
ls -la "$STAGE/APPLaunch/bin/"
'

echo "== packing dist-alpine/applaunch-$VER.tar.gz =="
docker exec "$CT" sh -c "
set -e
tar -C /repo/dist-alpine/stage -czf /repo/dist-alpine/applaunch-$VER.tar.gz APPLaunch
rm -rf /repo/dist-alpine/stage
tar -tzf /repo/dist-alpine/applaunch-$VER.tar.gz | head -12
ls -la /repo/dist-alpine/
"

echo "== DONE: dist-alpine/applaunch-$VER.tar.gz =="
echo "Deploy: unpack APPLaunch/ at /usr/share/APPLaunch; OpenRC units in deploy/alpine/."
