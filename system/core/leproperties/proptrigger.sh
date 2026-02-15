#!/bin/sh
# Copyright (c) 2017, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

case "$1" in
"qmmf.power.hint.on")
    case "$2" in
    "true" )
        echo 0 > /sys/devices/system/cpu/cpufreq/interactive/use_migration_notif
        echo 0 > /sys/devices/system/cpu/cpufreq/interactive/use_sched_load
        echo "85 1401600:95 2016000:99" > /sys/devices/system/cpu/cpufreq/interactive/target_loads
        echo 40000 > /sys/devices/system/cpu/cpufreq/interactive/timer_rate

        for cpu_io_percent in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/io_percent
        do
                echo 50 > $cpu_io_percent
        done

        for cpu_sample_ms in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/sample_ms
        do
                echo 10 > $cpu_sample_ms
        done
        ;;
    *)
        echo 1 > /sys/devices/system/cpu/cpufreq/interactive/use_migration_notif
        echo 1 > /sys/devices/system/cpu/cpufreq/interactive/use_sched_load
        echo "85 1401600:90 1958400:95" > /sys/devices/system/cpu/cpufreq/interactive/target_loads
        echo 20000 > /sys/devices/system/cpu/cpufreq/interactive/timer_rate

        for cpu_io_percent in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/io_percent
        do
                echo 34 > $cpu_io_percent
        done

        for cpu_sample_ms in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/sample_ms
        do
                echo 4 > $cpu_sample_ms
        done

        ;;
    esac
    ;;
*)
    echo "no triggers for $1 val: $2"
    ;;
esac
