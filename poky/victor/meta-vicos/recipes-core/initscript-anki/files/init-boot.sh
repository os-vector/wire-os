#!/bin/sh

fatal() {
	echo "$@"
	exit 1
}

export PATH=/sbin:/bin:/usr/sbin:/usr/bin

mkdir -p /proc
mkdir -p /sys
mkdir -p /dev
mount -t proc -o nodev,noexec,nosuid proc /proc
mount -t sysfs -o nodev,noexec,nosuid sysfs /sys
mount -t devtmpfs -o noexec none /dev
mount -t debugfs -o nodev,noexec,nosuid nodev /sys/kernel/debug

mkdir -p /var
mkdir -p /var/run

CMDLINE=$(cat /proc/cmdline)

ROOTFS="${CMDLINE##*root=}"
ROOTFS="${ROOTFS%% *}"

[ -z "${ROOTFS}" ] && fatal "ERROR: Kernel boot arg root=... not found"

# Enable power rails required for GPIO, LCD, IMU and Camera
echo 1 > /sys/kernel/debug/regulator/8916_l8/enable
echo 1 > /sys/kernel/debug/regulator/8916_l17/enable
echo 1 > /sys/kernel/debug/regulator/8916_l4/enable

# Power on hardware test and led states
if [ -z "${CMDLINE##*anki.dev*}" ]; then
	is_dev_device=true
	rampost -d | tee /dev/rampost.log
else
	is_dev_device=true
	rampost -d | tee /dev/rampost.log
fi

if [ -z "${CMDLINE##*dm=*}" ]; then
	DM="${CMDLINE##*dm=\"}"
	DM="${DM%\" *}"
	DM_TABLE="${DM##*,}"
	ROOTFS_OPTS="-o ro,noatime,noload,exec"

	echo "Setting up DM Verity device: $DM"
	dmsetup create system -r --table "$DM_TABLE" || fatal "ERROR: dmsetup failed"
else
	set -e
	ROOTFS_OPTS="-o ro,noatime,exec"
	if ! $is_dev_device; then
		rampost -x
		exit 1;
	fi
fi

ROOT_MOUNT_POINT=/rootfs
mkdir -p "$ROOT_MOUNT_POINT"
mount $ROOTFS_OPTS "$ROOTFS" "$ROOT_MOUNT_POINT"
mount -n --move /proc "$ROOT_MOUNT_POINT/proc"
mount -n --move /sys "$ROOT_MOUNT_POINT/sys"
mount -n --move /dev "$ROOT_MOUNT_POINT/dev"

cd $ROOT_MOUNT_POINT

exec switch_root -c /dev/console $ROOT_MOUNT_POINT /sbin/init $CMDLINE || fatal "Couldn't switch_root"
