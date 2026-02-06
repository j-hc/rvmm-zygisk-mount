#!/system/bin/sh

until [ "$(getprop sys.boot_completed)" = 1 ]; do sleep 1; done
until [ -d "/sdcard/Android" ]; do sleep 1; done
sleep 5

MODDIR=${0%/*}
. "$MODDIR/util.sh"

create_procs_map "$MODDIR"
