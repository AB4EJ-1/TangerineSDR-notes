#!/bin/bash'

mkdir /tmp/ramdisk
sudo chmod 777 /tmp/ramdisk
mount -t tmpfs -o size=1024m myramdisk /tmp/ramdisk
mkdir /tmp/ramdisk/ch0_3
mkdir /tmp/ramdisk/ch4_7
sudo chmod 777 /tmp/ramdisk/ch0_3
sudo chmod 777 /tmp/ramdisk/ch4_7
