#
# TEMPORARY bring-up settings for test builds (LineageOS 15.1).
# Remove the include from lineage.mk before making daily-use builds:
# these leave USB debugging on and unauthenticated, and SELinux permissive.
#

# USB debugging enabled from first boot
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.usb.config=mtp,adb

# Keep a persistent logcat on /data/misc/logd (readable from TWRP)
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.logd.logpersistd=logcatd \
    persist.logd.size=8388608

# adb auth is disabled via WITH_ADB_INSECURE=true in v521env; vendor/lineage/config/common.mk
# is patched locally to honour it (15.1 doesn't upstream).

# Note: sys.usb.ffs.aio_compat (needed on 16.0 with this 3.10 kernel) is not needed here:
# 8.1 adbd does not use AIO on functionfs.

# lk2nd (DISABLED): built outside the Android tree, see v521env.
