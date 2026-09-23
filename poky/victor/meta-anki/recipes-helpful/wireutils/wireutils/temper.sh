#!/bin/bash

while true; do
    raw=()
    for i in {0..3}; do
        raw+=( $(< /sys/class/thermal/thermal_zone$i/temp) )
    done
    sum=0
    temps=()
    for t in "${raw[@]}"; do
        (( sum+=t ))
        temps+=( $(awk "BEGIN{printf(\"%.1f\", $t/1000)}") )
    done
    avg=$(awk "BEGIN{printf(\"%.1f\", $sum/${#raw[@]}/1000)}")
    printf "\rtemps: %s | avg: %s°C   " \
      "$(printf '%s°C ' "${temps[@]}")" "$avg"
    sleep 0.5
done
