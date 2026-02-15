# Capture and display input events and coordinates
#
# Usage: ./capture.sh
#

# do a throw-away adb in case the server is out-of-date
adb devices -l 2>&1 >/dev/null

while [ $# -gt 0 ]
do
	case "$1" in
	(-d) DEVICE=$2; shift;;
	(*)
		echo Unknown option $1
		exit 1;;
	esac
	shift
done

if [ "$DEVICE" = "" ]; then
	devInfo=$(adb devices -l | grep -v ^List | head -1)
	set -- $devInfo
	echo devInfo=$devInfo
	DEVICE=$(echo $4 | sed 's/product://')
fi

function convert {
	in=$1
	max=$2
	scale=$3
	if [ $max -eq 0 ]; then
		echo $in
	else
		((out=in*scale/max))
		echo $out
	fi
}


case $DEVICE in
(shamu|hammerhead|bullhead)
	# no scaling necessary
	xmax=0
	ymax=0;;
(volantis)
	xmax=3060
	xscale=1500
	ymax=2304
	yscale=1950;;
(*)
	echo "Error: No display information available for $DEVICE"
	exit 1;;
esac

echo Capturing input for $DEVICE...
stdbuf -o0 adb shell getevent -t |
	stdbuf -o0 grep "event.: 0003" |
	stdbuf -o0 grep "0003 003[0156a9]" |
	stdbuf -o0 tr ':[]' ' ' | while read line
do
	set -- $line
	code=$4
	value=$((16#$5))
	case $code in
	(0035) x=$(convert $value $xmax $xscale);;
	(0036) y=$(convert $value $ymax $yscale);;
	(0030) tag="majorTouch";;
	(0031) tag="minorTouch";;
	(003a) tag="pressure";;
	(0039) tag="trackingId";;
	(--) echo unknown code=$code;;
	esac
	printf "%-10s %-4d  %-4d\n" $tag $x $y
done
