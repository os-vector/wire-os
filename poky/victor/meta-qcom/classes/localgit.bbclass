#
# Helper class to handle local checked out GIT repositories cleanly 
#

# Bring in the tip SHA fetch functionality to support git local repos fully.
inherit gitsha

# Automagically move ${S} and ${O} for the user.  They can further override it, but they shouldn't have to
# do anything for most cases- and they'd best know what they're doing when they do it.
S = "${UNPACKDIR}/${PN}"
O = "${WORKDIR}/${PN}-obj"

# Explicitly bypass fetch...  It's already fetched...
do_fetch () {
}

# Override unpack for this recipe.  It's basically unpacked, but we need to 
# do a symlink into the ${WORKDIR} to the area specified by ${SRC_DIR} so that
# other stages for things like the autotools stuff works like it's supposed
# to without too many extra special interventions...
do_unpack() {
	rm -rf ${WORKDIR}/${PN}
	ln -s ${SRC_DIR} ${WORKDIR}/${PN}
}

