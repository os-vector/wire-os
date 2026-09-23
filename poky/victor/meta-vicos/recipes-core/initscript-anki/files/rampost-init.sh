#!/bin/sh

# Enable power rails required for GPIO, LCD, IMU and Camera
REG_DEBUG_PATH=/sys/kernel/debug/regulator
echo 1 > $REG_DEBUG_PATH/soc:qcom,rpm-smd:rpm-regulator-ldoa8:regulator-l8-8916_l8/enable
echo 1 > $REG_DEBUG_PATH/soc:qcom,rpm-smd:rpm-regulator-ldoa17:regulator-l17-8916_l17/enable
echo 1 > $REG_DEBUG_PATH/soc:qcom,rpm-smd:rpm-regulator-ldoa4:regulator-l4-8916_l4/enable

i=0
while [ ! -c /dev/ttyHS0 ] && [ $i -lt 50 ]; do
	sleep 0.1
	i=$((i + 1))
done

rampost -d /etc/syscon.dfu | tee /dev/rampost.log

exit 0
