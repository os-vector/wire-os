#!/bin/sh

echo "anki-audio-init: START" > /dev/kmsg

# wait for /sys/class/sound/card0 to appear
# indicating that the alsa driver has detected our sound system

MAX_RETRIES=120 # 60 sec
COUNT=0
while [ ! -e /sys/class/sound/card0 ]
do
  sleep 0.5
  COUNT=$((COUNT+1))
  if [ $COUNT -ge $MAX_RETRIES ]; then
    exit 1
  fi
done

# BRC/JMR: Use amixer controls to find "numid" params

# tinymix set "PRI_MI2S_RX Audio Mixer MultiMedia1" 1
amixer cset numid=1440 1

# tinymix set "RX3 MIX1 INP1" "RX1"
amixer cset numid=1633 "RX1"

# tinymix set "SPK" 1
amixer cset numid=1641 1

# tinymix set "RX3 Digital Volume" 74
amixer cset numid=33 71
