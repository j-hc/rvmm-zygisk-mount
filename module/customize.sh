#!/system/bin/sh

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

chmod -R +x "$MODPATH/bin"
disable_unmount_modules_ksu
P=/data/adb/modules/rvmm-zygisk-mount
if [ -d $P ]; then
	create_procs_map $P
fi

chmod +x "$MODPATH/service.sh"
REAPPLY=/data/data/com.termux/files/usr/bin/
if [ -d $REAPPLY ]; then
	echo "su -c 'MODDIR=$MODPATH /data/adb/modules/rvmm-zygisk-mount/service.sh'; echo Done.;" >$REAPPLY/rvmm-zygisk-mount
	chmod 777 $REAPPLY/rvmm-zygisk-mount
fi

ui_print "* Done"
ui_print "  by j-hc (github.com/j-hc)"
