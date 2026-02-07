#!/system/bin/sh

collect_rvmm() {
	for RVMM in /data/adb/modules/*-jhc*; do
		if [ "$RVMM" = '/data/adb/modules/*-jhc*' ]; then break; fi
		if ! grep -Fxq "author=j-hc" "$RVMM/module.prop"; then continue; fi
		if [ ! -f "$RVMM/config" ]; then continue; fi
		echo "$RVMM"
	done
}

create_procs_map() {
	: >"$1/procs_map"
	collect_rvmm | while IFS= read -r rvmm_path; do
		. "$rvmm_path/config"

		if ! BASEPATH=$(pm path "$PKG_NAME" 2>&1 </dev/null) || [ -z "$BASEPATH" ]; then continue; fi
		RVPATH="/data/adb/rvhc/${rvmm_path##*/}.apk"

		for s in "$PKG_NAME" "$RVPATH" "${BASEPATH##*:}"; do
			printf '%b%s\0' "\\x$(printf '%02x' "${#s}")" "$s" >>"$1/procs_map"
		done

		ui_print "* Applied for $PKG_NAME"
	done
	printf '\0' >>"$1/procs_map"
}

disable_unmount_modules_ksu() {
	if [ -z "$KSU" ]; then return 0; fi

	collect_rvmm | while IFS= read -r rvmm_path; do
		. "$rvmm_path/config"

		uid=$(dumpsys package "$PKG_NAME" 2>&1 | grep -m1 "uid")
		uid=${uid#*=} uid=${uid%% *}
		if [ -z "$uid" ]; then
			uid=$(dumpsys package "$PKG_NAME" 2>&1 | grep -m1 userId)
			uid=${uid#*=} uid=${uid%% *}
		fi
		if [ -z "$uid" ]; then
			ui_print "* UID could not be found for $PKG_NAME"
			return 1
		fi

		if ! OP=$("${MODPATH:?}/bin/$ARCH/ksu_profile" "$uid" "$PKG_NAME" 2>&1); then
			ui_print "ERROR ksu_profile: $OP"
		fi
	done
}
