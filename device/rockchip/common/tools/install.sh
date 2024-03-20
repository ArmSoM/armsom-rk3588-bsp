#!/bin/bash -e

[ "$RK_ROOTFS_PREBUILT_TOOLS" ] || exit 0

TARGET_DIR="$1"
[ "$TARGET_DIR" ] || exit 1

OVERLAY_DIR="$(dirname "$(realpath "$0")")"

echo "Installing prebuilt tools: $OVERLAY_DIR to $TARGET_DIR..."

DEST_DIR="$TARGET_DIR/usr/bin/"
mkdir -p "$DEST_DIR"

if ls "$TARGET_DIR/lib/" | grep -wq "ld-linux-armhf.so"; then
	TARGET_ARCH=armhf
else
	TARGET_ARCH=aarch64
fi

$RK_RSYNC --exclude=adbd \
	"$OVERLAY_DIR/perf" "$OVERLAY_DIR/$TARGET_ARCH/" "$DEST_DIR/"
