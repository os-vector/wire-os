DEPENDS += "virtual/cross-binutils patchelf-native sfpshim"

SFPSHIM_DIR = "${RECIPE_SYSROOT}${datadir}/sfpshim"

sfpshim_patch() {
	map=${SFPSHIM_DIR}/rename.map
	fsyms=${SFPSHIM_DIR}/float-syms.txt
	renamed=$(awk '{print $1}' $map | tr '\n' '|' | sed 's/|$//')
	for f in $(find ${D} -type f); do
		[ "$(od -An -c -N4 "$f" | tr -d ' ')" = "177ELF" ] || continue
		hdr=$(${READELF} -h "$f")
		echo "$hdr" | grep -qE 'Type:[[:space:]]+(DYN|EXEC)' || continue
		echo "$hdr" | grep -qE 'Flags:[[:space:]]+0x5000000,' || continue
		if ${READELF} -d "$f" | grep -q '(NEEDED).*\[ld-linux.so.3\]'; then
			patchelf --replace-needed ld-linux.so.3 ld-linux-armhf.so.3 "$f"
		fi
		if ${READELF} -l "$f" | grep -q 'interpreter: /lib/ld-linux.so.3\]'; then
			patchelf --set-interpreter /lib/ld-linux-armhf.so.3 "$f"
		fi
		syms=$(${READELF} --dyn-syms -W "$f" | awk '$7=="UND"{print $8}' | sed 's/@.*//' | grep -xE "$renamed" | sort -u || true)
		if [ -n "$syms" ]; then
			patchelf $(for s in $syms; do printf -- '--clear-symbol-version %s ' "$s"; done) "$f"
			patchelf --rename-dynamic-symbols $map --add-needed libsfpshim.so "$f"
		fi
	done
}

do_install[postfuncs] += "sfpshim_patch"
do_install[depends] += "virtual/cross-binutils:do_populate_sysroot patchelf-native:do_populate_sysroot sfpshim:do_populate_sysroot"
do_prebuilt_install[postfuncs] += "sfpshim_patch"
do_prebuilt_install[depends] += "virtual/cross-binutils:do_populate_sysroot patchelf-native:do_populate_sysroot sfpshim:do_populate_sysroot"
