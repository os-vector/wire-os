# functions and definitions for workload automation scripts
#
# See recentfling.sh, systemapps.sh, and other scripts that use
# these definitions.
#

dflttracecategories="gfx input view am rs power sched freq idle load memreclaim"
dfltAppList="gmail hangouts chrome youtube camera photos play maps calendar earth calculator sheets docs home"
generateActivities=0

# default activities. Can dynamically generate with -g.
gmailActivity='com.google.android.gm/com.google.android.gm.ConversationListActivityGmail'
hangoutsActivity='com.google.android.talk/com.google.android.talk.SigningInActivity'
chromeActivity='com.android.chrome/_not_used'
youtubeActivity='com.google.android.youtube/com.google.android.apps.youtube.app.WatchWhileActivity'
cameraActivity='com.google.android.GoogleCamera/com.android.camera.CameraActivity'
playActivity='com.android.vending/com.google.android.finsky.activities.MainActivity'
feedlyActivity='com.devhd.feedly/com.devhd.feedly.Main'
photosActivity='com.google.android.apps.plus/com.google.android.apps.photos.phone.PhotosHomeActivity'
mapsActivity='com.google.android.apps.maps/com.google.android.maps.MapsActivity'
calendarActivity='com.google.android.calendar/com.android.calendar.AllInOneActivity'
earthActivity='com.google.earth/com.google.earth.EarthActivity'
calculatorActivity='com.android.calculator2/com.android.calculator2.Calculator'
sheetsActivity='com.google.android.apps.docs.editors.sheets/com.google.android.apps.docs.app.NewMainProxyActivity'
docsActivity='com.google.android.apps.docs.editors.docs/com.google.android.apps.docs.app.NewMainProxyActivity'
operaActivity='com.opera.mini.native/com.opera.mini.android.Browser'
firefoxActivity='org.mozilla.firefox/org.mozilla.firefox.App'
homeActivity='com.google.android.googlequicksearchbox/com.google.android.launcher.GEL'

function showUsage {
	echo "$0: unrecognized option: $1"
	echo; echo "Usage: $0 [options]"
	echo "-e : stop on error"
	echo "-i iterations"
	echo "-n : keep trace files"
	echo "-o output file"
	echo "-s device : adb device"
	echo "-t trace categories"
	echo "-g : generate activity strings"
}

DEVICE=unknown

