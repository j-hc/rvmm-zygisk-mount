#!/system/bin/sh

MODDIR=${0%/*}
. "$MODDIR/util.sh"

collect_rvmm | while IFS= read -r rvmm_path; do
	: >"$rvmm_path/disable"
	[ -f "$rvmm_path/err" ] || cp -f "$rvmm_path/module.prop" "$rvmm_path/err"
	sed -i "s/^des.*/description=⚠️ Keep this module disabled. Mounting is being handled by rvmm-zygisk-mount/g" "$rvmm_path/module.prop"
done
