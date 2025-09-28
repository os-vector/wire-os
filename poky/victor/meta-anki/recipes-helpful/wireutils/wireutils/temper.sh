#!/bin/bash

while true; do
    temps=()
    for i in {0..3}; do
        temps+=( $(< /sys/class/thermal/thermal_zone$i/temp) )
    done
    sum=0
    for t in "${temps[@]}"; do (( sum+=t )); done
    avg=$(awk "BEGIN{printf(\"%.1f\", $sum/${#temps[@]})}")
    printf "\rtemps: %s | avg: %s°C   " \
      "$(printf '%s°C ' "${temps[@]}")" "$avg"
    sleep 0.5
done
