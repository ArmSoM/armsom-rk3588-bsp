#!/bin/sh

USB_ROCKCHIP_DIR=/sys/kernel/config/usb_gadget/rockchip/
USB_FUNCTIONS_DIR=${USB_ROCKCHIP_DIR}/functions/
UVC_DIR=${USB_FUNCTIONS_DIR}/uvc.gs6/
UVC_STREAMING_DIR=${UVC_DIR}/streaming/
UVC_CONTROL_DIR=${UVC_DIR}/control/

UVC_U_DIR=${UVC_STREAMING_DIR}/uncompressed/u/
UVC_M_DIR=${UVC_STREAMING_DIR}/mjpeg/m/
UVC_F_DIR=${UVC_STREAMING_DIR}/framebased/f/

configure_uvc_resolution_yuyv()
{
	W=$1
	H=$2
	DIR=${UVC_U_DIR}/${H}p/
	mkdir ${DIR}
	echo $W > ${DIR}/wWidth
	echo $H > ${DIR}/wHeight
	echo 333333 > ${DIR}/dwDefaultFrameInterval
	echo $((W*H*20)) > ${DIR}/dwMinBitRate
	echo $((W*H*20)) > ${DIR}/dwMaxBitRate
	echo $((W*H*2)) > ${DIR}/dwMaxVideoFrameBufferSize
	echo -e "333333\n666666\n1000000\n2000000" > ${DIR}/dwFrameInterval
}

configure_uvc_resolution_mjpeg()
{
	W=$1
	H=$2
	DIR=${UVC_M_DIR}/${H}p/
	mkdir ${DIR}
	echo $W > ${DIR}/wWidth
	echo $H > ${DIR}/wHeight
	echo 333333 > ${DIR}/dwDefaultFrameInterval
	echo $((W*H*20)) > ${DIR}/dwMinBitRate
	echo $((W*H*20)) > ${DIR}/dwMaxBitRate
	echo $((W*H*2)) > ${DIR}/dwMaxVideoFrameBufferSize
	echo -e "333333\n666666\n1000000\n2000000" > ${DIR}/dwFrameInterval
}

configure_uvc_resolution_h264()
{
	W=$1
	H=$2
	DIR=${UVC_F_DIR}/${H}p/
	mkdir ${DIR}
	echo $W > ${DIR}/wWidth
	echo $H > ${DIR}/wHeight
	echo 333333 > ${DIR}/dwDefaultFrameInterval
	echo $((W*H*10)) > ${DIR}/dwMinBitRate
	echo $((W*H*10)) > ${DIR}/dwMaxBitRate
	#echo $((W*H*2)) > ${DIR}/dwMaxVideoFrameBufferSize
	echo -e "333333\n666666\n1000000\n2000000" > ${DIR}/dwFrameInterval
}

/etc/init.d/S10udev stop

umount /sys/kernel/config
mount -t configfs none /sys/kernel/config
mkdir -p ${USB_ROCKCHIP_DIR}
mkdir -p ${USB_ROCKCHIP_DIR}/strings/0x409
mkdir -p ${USB_ROCKCHIP_DIR}/configs/b.1/strings/0x409

echo 0x2207 > ${USB_ROCKCHIP_DIR}/idVendor
echo 0x0310 > ${USB_ROCKCHIP_DIR}/bcdDevice
echo 0x0200 > ${USB_ROCKCHIP_DIR}/bcdUSB

echo "2020" > ${USB_ROCKCHIP_DIR}/strings/0x409/serialnumber
echo "Rockchip" > ${USB_ROCKCHIP_DIR}/strings/0x409/manufacturer
echo "UVC" > ${USB_ROCKCHIP_DIR}/strings/0x409/product

mkdir ${UVC_DIR}
#echo 3072 > ${UVC_DIR}/streaming_maxpacket
#echo 1 > ${UVC_DIR}/streaming_bulk

mkdir ${UVC_CONTROL_DIR}/header/h
ln -s ${UVC_CONTROL_DIR}/header/h ${UVC_CONTROL_DIR}/class/fs/h
ln -s ${UVC_CONTROL_DIR}/header/h ${UVC_CONTROL_DIR}/class/ss/h

##YUYV support config
mkdir ${UVC_U_DIR}
configure_uvc_resolution_yuyv 640 480
configure_uvc_resolution_yuyv 1280 720

##mjpeg support config
mkdir ${UVC_M_DIR}
configure_uvc_resolution_mjpeg 640 480
configure_uvc_resolution_mjpeg 1280 720
configure_uvc_resolution_mjpeg 1920 1080
configure_uvc_resolution_mjpeg 2560 1440
configure_uvc_resolution_mjpeg 2592 1944

## h.264 support config
mkdir ${UVC_F_DIR}
configure_uvc_resolution_h264 640 480
configure_uvc_resolution_h264 1280 720
configure_uvc_resolution_h264 1920 1080

mkdir ${UVC_STREAMING_DIR}/header/h
ln -s ${UVC_U_DIR} ${UVC_STREAMING_DIR}/header/h/u
ln -s ${UVC_M_DIR} ${UVC_STREAMING_DIR}/header/h/m
ln -s ${UVC_F_DIR} ${UVC_STREAMING_DIR}/header/h/f
ln -s ${UVC_STREAMING_DIR}/header/h ${UVC_STREAMING_DIR}/class/fs/h
ln -s ${UVC_STREAMING_DIR}/header/h ${UVC_STREAMING_DIR}/class/hs/h
ln -s ${UVC_STREAMING_DIR}/header/h ${UVC_STREAMING_DIR}/class/ss/h

echo 0x1 > ${USB_ROCKCHIP_DIR}/os_desc/b_vendor_code
echo "MSFT100" > ${USB_ROCKCHIP_DIR}/os_desc/qw_sign
echo 500 > ${USB_ROCKCHIP_DIR}/configs/b.1/MaxPower
ln -s ${USB_ROCKCHIP_DIR}/configs/b.1 ${USB_ROCKCHIP_DIR}/os_desc/b.1

echo 0x0005 > ${USB_ROCKCHIP_DIR}/idProduct
echo "uvc" > ${USB_ROCKCHIP_DIR}/configs/b.1/strings/0x409/configuration
USB_CONFIGS_DIR=${USB_ROCKCHIP_DIR}/configs/b.1
if [ -e ${USB_CONFIGS_DIR}/ffs.adb ]; then
   #for rk1808 kernel 4.4
   rm -f ${USB_CONFIGS_DIR}/ffs.adb
else
   ls ${USB_CONFIGS_DIR} | grep f[0-9] | xargs -I {} rm ${USB_CONFIGS_DIR}/{}
fi
ln -s ${UVC_DIR} ${USB_ROCKCHIP_DIR}/configs/b.1/f1

UDC=`ls /sys/class/udc/| awk '{print $1}'`
echo $UDC > ${USB_ROCKCHIP_DIR}/UDC
