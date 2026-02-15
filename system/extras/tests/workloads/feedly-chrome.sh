# Script to automate the following sequence:
# - Open Feedly
# - Open an article
# - Scroll to bottome
# - Open the same article in Chrome
# - Scroll the article
# - Back to Feely (should still be in memory)
# - Home screen
# ---- repeat ----
#
# Currently works on volantis only (verticle orientation)
#

CMDDIR=$(dirname $0 2>/dev/null)
CMDDIR=${CMDDIR:=.}
. $CMDDIR/defs.sh

case "$DEVICE" in
(volantis)
	echo volantis...
	feedlyArticle="500 700"
	feedlyOptions="1480 100"
	feedlyBrowserSelect="1350 650"
	feedlyArticleSwipeUp="700 700 700 50 50"
	feedlyArticleSwipeDown="700 200 700 700 50"
	chromeSwipe="700 700 700 50 50"
	;;
(shamu|*)
	echo shamu...
	feedlyArticle="676 500"
	feedlyOptions="1327 207"
	feedlyBrowserSelect="1278 1191"
	feedlyArticleSwipeUp="700 1847 700 400 50"
	feedlyArticleSwipeDown="700 400 700 1847 50"
	chromeSwipe="700 1847 700 400 50"
	;;
(hammerhead|*)
	echo "Error: No feedly screen geometry information available for $DEVICE"
	exit 1;;
esac

feedlySwitchToTime=600

# start feedly, if not installed, error out
t=$(forceStartActivity feedly)
checkIsRunning feedly "initial start of feedly"
echo Feedly start time = ${t}ms

# start chrome, if not installed, error out
t=$(forceStartActivity chrome)
checkIsRunning chrome "initial start of chrome"
echo Chrome start time = ${t}ms
sleep 1

feedlyStartTimes=0

cur=1
while [ $cur -le $iterations ]
do
	echo =======================================
	echo Iteration $cur of $iterations
	echo =======================================
	startInstramentation
	t=$(startActivity feedly)
	if [ $(checkStartTime "$t" $feedlySwitchToTime) != true ]; then
		handleError Feedly took too long to start: $t v $feedlySwitchToTime: $?
		# for now, not fatal
		# exit 1
	fi
	sleep 2
	((feedlyStartTimes=feedlyStartTimes+t))
	echo feedly started in ${t}ms
	checkIsRunning chrome "switch back to feedly"
	checkIsRunning googlequicksearchbox "switch back to feedly"

	# click on first article
	doTap $feedlyArticle
	sleep 2

	# scroll through article
	doSwipe $feedlyArticleSwipeUp
	sleep 5
	checkIsRunning chrome "feedly swipe"
	checkIsRunning googlequicksearchbox "feedly swipe"

	# scroll back to top
	doSwipe $feedlyArticleSwipeDown
	sleep 2

	# switch to chrome
	# 1. click on menu bar
	doTap $feedlyOptions
	sleep 1
	# 2. click on browser
	doTap $feedlyBrowserSelect
	sleep 10

	checkIsRunning feedly "switch to chrome"
	checkIsRunning googlequicksearchbox "switch to chrome"

	# Now we're back in chrome, swipe to bottom of article
	doSwipe $chromeSwipe
	sleep 2
	checkIsRunning feedly "swiped chrome"
	stopInstramentation
	((cur=cur+1))
done
((feedlyAve=feedlyStartTimes/iterations))
echo Avg start times: feedly: ${feedlyAve}ms

doKeyevent HOME
