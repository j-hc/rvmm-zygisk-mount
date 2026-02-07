#!/system/bin/sh

if [ -n "$KSU" ]; then
	ui_print "* KernelSU detected. Make sure you are using a Zygisk module!"
fi

. "$MODPATH/util.sh"

chmod -R +x "$MODPATH/bin"
disable_unmount_modules_ksu

P=/data/adb/modules/rvmm-zygisk-mount
if [ -d $P ]; then
	create_procs_map $P
fi

ui_print "* Done"
ui_print "  by j-hc (github.com/j-hc)"
