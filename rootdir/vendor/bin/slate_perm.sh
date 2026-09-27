#!/system/bin/sh
#
# Hand /sys/class/power_supply/battery/batt_slate_mode to system:system.
#
# LineageOS battery charge control is present, VINTF-registered and correctly
# configured on this device, but it has never been able to do anything.
# vendor.lineage.health-service.default runs as user:system / group:system and is
# the only writer of that node, through
#   ChargingControl::setChargingEnabled() -> android::base::WriteStringToFile()
# The attribute is created by the kernel's SEC_BATTERY_ATTR macro as root:root
# 0664, so uid 1000 lands in "other" with r-- and the write returns EACCES. The
# HAL turns that into EX_UNSUPPORTED_OPERATION, so the feature looks enabled and
# silently does nothing:
#
#   I/LineageHealth: Current battery level: 100.0, target: 80, limit set: true
#   E/LineageHealth: Failed to set charging enabled
#   E/LineageHealth: java.lang.UnsupportedOperationException:
#         at ...IChargingControl$Stub$Proxy.setChargingEnabled
#
# The SELinux label is already correct (sysfs_battery_writable), so this is purely
# a uid/gid problem. Verified on device: after the chown, batt_slate_mode went to
# 1, the kernel logged "sec_bat_cable_work: slate mode on", the pack went to
# Discharging with charge_now=0, and the exception stopped appearing.
#
# This is a script rather than a bare "chown" line in the rc because the battery
# driver creates the node at probe time, which is not ordered against init, and a
# chown that runs too early would silently miss. Retry instead of assuming.
NODE=/sys/class/power_supply/battery/batt_slate_mode
TRIES=60

i=0
while [ "$i" -lt "$TRIES" ]; do
    if [ -e "$NODE" ]; then
        [ "$(stat -c '%U:%G' "$NODE" 2>/dev/null)" = "system:system" ] && exit 0
        chown system:system "$NODE" 2>/dev/null
        [ "$(stat -c '%U:%G' "$NODE" 2>/dev/null)" = "system:system" ] && {
            log -p d -t slate_perm "batt_slate_mode -> system:system"
            exit 0
        }
    fi
    i=$((i + 1))
    sleep 1
done

log -p e -t slate_perm "batt_slate_mode ownership fix did not take after ${TRIES}s"
exit 1
