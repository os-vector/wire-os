#! /bin/sh
# Copyright (c) 2009-2011, 2016 The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of The Linux Foundation nor
#       the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

set -e

configure_memory_parameters () {
    # Set Memory paremeters.
    #
    # Set per_process_reclaim tuning parameters
    # 2GB 64-bit will have aggressive settings when compared to 1GB 32-bit
    # 1GB and less will use vmpressure range 50-70, 2GB will use 10-70
    # 1GB and less will use 512 pages swap size, 2GB will use 1024
    #
    # Set Low memory killer minfree parameters
    # 32 bit all memory configurations will use 15K series
    # 64 bit up to 2GB with use 14K, and above 2GB will use 18K
    #
    # Set ALMK parameters (usually above the highest minfree values)
    # 32 bit will have 53K & 64 bit will have 81K
    #
    # Set ZCache parameters
    # max_pool_percent is the percentage of memory that the compressed pool
    # can occupy.
    # clear_percent is the percentage of memory at which zcache starts
    # evicting compressed pages. This should be slighlty above adj0 value.
    # clear_percent = (adj0 * 100 / avalible memory in pages)+1
    #
    arch_type=`uname -m`
    MemTotalStr=`cat /proc/meminfo | grep MemTotal`
    MemTotal=${MemTotalStr:16:8}
    MemTotalPg=$((MemTotal / 4))
    adjZeroMinFree=18432
    # Read adj series and set adj threshold for PPR and ALMK.
    # This is required since adj values change from framework to framework.
    adj_series=`cat /sys/module/lowmemorykiller/parameters/adj`
    adj_1="${adj_series#*,}"
    set_almk_ppr_adj="${adj_1%%,*}"
    # PPR and ALMK should not act on HOME adj and below.
    # Normalized ADJ for HOME is 6. Hence multiply by 6
    # ADJ score represented as INT in LMK params, actual score can be in decimal
    # Hence add 6 considering a worst case of 0.9 conversion to INT (0.9*6).
    set_almk_ppr_adj=$(((set_almk_ppr_adj * 6) + 6))
    echo $set_almk_ppr_adj > /sys/module/lowmemorykiller/parameters/adj_max_shift
    echo $set_almk_ppr_adj > /sys/module/process_reclaim/parameters/min_score_adj
    echo 1 > /sys/module/process_reclaim/parameters/enable_process_reclaim
    echo 70 > /sys/module/process_reclaim/parameters/pressure_max
    echo 30 > /sys/module/process_reclaim/parameters/swap_opt_eff
    echo 1 > /sys/module/lowmemorykiller/parameters/enable_adaptive_lmk
    if [ "$arch_type" == "aarch64" ] && [ $MemTotal -gt 2097152 ]; then
        echo 10 > /sys/module/process_reclaim/parameters/pressure_min
        echo 1024 > /sys/module/process_reclaim/parameters/per_swap_size
        echo "18432,23040,27648,32256,55296,80640" > /sys/module/lowmemorykiller/parameters/minfree
        echo 81250 > /sys/module/lowmemorykiller/parameters/vmpressure_file_min
        adjZeroMinFree=18432
    elif [ "$arch_type" == "aarch64" ] && [ $MemTotal -gt 1048576 ]; then
        echo 10 > /sys/module/process_reclaim/parameters/pressure_min
        echo 1024 > /sys/module/process_reclaim/parameters/per_swap_size
        echo "14746,18432,22118,25805,40000,55000" > /sys/module/lowmemorykiller/parameters/minfree
        echo 81250 > /sys/module/lowmemorykiller/parameters/vmpressure_file_min
        adjZeroMinFree=14746
    elif [ "$arch_type" == "aarch64" ]; then
        echo 50 > /sys/module/process_reclaim/parameters/pressure_min
        echo 512 > /sys/module/process_reclaim/parameters/per_swap_size
        echo "14746,18432,22118,25805,40000,55000" > /sys/module/lowmemorykiller/parameters/minfree
        echo 81250 > /sys/module/lowmemorykiller/parameters/vmpressure_file_min
        adjZeroMinFree=14746
    else
        echo 50 > /sys/module/process_reclaim/parameters/pressure_min
        echo 512 > /sys/module/process_reclaim/parameters/per_swap_size
        echo "15360,19200,23040,26880,34415,43737" > /sys/module/lowmemorykiller/parameters/minfree
        echo 53059 > /sys/module/lowmemorykiller/parameters/vmpressure_file_min
        adjZeroMinFree=15360
    fi
    clearPercent=$((((adjZeroMinFree * 100) / MemTotalPg) + 1))
    echo $clearPercent > /sys/module/zcache/parameters/clear_percent
    echo 30 >  /sys/module/zcache/parameters/max_pool_percent

    # Zram disk - 512MB size
    zram_enable=`getprop ro.config.zram`
    if [ "$zram_enable" == "true" ] && [ -f /dev/block/zram0 ]; then
        echo 536870912 > /sys/block/zram0/disksize
        mkswap /dev/block/zram0
        swapon /dev/block/zram0 -p 32758
    fi

    SWAP_ENABLE_THRESHOLD=1048576
    swap_enable=`getprop ro.config.swap`

    if [ -f /sys/devices/soc0/soc_id ]; then
        soc_id=`cat /sys/devices/soc0/soc_id`
    else
        soc_id=`cat /sys/devices/system/soc/soc0/id`
    fi

    # Enable swap initially only for 1 GB targets
    if [ "$MemTotal" -le "$SWAP_ENABLE_THRESHOLD" ] && [ "$swap_enable" == "true" ]; then
        # Static swiftness
        echo 1 > /proc/sys/vm/swap_ratio_enable
        echo 70 > /proc/sys/vm/swap_ratio

        # Swap disk - 200MB size
        if [ ! -f /data/system/swap/swapfile ]; then
            dd if=/dev/zero of=/data/system/swap/swapfile bs=1m count=200
        fi
        mkswap /data/system/swap/swapfile
        swapon /data/system/swap/swapfile -p 32758
    fi
}

