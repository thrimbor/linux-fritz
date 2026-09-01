#! /bin/sh
start_ppa() {
temp=`grep ifx_ppa /proc/devices`
b_major=${temp%%ifx_ppa}
mknod /dev/ifx_ppa c $b_major 0
}
start_ppa & 
