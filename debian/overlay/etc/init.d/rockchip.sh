#!/bin/bash -e
### BEGIN INIT INFO
# Provides:          rockchip
# Required-Start:
# Required-Stop:
# Default-Start:
# Default-Stop:
# Short-Description:
# Description:       Setup rockchip platform environment
### END INIT INFO

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

install_packages() {
    case $1 in
        rk3288)
		MALI=midgard-t76x-r18p0-r0p0
		ISP=rkisp
		# 3288w
		cat /sys/devices/platform/*gpu/gpuinfo | grep -q r1p0 && \
		MALI=midgard-t76x-r18p0-r1p0
		sed -i "s/always/none/g" /etc/X11/xorg.conf.d/20-modesetting.conf
		[ -e /usr/lib/arm-linux-gnueabihf/ ] && apt install -fy --allow-downgrades /camera_engine_$ISP*.deb
		;;
        rk3399|rk3399pro)
		MALI=midgard-t86x-r18p0
		ISP=rkisp
		sed -i "s/always/none/g" /etc/X11/xorg.conf.d/20-modesetting.conf
		[ -e /usr/lib/aarch64-linux-gnu/ ] && apt install -fy --allow-downgrades /camera_engine_$ISP*.deb
		;;
        rk3328|rk3528)
		MALI=utgard-450
		ISP=rkisp
		sed -i "s/always/none/g" /etc/X11/xorg.conf.d/20-modesetting.conf
		[ -e /usr/lib/aarch64-linux-gnu/ ] && apt install -fy --allow-downgrades /camera_engine_$ISP*.deb
		;;
        rk3326|px30)
		MALI=bifrost-g31-g13p0
		ISP=rkisp
		sed -i "s/always/none/g" /etc/X11/xorg.conf.d/20-modesetting.conf
		[ -e /usr/lib/aarch64-linux-gnu/ ] && apt install -fy --allow-downgrades /camera_engine_$ISP*.deb
		;;
        rk3128|rk3036)
		MALI=utgard-400
		ISP=rkisp
		sed -i "s/always/none/g" /etc/X11/xorg.conf.d/20-modesetting.conf
		[ -e /usr/lib/arm-linux-gnueabihf/ ] && apt install -fy --allow-downgrades /camera_engine_$ISP*.deb
		;;
        rk3568|rk3566)
		MALI=bifrost-g52-g13p0
		ISP=rkaiq_rk3568
		[ -e /usr/lib/aarch64-linux-gnu/ ] && tar xvf /rknpu2-*.tar -C /
		[ -e /usr/lib/aarch64-linux-gnu/ ] && apt install -fy --allow-downgrades /camera_engine_$ISP*.deb
		;;
        rk3562)
		MALI=bifrost-g52-g13p0
		ISP=rkaiq_rk3562
		[ -e /usr/lib/aarch64-linux-gnu/ ] && tar xvf /rknpu2-*.tar -C /
		[ -e /usr/lib/aarch64-linux-gnu/ ] && apt install -fy --allow-downgrades /camera_engine_$ISP*.deb
		;;
        rk3588|rk3588s)
		ISP=rkaiq_rk3588
		MALI=valhall-g610-g13p0
		[ -e /usr/lib/aarch64-linux-gnu/ ] && tar xvf /rknpu2-*.tar -C /
		[ -e /usr/lib/aarch64-linux-gnu/ ] && apt install -fy --allow-downgrades /camera_engine_$ISP*.deb
		;;
        *)
        echo "This chip does not support gpu acceleration or not input!!!"
		;;
    esac

    apt install -fy --allow-downgrades /libmali-*$MALI*-x11*.deb
}

function update_npu_fw() {
    /usr/bin/npu-image.sh
    sleep 1
    /usr/bin/npu_transfer_proxy&
}

COMPATIBLE=$(cat /proc/device-tree/compatible)
if [[ $COMPATIBLE =~ "rk3288" ]];
then
    CHIPNAME="rk3288"
elif [[ $COMPATIBLE =~ "rk3328" ]]; then
    CHIPNAME="rk3328"
elif [[ $COMPATIBLE =~ "rk3399" && $COMPATIBLE =~ "rk3399pro" ]]; then
    CHIPNAME="rk3399pro"
    update_npu_fw
elif [[ $COMPATIBLE =~ "rk3399" ]]; then
    CHIPNAME="rk3399"
elif [[ $COMPATIBLE =~ "rk3326" ]]; then
    CHIPNAME="rk3326"
elif [[ $COMPATIBLE =~ "px30" ]]; then
    CHIPNAME="px30"
elif [[ $COMPATIBLE =~ "rk3128" ]]; then
    CHIPNAME="rk3128"
elif [[ $COMPATIBLE =~ "rk3562" ]]; then
    CHIPNAME="rk3562"
elif [[ $COMPATIBLE =~ "rk3566" ]]; then
    CHIPNAME="rk3566"
elif [[ $COMPATIBLE =~ "rk3568" ]]; then
    CHIPNAME="rk3568"
elif [[ $COMPATIBLE =~ "rk3588" ]]; then
    CHIPNAME="rk3588"
elif [[ $COMPATIBLE =~ "rk3036" ]]; then
    CHIPNAME="rk3036"
elif [[ $COMPATIBLE =~ "rk3308" ]]; then
    CHIPNAME="rk3208"
elif [[ $COMPATIBLE =~ "rv1126" ]]; then
    CHIPNAME="rv1126"
elif [[ $COMPATIBLE =~ "rv1109" ]]; then
    CHIPNAME="rv1109"
else
    echo "please check if the Socs had been supported on rockchip linux!!!!!!!"
fi
COMPATIBLE=${COMPATIBLE#rockchip,}
BOARDNAME=${COMPATIBLE%%rockchip,*}

# first boot configure
if [ ! -e "/usr/local/first_boot_flag" ] ;
then
    echo "It's the first time booting."
    echo "The rootfs will be configured."

    # Force rootfs synced
    mount -o remount,sync /

    install_packages ${CHIPNAME}

    setcap CAP_SYS_ADMIN+ep /usr/bin/gst-launch-1.0

    if [ -e "/dev/rfkill" ] ;
    then
       rm /dev/rfkill
    fi

    rm -rf /*.deb
    rm -rf /*.tar

    # The base target does not come with lightdm/rkaiq_3A
if [ -e /etc/gdm3/daemon.conf ]; then
    systemctl restart gdm3.service || true
elif [ -e /etc/lightdm/lightdm.conf ]; then
    systemctl restart lightdm.service || true
fi

if [ -e /usr/lib/systemd/system/rkisp_3A.service ]; then
    systemctl restart rkisp_3A.service || true
elif [ -e /usr/lib/systemd/system/rkaiq_3A.service ]; then
    systemctl restart rkaiq_3A.service || true
fi
    touch /usr/local/first_boot_flag
fi

# enable async service
#service async start

# enable adbd service
#service adbd start

# support power management
if [ -e "/usr/sbin/pm-suspend" -a -e /etc/Powermanager ] ;
then
    mv /etc/Powermanager/power-key.sh /usr/bin/
    mv /etc/Powermanager/power-key.conf /etc/triggerhappy/triggers.d/
    if [[ "$CHIPNAME" == "rk3399pro" ]];
    then
        mv /etc/Powermanager/01npu /usr/lib/pm-utils/sleep.d/
        mv /etc/Powermanager/02npu /lib/systemd/system-sleep/
    fi
    mv /etc/Powermanager/03wifibt /usr/lib/pm-utils/sleep.d/
    mv /etc/Powermanager/04wifibt /lib/systemd/system-sleep/
    mv /etc/Powermanager/triggerhappy /etc/init.d/triggerhappy

    rm /etc/Powermanager -rf
    service triggerhappy restart
fi

# Create dummy video node for chromium V4L2 VDA/VEA with rkmpp plugin
echo dec > /dev/video-dec0
echo enc > /dev/video-enc0
chmod 660 /dev/video-*
chown root:video /dev/video-*

# The chromium using fixed pathes for libv4l2.so
ln -rsf /usr/lib/*/libv4l2.so /usr/lib/
[ -e /usr/lib/aarch64-linux-gnu/ ] && ln -Tsf lib /usr/lib64

# sync system time
hwclock --systohc

# read mac-address from efuse
# if [ "$BOARDNAME" == "rk3288-miniarm" ]; then
#     MAC=`xxd -s 16 -l 6 -g 1 /sys/bus/nvmem/devices/rockchip-efuse0/nvmem | awk '{print $2$3$4$5$6$7 }'`
#     ifconfig eth0 hw ether $MAC
# fi
