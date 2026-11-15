#!/system/bin/sh
# Claude: export WHOLE UFS LUN0 (/dev/block/sda) over USB mass storage, keep adb function.
setenforce 0
G=/config/usb_gadget/g1
L=$G/functions/mass_storage.0/lun.0
echo '' > $G/UDC
sleep 1
echo 1 > $L/forced_eject 2>/dev/null
echo 0 > $L/ro
echo 0 > $L/removable
echo 0 > $L/cdrom
echo /dev/block/sda > $L/file
for f in $G/configs/b.1/*; do [ -L "$f" ] && rm -f "$f"; done
ln -s $G/functions/mass_storage.0 $G/configs/b.1/f1
ln -s $G/functions/ffs.adb $G/configs/b.1/f2
sleep 1
echo a600000.dwc3 > $G/UDC
echo "lun0 file: $(cat $L/file)"
