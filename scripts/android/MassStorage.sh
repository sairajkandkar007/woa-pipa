#!/sbin/sh
# Upstream Project-Silicium/Guides SoCs/Snapdragon/Mass-Storage/0x0A600000/MassStorage.sh (SM8250)
setenforce 0
echo 0xEF > /config/usb_gadget/g1/bDeviceClass
echo 0x02 > /config/usb_gadget/g1/bDeviceSubClass
echo 0x01 > /config/usb_gadget/g1/bDeviceProtocol
ln -s /config/usb_gadget/g1/functions/mass_storage.0/ /config/usb_gadget/g1/configs/b.1/
echo /dev/block/sda > /config/usb_gadget/g1/configs/b.1/mass_storage.0/lun.0/file
echo 0 > /config/usb_gadget/g1/configs/b.1/mass_storage.0/lun.0/cdrom
echo 0 > /config/usb_gadget/g1/configs/b.1/mass_storage.0/lun.0/removable
echo 0 > /config/usb_gadget/g1/configs/b.1/mass_storage.0/lun.0/ro
sh -c 'echo > /config/usb_gadget/g1/UDC; echo a600000.dwc3 > /config/usb_gadget/g1/UDC' &