case "$1" in
start)
echo -n "Starting init_qcom_post_boot: "
if [ -f /sys/devices/soc0/machine ]; then
    target=`cat /sys/devices/soc0/machine | tr [:upper:] [:lower:]`
else
    target=`getprop ro.board.platform`
fi

emmc_boot=`getprop ro.boot.emmc`
case "$emmc_boot"
    in "true")
        chown -h system /sys/devices/platform/rs300000a7.65536/force_sync
        chown -h system /sys/devices/platform/rs300000a7.65536/sync_sts
        chown -h system /sys/devices/platform/rs300100a7.65536/force_sync
        chown -h system /sys/devices/platform/rs300100a7.65536/sync_sts
    ;;
esac

case "$target" in
    "msm8917" | "apq8017")
        if [ -f /sys/devices/soc0/soc_id ]; then
            soc_id=`cat /sys/devices/soc0/soc_id`
        else
            soc_id=`cat /sys/devices/system/soc/soc0/id`
        fi

        if [ -f /sys/devices/soc0/hw_platform ]; then
            hw_platform=`cat /sys/devices/soc0/hw_platform`
        else
            hw_platform=`cat /sys/devices/system/soc/soc0/hw_platform`
        fi

        case "$soc_id" in
           "303" | "307" | "308" | "309" )

                # Apply Scheduler and Governor settings for 8917

                # HMP scheduler settings
                echo 3 > /proc/sys/kernel/sched_window_stats_policy
                echo 3 > /proc/sys/kernel/sched_ravg_hist_size
                echo 1 > /proc/sys/kernel/sched_restrict_tasks_spread

                #disable sched_boost in 8917
                if [ -f /proc/sys/kernel/sched_boost ]; then
                    boost=`cat /proc/sys/kernel/sched_boost`
                    if [ $boost != 0 ] ; then
                        echo 0 > /proc/sys/kernel/sched_boost
                    fi
                fi

                # HMP Task packing settings
                echo 20 > /proc/sys/kernel/sched_small_task
                echo 30 > /sys/devices/system/cpu/cpu0/sched_mostly_idle_load
                echo 30 > /sys/devices/system/cpu/cpu1/sched_mostly_idle_load
                echo 30 > /sys/devices/system/cpu/cpu2/sched_mostly_idle_load
                echo 30 > /sys/devices/system/cpu/cpu3/sched_mostly_idle_load

                echo 3 > /sys/devices/system/cpu/cpu0/sched_mostly_idle_nr_run
                echo 3 > /sys/devices/system/cpu/cpu1/sched_mostly_idle_nr_run
                echo 3 > /sys/devices/system/cpu/cpu2/sched_mostly_idle_nr_run
                echo 3 > /sys/devices/system/cpu/cpu3/sched_mostly_idle_nr_run

                echo 0 > /sys/devices/system/cpu/cpu0/sched_prefer_idle
                echo 0 > /sys/devices/system/cpu/cpu1/sched_prefer_idle
                echo 0 > /sys/devices/system/cpu/cpu2/sched_prefer_idle
                echo 0 > /sys/devices/system/cpu/cpu3/sched_prefer_idle

                # core_ctl is not needed for 8917. Disable it.
                if [ -f /sys/devices/system/cpu/cpu0/core_ctl/disable ]; then
                    echo 1 > /sys/devices/system/cpu/cpu0/core_ctl/disable
                fi

                for devfreq_gov in /sys/class/devfreq/soc:qcom,mincpubw*/governor
                do
                    node=`cat $devfreq_gov`
                    if [ $node != "cpufreq" ] ; then
                        echo "cpufreq" > $devfreq_gov
                    fi
                done

                for devfreq_gov in /sys/class/devfreq/soc:qcom,cpubw/governor
                do
                    node=`cat $devfreq_gov`
                    if [ $node != "bw_hwmon" ] ; then
                        echo "bw_hwmon" > $devfreq_gov
                    fi
                    for cpu_io_percent in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/io_percent
                    do
                        echo 20 > $cpu_io_percent
                    done

                for cpu_guard_band in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/guard_band_mbps
                    do
                        echo 30 > $cpu_guard_band
                    done
                done

                # disable thermal core_control to update interactive gov settings
                if [ -f /sys/module/msm_thermal/core_control/enabled ]; then
                     echo 0 > /sys/module/msm_thermal/core_control/enabled
                fi

                echo 1 > /sys/devices/system/cpu/cpu0/online
                echo "interactive" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
                echo "19000 1094400:39000" > /sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay
                echo 85 > /sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load
                echo 20000 > /sys/devices/system/cpu/cpufreq/interactive/timer_rate
                echo 1094400 > /sys/devices/system/cpu/cpufreq/interactive/hispeed_freq
                echo 0 > /sys/devices/system/cpu/cpufreq/interactive/io_is_busy
                echo "1 960000:85 1094400:90" > /sys/devices/system/cpu/cpufreq/interactive/target_loads
                echo 40000 > /sys/devices/system/cpu/cpufreq/interactive/min_sample_time
                echo 960000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq

                # re-enable thermal core_control now
                if [ -f /sys/module/msm_thermal/core_control/enabled ]; then
                     echo 1 > /sys/module/msm_thermal/core_control/enabled
                fi
                # Disable L2-GDHS low power modes
                echo N > /sys/module/lpm_levels/perf/perf-l2-gdhs/idle_enabled
                echo N > /sys/module/lpm_levels/perf/perf-l2-gdhs/suspend_enabled

                # Bring up all cores online
                echo 1 > /sys/devices/system/cpu/cpu1/online
                echo 1 > /sys/devices/system/cpu/cpu2/online
                echo 1 > /sys/devices/system/cpu/cpu3/online

                # Enable low power modes
                echo 0 > /sys/module/lpm_levels/parameters/sleep_disabled
                echo mem > /sys/power/autosleep

                # Enable sched guided freq control
                echo 1 > /sys/devices/system/cpu/cpufreq/interactive/use_sched_load
                echo 1 > /sys/devices/system/cpu/cpufreq/interactive/use_migration_notif
                echo 50000 > /proc/sys/kernel/sched_freq_inc_notify
                echo 50000 > /proc/sys/kernel/sched_freq_dec_notify

                # Enable dynamic clock gating
                if [ -f /sys/module/lpm_levels/lpm_workarounds/dynamic_clock_gating ]; then
                     echo 1 > /sys/module/lpm_levels/lpm_workarounds/dynamic_clock_gating
                fi
                # Enable timer migration to little cluster
                if [ -f /proc/sys/kernel/power_aware_timer_migration ]; then
                     echo 1 > /proc/sys/kernel/power_aware_timer_migration
                fi
                # Set Memory parameters
                #configure_memory_parameters
                ;;
                *)
                ;;
        esac
    ;;
