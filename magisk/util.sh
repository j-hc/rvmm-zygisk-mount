#!/system/bin/sh

create_procs_map() {
	: >"$1/procs_map"
	for RVMM in /data/adb/modules/*-jhc*; do
		if [ "$RVMM" = '/data/adb/modules/*-jhc*' ]; then break; fi
		if ! grep -Fxq "author=j-hc" "$RVMM/module.prop"; then continue; fi
		if ! grep -qF "version=" "$RVMM/module.prop" && ! grep -qF "rvp" "$RVMM/module.prop"; then continue; fi

		. "$RVMM/config"

		if ! BASEPATH=$(pm path "$PKG_NAME" 2>&1 </dev/null) || [ -z "$BASEPATH" ]; then continue; fi

		RVPATH="/data/adb/rvhc/${RVMM##*/}.apk"

		echo >&2 "$PKG_NAME $RVPATH ${BASEPATH##*:}"

		for s in "$PKG_NAME" "$RVPATH" "${BASEPATH##*:}"; do
			printf '%b%s\0' "\\x$(printf '%02x' "${#s}")" "$s" >>"$1/procs_map"
		done
	done
	printf '\0' >>"$1/procs_map"
}
