#!/bin/bash

# Minimal network initialization.
ip link set eth0 up

# Wait for autoconf and DAD to complete.
sleep 3 &

# Block on starting DHCPv4.
udhcpc -i eth0

# If DHCPv4 took less than 3 seconds, keep waiting.
wait

# Run the test.
$(dirname $0)/ping6_test.py
