#!/system/bin/sh

for RVMM in /data/adb/modules/*-jhc*; do
	if [ "$RVMM" = '/data/adb/modules/*-jhc*' ]; then break; fi
	if ! grep -Fxq "author=j-hc" "$RVMM/module.prop"; then continue; fi
	if ! grep -qF "version=" "$RVMM/module.prop" && ! grep -qF "rvp" "$RVMM/module.prop"; then continue; fi

	: >"$RVMM/disable"
	[ -f "$RVMM/err" ] || cp -f "$RVMM/module.prop" "$RVMM/err"
	sed -i "s/^des.*/description=⚠️ Keep this module disabled. Mounting is being handled by rvmm-zygisk-mount/g" "$RVMM/module.prop"
done
