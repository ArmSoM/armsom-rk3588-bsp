#!/bin/bash -e

TARGET_ROOTFS_DIR=./binary
ROOTFSIMAGE=linaro-rootfs.img
EXTRA_SIZE_MB=300
IMAGE_SIZE_MB=$(( $(sudo du -sh -m ${TARGET_ROOTFS_DIR} | cut -f1) + ${EXTRA_SIZE_MB} ))


echo Making rootfs!

if [ -e ${ROOTFSIMAGE} ]; then
	sudo rm ${ROOTFSIMAGE}
fi

for script in ./post-build.sh ../device/rockchip/common/post-build.sh; do
	[ -x $script ] || continue
	sudo $script "$(realpath "$TARGET_ROOTFS_DIR")"
done

sudo mkfs.ext4 -d ${TARGET_ROOTFS_DIR} ${ROOTFSIMAGE} ${IMAGE_SIZE_MB}M

echo Rootfs Image: ${ROOTFSIMAGE}
