#!/bin/bash

# In case IPv6 is compiled as a module.
[ -f /proc/net/if_inet6 ] || insmod $DIR/kernel/net-next/net/ipv6/ipv6.ko

# Minimal network setup.
ip link set lo up
ip link set lo mtu 16436
ip link set eth0 up

# Allow people to run ping.
echo "0 65536" > /proc/sys/net/ipv4/ping_group_range

# Fall out to a shell once the test completes or if there's an error.
trap "exec /bin/bash" ERR EXIT

# Find and run the test.
test=$(cat /proc/cmdline | sed -re 's/.*net_test=([^ ]*).*/\1/g')
echo -e "Running $test\n"
$test
