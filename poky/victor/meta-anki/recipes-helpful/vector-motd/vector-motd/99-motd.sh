if [ -z "$VECTOR_MOTD_SHOWN" ] && [ -t 0 ] && [ -t 1 ]; then
	case $- in
	*i*)
		VECTOR_MOTD_SHOWN=1
		export VECTOR_MOTD_SHOWN
		[ -x /usr/sbin/vector-motd ] && /usr/sbin/vector-motd
		;;
	esac
fi