esac


case "$target" in
    "msm8953" | "apq8053" )

        echo 128 > /sys/block/mmcblk0/bdi/read_ahead_kb
        echo 128 > /sys/block/mmcblk0/queue/read_ahead_kb
        setprop sys.post_boot.parsed 1

        if [ -f /sys/devices/soc0/soc_id ]; then
            soc_id=`cat /sys/devices/soc0/soc_id`
        else
            soc_id=`cat /sys/devices/system/soc/soc0/id`
        fi

        if [ -f /sys/devices/soc0/hw_platform ]; then
            hw_platform=`cat /sys/devices/soc0/hw_platform`
        else
            hw_platform=`cat /sys/devices/system/soc/soc0/hw_platform`
        fi

        case "$soc_id" in
            "293" | "304" )

                #scheduler settings
                echo 3 > /proc/sys/kernel/sched_window_stats_policy
                echo 3 > /proc/sys/kernel/sched_ravg_hist_size
                #task packing settings
                echo 0 > /sys/devices/system/cpu/cpu0/sched_static_cpu_pwr_cost
                echo 0 > /sys/devices/system/cpu/cpu1/sched_static_cpu_pwr_cost
                echo 0 > /sys/devices/system/cpu/cpu2/sched_static_cpu_pwr_cost
                echo 0 > /sys/devices/system/cpu/cpu3/sched_static_cpu_pwr_cost
                echo 0 > /sys/devices/system/cpu/cpu4/sched_static_cpu_pwr_cost
                echo 0 > /sys/devices/system/cpu/cpu5/sched_static_cpu_pwr_cost
                echo 0 > /sys/devices/system/cpu/cpu6/sched_static_cpu_pwr_cost
                echo 0 > /sys/devices/system/cpu/cpu7/sched_static_cpu_pwr_cost

                #init task load, restrict wakeups to preferred cluster
                echo 15 > /proc/sys/kernel/sched_init_task_load
                # spill load is set to 100% by default in the kernel
                echo 3 > /proc/sys/kernel/sched_spill_nr_run
                # Apply inter-cluster load balancer restrictions
                echo 1 > /proc/sys/kernel/sched_restrict_cluster_spill

                for devfreq_gov in /sys/class/devfreq/soc:qcom,mincpubw*/governor
                do
                    node=`cat $devfreq_gov`
                    if [ $node != "cpufreq" ] ; then
                        echo "cpufreq" > $devfreq_gov
                    fi
                done

                for devfreq_gov in /sys/class/devfreq/soc:qcom,cpubw/governor
                do
                    node=`cat $devfreq_gov`
                    if [ $node != "bw_hwmon" ] ; then
                        echo "bw_hwmon" > $devfreq_gov
                    fi
                    for cpu_io_percent in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/io_percent
                    do
                        echo 34 > $cpu_io_percent
                    done
                    for cpu_guard_band in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/guard_band_mbps
                    do
                        echo 0 > $cpu_guard_band
                    done
                    for cpu_hist_memory in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/hist_memory
                    do
                        echo 20 > $cpu_hist_memory
                    done
                    for cpu_hyst_length in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/hyst_length
                    do
                        echo 10 > $cpu_hyst_length
                    done
                    for cpu_idle_mbps in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/idle_mbps
                    do
                        echo 1600 > $cpu_idle_mbps
                    done
                    for cpu_low_power_delay in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/low_power_delay
                    do
                        echo 20 > $cpu_low_power_delay
                    done
                    for cpu_low_power_io_percent in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/low_power_io_percent
                    do
                        echo 34 > $cpu_low_power_io_percent
                    done
                    for cpu_mbps_zones in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/mbps_zones
                    do
                        echo "1611 3221 5859 6445 7104" > $cpu_mbps_zones
                    done
                    for cpu_sample_ms in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/sample_ms
                    do
                        echo 4 > $cpu_sample_ms
                    done
                    for cpu_up_scale in /sys/class/devfreq/soc:qcom,cpubw/bw_hwmon/up_scale
                    do
                        echo 250 > $cpu_up_scale
                    done
                    for cpu_min_freq in /sys/class/devfreq/soc:qcom,cpubw/min_freq
                    do
                        echo 1611 > $cpu_min_freq
                    done
                done

                # Configure DCC module to capture critical register contents when device crashes
                for DCC_PATH in /sys/bus/platform/devices/*.dcc*
                do
                    if [ -f $DCC_PATH/enable ] ; then
                        echo  0 > $DCC_PATH/enable
                        echo cap >  $DCC_PATH/func_type
                        echo sram > $DCC_PATH/data_sink
                        echo  1 > $DCC_PATH/config_reset
                        # Register specifies APC CPR closed-loop settled voltage for current voltage corner
                        echo 0xb1d2c18 1 > $DCC_PATH/config

                        # Register specifies SW programmed open-loop voltage for current voltage corner
                        echo 0xb1d2900 1 > $DCC_PATH/config

                        # Register specifies APM switch settings and APM FSM state
                        echo 0xb1112b0 1 > $DCC_PATH/config

                        # Register specifies CPR mode change state and also #online cores input to CPR HW
                        echo 0xb018798 1 > $DCC_PATH/config

                        echo  1 > $DCC_PATH/enable
                    fi
                done

                # disable thermal & BCL core_control to update interactive gov settings
                echo 0 > /sys/module/msm_thermal/core_control/enabled
                #governor settings
                echo 1 > /sys/devices/system/cpu/cpu0/online
                echo "interactive" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
                echo "19000 1401600:39000" > /sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay
                echo 85 > /sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load
                echo 20000 > /sys/devices/system/cpu/cpufreq/interactive/timer_rate
                echo 1401600 > /sys/devices/system/cpu/cpufreq/interactive/hispeed_freq
                echo 0 > /sys/devices/system/cpu/cpufreq/interactive/io_is_busy
                echo "85 1401600:90 1958400:95" > /sys/devices/system/cpu/cpufreq/interactive/target_loads
                echo 19000 > /sys/devices/system/cpu/cpufreq/interactive/min_sample_time
                echo 652800 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq

                # re-enable thermal & BCL core_control now
                echo 1 > /sys/module/msm_thermal/core_control/enabled
                # Bring up all cores online
                echo 1 > /sys/devices/system/cpu/cpu1/online
                echo 1 > /sys/devices/system/cpu/cpu2/online
                echo 1 > /sys/devices/system/cpu/cpu3/online
                echo 1 > /sys/devices/system/cpu/cpu4/online
                echo 1 > /sys/devices/system/cpu/cpu5/online
                echo 1 > /sys/devices/system/cpu/cpu6/online
                echo 1 > /sys/devices/system/cpu/cpu7/online

                # Enable low power modes
                echo 0 > /sys/module/lpm_levels/parameters/sleep_disabled

                # SMP scheduler
                echo 85 > /proc/sys/kernel/sched_upmigrate
                echo 85 > /proc/sys/kernel/sched_downmigrate
                echo 19 > /proc/sys/kernel/sched_upmigrate_min_nice

                # Enable sched guided freq control
                echo 1 > /sys/devices/system/cpu/cpufreq/interactive/use_sched_load
                echo 1 > /sys/devices/system/cpu/cpufreq/interactive/use_migration_notif
                echo 200000 > /proc/sys/kernel/sched_freq_inc_notify
                echo 200000 > /proc/sys/kernel/sched_freq_dec_notify

                # Set Memory parameters
                configure_memory_parameters
        ;;
        esac
     echo mem > /sys/power/autosleep
     ;;

    "msm8996" | "apq8096" | "msm8996pro" )
        # disable thermal bcl hotplug to switch governor
        echo 0 > /sys/module/msm_thermal/core_control/enabled
        if [ -f /sys/devices/soc/soc:qcom,bcl/mode ] && [ -f /sys/devices/soc/soc:qcom,bcl/hotplug_mask ]; then
            echo -n disable > /sys/devices/soc/soc:qcom,bcl/mode
            bcl_hotplug_mask=`cat /sys/devices/soc/soc:qcom,bcl/hotplug_mask`
            echo 0 > /sys/devices/soc/soc:qcom,bcl/hotplug_mask
            bcl_soc_hotplug_mask=`cat /sys/devices/soc/soc:qcom,bcl/hotplug_soc_mask`
            echo 0 > /sys/devices/soc/soc:qcom,bcl/hotplug_soc_mask 
            echo -n enable > /sys/devices/soc/soc:qcom,bcl/mode
        fi

        # Enable Adaptive LMK
        echo 1 > /sys/module/lowmemorykiller/parameters/enable_adaptive_lmk
        echo 81250 > /sys/module/lowmemorykiller/parameters/vmpressure_file_min
        # configure governor settings for little cluster
        echo "interactive" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
        echo 1 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/use_sched_load
        echo 1 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/use_migration_notif
        echo 19000 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/above_hispeed_delay
        echo 90 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/go_hispeed_load
        echo 20000 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/timer_rate
        echo 960000 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/hispeed_freq
        echo 1 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/io_is_busy
        echo 80 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/target_loads
        echo 39000 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/min_sample_time
        echo 79000 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/max_freq_hysteresis
        echo 300000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq
        echo 1 > /sys/devices/system/cpu/cpu0/cpufreq/interactive/ignore_hispeed_on_notif
        # online CPU2
        echo 1 > /sys/devices/system/cpu/cpu2/online
        # configure governor settings for big cluster
        echo "interactive" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
        echo 1 > /sys/devices/system/cpu/cpu2/cpufreq/interactive/use_sched_load
        echo 1 > /sys/devices/system/cpu/cpu2/cpufreq/interactive/use_migration_notif
        echo "19000 1400000:39000 1700000:19000 2100000:79000" > /sys/devices/system/cpu/cpu2/cpufreq/interactive/above_hispeed_delay
        echo 90 > /sys/devices/system/cpu/cpu2/cpufreq/interactive/go_hispeed_load
        echo 20000 > /sys/devices/system/cpu/cpu2/cpufreq/interactive/timer_rate
        echo 1248000 > /sys/devices/system/cpu/cpu2/cpufreq/interactive/hispeed_freq
        echo 1 > /sys/devices/system/cpu/cpu2/cpufreq/interactive/io_is_busy
        echo "85 1500000:90 1800000:70 2100000:95" > /sys/devices/system/cpu/cpu2/cpufreq/interactive/target_loads
        echo 39000 > /sys/devices/system/cpu/cpu2/cpufreq/interactive/min_sample_time
        echo 79000 > /sys/devices/system/cpu/cpu2/cpufreq/interactive/max_freq_hysteresis
        echo 300000 > /sys/devices/system/cpu/cpu2/cpufreq/scaling_min_freq
        echo 1 > /sys/devices/system/cpu/cpu2/cpufreq/interactive/ignore_hispeed_on_notif
        # re-enable thermal and BCL hotplug
        echo 1 > /sys/module/msm_thermal/core_control/enabled
        if [ -f /sys/devices/soc/soc:qcom,bcl/mode ] && [ -f /sys/devices/soc/soc:qcom,bcl/hotplug_mask ]; then
            echo -n disable > /sys/devices/soc/soc:qcom,bcl/mode
            echo $bcl_hotplug_mask > /sys/devices/soc/soc:qcom,bcl/hotplug_mask
            echo $bcl_soc_hotplug_mask > /sys/devices/soc/soc:qcom,bcl/hotplug_soc_mask
            echo -n enable > /sys/devices/soc/soc:qcom,bcl/mode
        fi

        # input boost configuration
        echo "0:1324800 2:1324800" > /sys/module/cpu_boost/parameters/input_boost_freq
        echo 40 > /sys/module/cpu_boost/parameters/input_boost_ms
        # Setting b.L scheduler parameters
        echo 45 > /proc/sys/kernel/sched_downmigrate
        echo 45 > /proc/sys/kernel/sched_upmigrate
        echo 400000 > /proc/sys/kernel/sched_freq_inc_notify
        echo 400000 > /proc/sys/kernel/sched_freq_dec_notify
        echo 3 > /proc/sys/kernel/sched_spill_nr_run
        echo 100 > /proc/sys/kernel/sched_init_task_load

        # Enable bus-dcvs
        for cpubw in /sys/class/devfreq/*qcom,cpubw*
        do
            echo "bw_hwmon" > $cpubw/governor
            echo 50 > $cpubw/polling_interval
            echo 1525 > $cpubw/min_freq
            echo "1525 5195 11863 13763" > $cpubw/bw_hwmon/mbps_zones
            echo 4 > $cpubw/bw_hwmon/sample_ms
            echo 34 > $cpubw/bw_hwmon/io_percent
            echo 20 > $cpubw/bw_hwmon/hist_memory
            echo 10 > $cpubw/bw_hwmon/hyst_length
            echo 0 > $cpubw/bw_hwmon/low_power_ceil_mbps
            echo 34 > $cpubw/bw_hwmon/low_power_io_percent
            echo 20 > $cpubw/bw_hwmon/low_power_delay
            echo 0 > $cpubw/bw_hwmon/guard_band_mbps
            echo 250 > $cpubw/bw_hwmon/up_scale
            echo 1600 > $cpubw/bw_hwmon/idle_mbps
        done

        for memlat in /sys/class/devfreq/*qcom,memlat-cpu*
        do
            echo "mem_latency" > $memlat/governor
            echo 10 > $memlat/polling_interval
        done

        soc_revision=`cat /sys/devices/soc0/revision`
        if [ "$soc_revision" == "2.0" ]; then
            #Disable suspend for v2.0
            echo pwr_dbg > /sys/power/wake_lock
        elif [ "$soc_revision" == "2.1" ]; then
            # Enable C4.D4.E4.M3 LPM modes
            # Disable D3 state
            echo 0 > /sys/module/lpm_levels/system/pwr/pwr-l2-gdhs/idle_enabled
            echo 0 > /sys/module/lpm_levels/system/perf/perf-l2-gdhs/idle_enabled
            # Disable DEF-FPC mode
            echo N > /sys/module/lpm_levels/system/pwr/cpu0/fpc-def/idle_enabled
            echo N > /sys/module/lpm_levels/system/pwr/cpu1/fpc-def/idle_enabled
            echo N > /sys/module/lpm_levels/system/perf/cpu2/fpc-def/idle_enabled
            echo N > /sys/module/lpm_levels/system/perf/cpu3/fpc-def/idle_enabled
        else
            # Enable all LPMs by default
            # This will enable C4, D4, D3, E4 and M3 LPMs
            echo N > /sys/module/lpm_levels/parameters/sleep_disabled
        fi
        echo N > /sys/module/lpm_levels/parameters/sleep_disabled

        # Tweak background writeout
        echo 200 > /proc/sys/vm/dirty_expire_centisecs
        echo 5 > /proc/sys/vm/dirty_background_ratio

        # Set panic as hung and keep SLPI alive during panic
        if [ -f /proc/sys/kernel/panic ]; then
                echo 0 > /proc/sys/kernel/panic
        fi
        if [ -f /sys/devices/soc/1c00000.qcom,ssc/subsys4/keep_alive ]; then
                echo 1 > /sys/devices/soc/1c00000.qcom,ssc/subsys4/keep_alive
        fi

        #making ssh to login as admin
        setsebool -P ssh_sysadm_login 1
    ;;
esac

case "$target" in
    "apq8009" | "msm8909" )
        ProductName="robot"
        case $ProductName in
            *robot*)
                # HMP scheduler settings for 8909 similiar to 8916
                #echo 2 > /proc/sys/kernel/sched_window_stats_policy
                #echo 3 > /proc/sys/kernel/sched_ravg_hist_size

                # Apply governor settings for 8909

                # disable thermal core_control to update scaling_min_freq
                echo 0 > /sys/module/msm_thermal/core_control/enabled
                echo 1 > /sys/devices/system/cpu/cpu0/online
                echo "interactive" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
                echo 400000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq
                # enable thermal core_control now
                echo 1 > /sys/module/msm_thermal/core_control/enabled

                echo "25000" > /sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay
                echo 90 > /sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load
                echo 30000 > /sys/devices/system/cpu/cpufreq/interactive/timer_rate
                echo 800000 > /sys/devices/system/cpu/cpufreq/interactive/hispeed_freq
                echo 0 > /sys/devices/system/cpu/cpufreq/interactive/io_is_busy
                echo "1 400000:85 998400:90 1094400:80" > /sys/devices/system/cpu/cpufreq/interactive/target_loads
                echo 50000 > /sys/devices/system/cpu/cpufreq/interactive/min_sample_time

                # Bring up all cores online
                echo 1 > /sys/devices/system/cpu/cpu1/online
                echo 1 > /sys/devices/system/cpu/cpu2/online
                echo 1 > /sys/devices/system/cpu/cpu3/online
                echo N > /sys/module/lpm_levels/parameters/sleep_disabled

                for cpubw in /sys/class/devfreq/*qcom,cpubw*
                do
                    echo "bw_hwmon" > $cpubw/governor
                done
                ;;
            *)
                echo 1 > /sys/devices/system/cpu/cpu0/online
                echo "interactive" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
                # Enable all LPMs by default
                echo N > /sys/module/lpm_levels/parameters/sleep_disabled
                ;;
        esac
;;
esac

# disable msm thermal driver
echo 0 > /sys/module/msm_thermal/core_control/enabled
echo N > /sys/module/msm_thermal/parameters/enabled
#sleep 0.1

#echo 1267200 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
#echo disabled > /sys/kernel/debug/msm_otg/bus_voting  # This prevents USB from pinning RAM to 400MHz
#echo 0 > /sys/kernel/debug/msm-bus-dbg/shell-client/update_request
#echo 1 > /sys/kernel/debug/msm-bus-dbg/shell-client/mas
#echo 512 > /sys/kernel/debug/msm-bus-dbg/shell-client/slv
#echo 0 > /sys/kernel/debug/msm-bus-dbg/shell-client/ab
#echo active clk2 0 1 max 800000 > /sys/kernel/debug/rpm_send_msg/message # Max RAM freq in KHz = 400MHz
#echo 1 > /sys/kernel/debug/msm-bus-dbg/shell-client/update_request


echo "init_qcom_post_boot completed"
;;
stop)
    echo -n "Stopping init_qcom_post_boot: "
    echo "done"
;;
restart)
    $0 stop
    $0 start
;;
*)
    echo "Incorrect option specified"
    exit 1
;;
esac

export MODULE_BASE=/lib/modules/`uname -r`
case "$1" in
start)
    insmod $MODULE_BASE/extra/wlan.ko
esac
