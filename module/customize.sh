#!/system/bin/sh

MODDIR="$MODPATH"
if [ -n "$KSU" ]; then
	ui_print "* KernelSU detected. Make sure you are using a Zygisk module!"
fi

. "$MODPATH/util.sh"

if [ -z "$(collect_rvmm)" ]; then
	ui_print "* No revanced-magisk-module is installed."
	ui_print "  Go install the modules you want first,"
	ui_print "  then flash this module."
	abort ""
fi

chmod -R +x "$MODPATH/bin" 2>/dev/null || :
disable_unmount_modules "$MODPATH"

create_procs_map "$MODPATH"
if [ -d /data/adb/modules/rvmm-zygisk-mount ]; then
	create_procs_map /data/adb/modules/rvmm-zygisk-mount
fi

chmod +x "$MODPATH/service.sh"

ui_print "* Done"
ui_print "  by j-hc (github.com/j-hc)"
