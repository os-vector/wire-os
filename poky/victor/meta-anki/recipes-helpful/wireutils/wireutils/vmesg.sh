#!/bin/bash

# author: Wire/@kercre123

function usage() {
	echo "usage: vmesg [-t|-c] <grep args>"
	echo "this is a helper tool for viewing Vector's /var/log/messages"
	echo "if no grep args are provided, the tailed/whole log will be given"
	echo "-t = tail (-f), -c = cat"
	echo 'example for searching: vmesg -t -i "tflite\|gpu"'
	echo 'example for whole log: vmesg -c'
	exit 1
}

if [[ ! $1 == "-c" ]] && [[ ! $1 == "-t" ]]; then
	echo "error: no arg given"
	echo
	usage
fi

if [[ $1 == "-c" ]]; then
	GRABCOMMAND="cat"
elif [[ $1 == "-t" ]]; then
	GRABCOMMAND="tail -f"
fi

if [[ ! $2 == "" ]]; then
        ${GRABCOMMAND} /var/log/messages | grep "${@:2}"
else
    	${GRABCOMMAND} /var/log/messages
fi