# handle args
while [ $# -gt 0 ]
do
	case "$1" in
	(-d) DEVICE=$2; shift;;
	(-e) stoponerror=1;;
	(-n) savetmpfiles=1;;
	(-t) tracecategories=$2; shift;;
	(-i) iterations=$2; shift;;
	(-o) output=$2; shift;;
	(-v) verbose=1;;
	(-nz) compress=0;;
	(-s) deviceName=$2; shift;;
	(-g) generateActivities=1;;
	(--) ;;
	(*)
		chk1=$(functions 2>/dev/null)
		chk2=$(typeset -F 2>/dev/null)

		if echo $chk1 $chk2 | grep -q processLocalOption; then
			if ! processLocalOption "$1" "$2"; then
				shift
			fi
		else
			showUsage $1
			exit 1
		fi;;
	esac
	shift
done

# check if running on a device
if ls /etc/* 2>/dev/null | grep -q android.hardware; then
	ADB=""
	compress=0
	isOnDevice=1
else
	# do a throw-away adb in case the server is out-of-date
	adb devices -l 2>&1 >/dev/null

	if [ -z "$deviceName" ]; then
		devInfo=$(adb devices -l | grep -v ^List | head -1)
	else
		devInfo=$(adb devices -l | grep $deviceName)
	fi
	set -- $devInfo
	if [ -z $1 ]; then
		echo Error: could not find device $deviceName
		exit 1
	fi
	deviceName=$1
	ADB="adb -s $deviceName shell "
	DEVICE=$(echo $4 | sed 's/product://')
	isOnDevice=0
fi

# default values if not set by options or calling script
appList=${appList:=$dfltAppList}
savetmpfiles=${savetmpfiles:=0}
stoponerror=${stoponerror:=0}
verbose=${verbose:=0}
compress=${compress:=1}
iterations=${iterations:=5}
tracecategories=${tracecategories:=$dflttracecategories}
ADB=${ADB:=""}
output=${output:="./out"}

# clear the output file
> $output

# ADB commands
AM_FORCE_START="${ADB}am start -W -S"
AM_START="${ADB}am start -W"
AM_START_NOWAIT="${ADB}am start"
AM_STOP="${ADB}am force-stop"
AM_LIST="${ADB}am stack list"
WHO="${ADB}whoami"
INPUT="${ADB}input"
PS="${ADB}ps"

function vout {
	# debug output enabled by -v
	if [ $verbose -gt 0 ]; then
	    echo DEBUG: $* >&2
	    echo DEBUG: $* >&2 >> $output
	fi
}

function findtimestamp {
	# extract timestamp from atrace log entry
	while [ "$2" != "" -a "$2" != "tracing_mark_write" ]
	do
		shift
	done
	echo $1
}

function computeTimeDiff {
	# Compute time diff given: startSeconds startNs endSeconds endNS

	# strip leading zeros
	startS=$(expr 0 + $1)
	endS=$(expr 0 + $3)
	if [ "$2" = N ]; then
		startNs=0
		endNs=0
	else
		startNs=$(expr 0 + $2)
		endNs=$(expr 0 + $4)
	fi

	((startMs=startS*1000 + startNs/1000000))
	((endMs=endS*1000 + endNs/1000000))
	((diff=endMs-startMs))
	echo $diff
}

function log2msec {
	in=$1
	in=${in:=0.0}
	set -- $(echo $in | tr . " ")
	# shell addition via (( )) doesn't like leading zeroes in msecs
	# field so remove leading zeroes
	msecfield=$(expr 0 + $2)

	((msec=$1*1000000+msecfield))
	((msec=msec/1000))
	echo $msec
}

function getStartTime {
	# extract event indicating beginning of start sequence
	# a) look for a "launching" event indicating start from scratch
	# b) look for another activity getting a pause event
	_app=$1
	traceout=$2
	ret=0
	s=$(grep "Binder.*tracing_mark_write.*launching" $traceout 2>/dev/null | head -1| tr [\(\)\[\]:] " ")
	if [ -z "$s" ]; then
		s=$(grep activityPause $traceout | head -1 2>/dev/null| tr [\(\)\[\]:] " ")
	else
		vout $_app was restarted!
		ret=1
	fi
	vout STARTLOG: $s
	log2msec $(findtimestamp $s)
	return $ret
}

function getEndTime {
	# extract event indicating end of start sequence. We use the
	# first surfaceflinger event associated with the target activity
	_app=$1
	traceout=$2
	f=$(grep "surfaceflinger.*tracing_mark_write.*$_app" $traceout 2>/dev/null |
		grep -v Starting | head -1 | tr [\(\)\[\]:] " ")
	if [ -z "$f" ]; then
		# Hmm. sf symbols may not be there... get the pid
		pid=$(${ADB}pidof /system/bin/surfaceflinger | tr "[]" "[ ]")
		f=$(grep "           <...>-$pid.*tracing_mark_write.*$_app" $traceout 2>/dev/null |
			grep -v Starting | head -1 | tr [\(\)\[\]:] " ")
	fi
	vout ENDLOG: $f
	log2msec $(findtimestamp $f)
}

function resetJankyFrames {
	_gfxapp=$1
	_gfxapp=${app:="com.android.systemui"}
	${ADB}dumpsys gfxinfo $_gfxapp reset 2>&1 >/dev/null
}

function getJankyFrames {
	_gfxapp=$1
	_gfxapp=${_gfxapp:="com.android.systemui"}

	# Note: no awk or sed on devices so have to do this
	# purely with bash
	total=0
	janky=0
	latency=0
	${ADB}dumpsys gfxinfo $_gfxapp | tr "\r" " " | egrep "9[059]th| frames" | while read line
	do
		if echo $line | grep -q "Total frames"; then
			set -- $line
			total=$4
		elif echo $line | grep -q "Janky frames"; then
			set -- $line
			janky=$3
		elif echo $line | grep -q "90th"; then
			set -- $(echo $line | tr m " ")
			l90=$3
		elif echo $line | grep -q "95th"; then
			set -- $(echo $line | tr m " ")
			l95=$3
		elif echo $line | grep -q "99th"; then
			set -- $(echo $line | tr m " ")
			l99=$3
			echo $total $janky $l90 $l95 $l99
			break
		fi
	done
}

function checkForDirectReclaim {
	# look for any reclaim events in atrace output
	_app=$1
	traceout=$2
	if grep -qi reclaim $traceout; then
	   return 1
	fi
	return 0
}

function startInstramentation {
	# Called at beginning of loop. Turn on instramentation like atrace
	vout start instramentation $(date)
	echo =============================== >> $output
	echo Before iteration >> $output
	echo =============================== >> $output
	${ADB}cat /proc/meminfo 2>&1 >> $output
	${ADB}dumpsys meminfo 2>&1 >> $output
	if [ "$user" = root ]; then
		vout ${ADB}atrace -b 32768 --async_start $tracecategories
		${ADB}atrace -b 32768 --async_start $tracecategories >> $output
		echo >> $output
	fi
}

function stopInstramentation {
	if [ "$user" = root ]; then
		vout ${ADB}atrace --async_stop
		${ADB}atrace --async_stop > /dev/null
	fi
}

function stopAndDumpInstramentation {
	# Called at beginning of loop. Turn on instramentation like atrace
	vout stop instramentation $(date)
	echo =============================== >> $output
	echo After iteration >> $output
	echo =============================== >> $output
	${ADB}cat /proc/meminfo 2>&1 >> $output
	${ADB}dumpsys meminfo 2>&1 >> $output
	if [ "$user" = root ]; then
		traceout=$1
		traceout=${traceout:=$output}
		echo =============================== >> $traceout
		echo TRACE >> $traceout
		echo =============================== >> $traceout
		if [ $compress -gt 0 ]; then
			tmpTrace=./tmptrace.$$
			UNCOMPRESS=$CMDDIR/atrace-uncompress.py
			> $tmpTrace
			zarg="-z"
			${ADB}atrace -z -b 32768 --async_dump >> $tmpTrace
			python $UNCOMPRESS $tmpTrace >> $traceout
			rm -f $tmpTrace
		else
			${ADB}atrace $zarg -b 32768 --async_dump >> $traceout
		fi
		vout ${ADB}atrace $zarg --async_dump
		vout ${ADB}atrace --async_stop
		${ADB}atrace --async_stop > /dev/null
	fi
}

function getActivityName {
	cmd="actName=\$${1}Activity"
	eval $cmd
	echo $actName
}

function getPackageName {
	set -- $(getActivityName $1 | tr "[/]" "[ ]")
	echo $1
}

function startActivityFromPackage {
	if [ "$1" = home ]; then
		doKeyevent HOME
		echo 0
		return 0
	fi
	vout $AM_START_NOWAIT -p "$(getPackageName $1)" -c android.intent.category.LAUNCHER -a android.intent.action.MAIN
	$AM_START_NOWAIT -p "$(getPackageName $1)" -c android.intent.category.LAUNCHER -a android.intent.action.MAIN 2>&1
	echo 0
}

function startActivity {
	if [ "$1" = home ]; then
		doKeyevent HOME
		echo 0
		return 0
	elif [ "$1" = chrome ]; then
		if [ "$DEVICE" = volantis ]; then
			vout $AM_START_NOWAIT -p "$(getPackageName $1)" http://www.theverge.com
			$AM_START_NOWAIT -p "$(getPackageName $1)" http://www.theverge.com > /dev/null
			set -- 0 0
		else
			vout $AM_START -p "$(getPackageName $1)" http://www.theverge.com
			set -- $($AM_START -p "$(getPackageName $1)" http://www.theverge.com | grep ThisTime)
		fi
	else
		vout $AM_START "$(getActivityName $1)"
		set -- $($AM_START "$(getActivityName $1)" | grep ThisTime)
	fi
	echo $2 | tr "[\r]" "[\n]"
}

function forceStartActivity {
	if [ "$1" = chrome ]; then
		vout $AM_START -p "$(getPackageName $1)" http://www.theverge.com
		set -- $($AM_FORCE_START -p "$(getPackageName $1)" http://www.theverge.com | grep ThisTime)
	else
		vout $AM_FORCE_START "$(getActivityName $1)"
		set -- $($AM_FORCE_START "$(getActivityName $1)" | grep ThisTime)
	fi
	echo $2 | tr "[\r]" "[\n]"
}

function checkActivity {
	# requires root
	actName="$(getActivityName $1)"
	$AM_LIST | grep $actName
}

#function stopActivity {
#    vout $AM_STOP $(getActivityName $1)
#    $AM_STOP $(getActivityName $1)
#}

function doSwipe {
	vout ${ADB}input swipe $*
	${ADB}input swipe $*
}

function doTap {
	vout ${ADB}input tap $*
	${ADB}input tap $*
}

function doKeyevent {
	vout $INPUT keyevent $*
	$INPUT keyevent $*
}

function checkIsRunning {
	p=$1
	shift
	if ! $PS | grep $p | grep -qv grep; then
	   handleError $*: $p is not running
	   exit 1
	fi
}

function checkStartTime {
	vout checkStartTime $1 v $2
	if [ -z "$2" ]; then
	    echo false
	    return 2
	fi
	if [ "$1" -gt "$2" ]; then
	    echo false
	    return 1
	fi
	echo true
	return 0
}

function handleError {
	echo Error: $*
	stopAndDumpInstramentation
	if [ $stoponerror -gt 0 ]; then
		exit 1
	fi
}

user=root
if ${ADB}ls /data 2>/dev/null | grep -q "Permission denied"; then
	user=shell
fi
vout User is $user

if [ $generateActivities -gt 0  ]; then
	if [ $isOnDevice -gt 0 ]; then
		echo Error: cannot generate activity list when run on device
		exit 1
	fi
	echo Generating activities...
	for app in $appList
	do
		startActivityFromPackage $app 2>&1 > /dev/null
		act=$(${ADB}am stack list | grep $(getPackageName $app) | sed -e 's///' | head -1 | awk '{ print $2; }')
		eval "${app}Activity=$act"
		echo "ACTIVITY: $app --> $(getActivityName $app)"
	done
fi

