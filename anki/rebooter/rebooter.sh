#!/bin/bash

# messy rebooter script :3

INHIBITOR_FILES=(/data/inhibit_reboot /run/inhibit_reboot)
INTERPRETERS=(python3)
LOCALTIME_FILE="/etc/localtime"
DEFAULT_TIMEZONE="/usr/share/zoneinfo/Universal"
UPDATER_PROCESS="/anki/bin/update-engine"

EARLIEST=${REBOOTER_EARLIEST:-3600}
LATEST=${REBOOTER_LATEST:-18000}
MIN_UPTIME=${REBOOTER_MINIMUM_UPTIME:-14400}
INHIBITOR_DELAY=${REBOOTER_INHIBITOR_DELAY:-17}
VERBOSE=${REBOOTER_VERBOSE_LOGGING:-false}

das_event(){
  /anki/bin/vic-log-event rebooter.sh "$1" "$2" "$3"
}

fail(){
  das_event "robot.maintenance_reboot" "fail" "$1"
}

status(){
  echo "$2" > "$1"
}

reboot_robot(){
  $VERBOSE && echo "Rebooting... see you on the other side or whatever"
  das_event "robot.maintenance_reboot" "success"
  status /data/maintenance_reboot 1
  /sbin/reboot
}

uptime_secs(){
  awk '{print int($1)}' /proc/uptime
}

now_secs(){
  date +%s | awk -v d="$(date +%T)" '{split(d,a,":"); print a[1]*3600 + a[2]*60 + a[3]}'
}

exit_if_too_late(){
  [ $(now_secs) -gt $LATEST ] && { echo "too late"; fail "$1"; exit 1; }
}

ps_list(){
  for pid in /proc/[0-9]*; do
    cmd=$(tr '\0' ' ' < "$pid/cmdline" 2>/dev/null)
    [ -n "$cmd" ] && echo "$cmd"
  done
}

os_update_pending(){
  [ -e /run/update-engine/done ]
}

inhibitors(){
  local list=()
  for f in "${INHIBITOR_FILES[@]}"; do [ -e "$f" ] && list+=("$f"); done
  os_update_pending && { echo "${list[@]}"; return; }
  gov_file="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
  if [ -r "$gov_file" ]; then
    gov=$(cat "$gov_file")
    [[ "$gov" != powersave* && "$gov" != userspace* ]] || list+=(powersave)
  fi
  ps_list | grep -q "$UPDATER_PROCESS" && list+=("$UPDATER_PROCESS")
  echo "${list[@]}"
}

if ! os_update_pending; then
  [ ! -e "$LOCALTIME_FILE" ] && { echo "$LOCALTIME_FILE missing"; fail no_timezone; exit 1; }
  [ "$(realpath $LOCALTIME_FILE)" = "$DEFAULT_TIMEZONE" ] && { echo "default timezone"; fail default_timezone; exit 1; }
  exit_if_too_late late
fi

[ $(now_secs) -lt $EARLIEST ] && { sleep $((EARLIEST-$(now_secs))); }

if ! os_update_pending; then
  exit_if_too_late late
  up=$(uptime_secs)
  if [ $up -lt $MIN_UPTIME ]; then
    need=$((MIN_UPTIME - up))
    max_sleep=$((LATEST - $(now_secs)))
    [ $need -gt $max_sleep ] && { echo "uptime fail"; fail uptime; exit 1; }
    sleep $need
  fi
  exit_if_too_late late
fi

max_sleep=$((LATEST - $(now_secs) - 60))
[ $max_sleep -lt 1 ] && max_sleep=1
sleep $((RANDOM % max_sleep + 1))

if ! os_update_pending; then exit_if_too_late late; fi

while [ -n "$(inhibitors)" ]; do
  exit_if_too_late "$(inhibitors)"
  sleep $INHIBITOR_DELAY
done

reboot_robot
