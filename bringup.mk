#
# TEMPORARY bring-up settings for test builds (LineageOS 16.0).
# Remove the include from lineage.mk before making daily-use builds:
# these leave USB debugging on and unauthenticated.
#

# USB debugging enabled from first boot
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.usb.config=mtp,adb

# Pie's adbd and MTP server use AIO on the ffs functionfs endpoint by
# default (system/core/adb/daemon/usb.cpp:516 and
# frameworks/av/media/mtp/MtpServer.cpp:125 both read this property).  The
# 3.10 kernel of this device does not implement the ffs AIO interface the
# way Pie expects, so the compat path has to be selected explicitly.
#
# 15.1 did not need this: the 8.1 adbd did not use AIO on functionfs.
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    sys.usb.ffs.aio_compat=1

# Keep a persistent logcat on /data/misc/logd (readable from TWRP)
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.logd.logpersistd=logcatd \
    persist.logd.size=8388608

# adb auth is disabled via WITH_ADB_INSECURE=true in v521env; vendor/lineage/config/common.mk
# is patched locally to honour it (15.1 doesn't upstream).

# lk2nd (DISABLED): built outside the Android tree, see v521env.
