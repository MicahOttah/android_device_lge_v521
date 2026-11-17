#
# Copyright (C) 2017 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# inherit from common lge
-include device/lge/common/BoardConfigCommon.mk

TARGET_OTA_ASSERT_DEVICE := v521,b3

DEVICE_PATH := device/lge/v521

# Treble
#
# This device is NOT Treble.  It has no vendor partition, it is 32-bit only,
# and its vendor blobs are 2016 CAF binaries that are not namespace clean.
#
# Pie decides this from PRODUCT_SHIPPING_API_LEVEL, which must therefore stay
# UNSET.  Leaving it unset gives, in build/make/core/config.mk:
#
#   PRODUCT_FULL_TREBLE            := false   (config.mk:783-790)
#   PRODUCT_SEPOLICY_SPLIT         := false   (config.mk:795-812)
#   PRODUCT_ENFORCE_VINTF_MANIFEST := false   (config.mk:795-812)
#   PRODUCT_USE_VNDK               := false   (config.mk:834-841)
#   PRODUCT_COMPATIBLE_PROPERTY    := false   (config.mk:769-777)
#
# Consequences:
#
#   * no platform/vendor policy split -> device/qcom/sepolicy-legacy is the
#     policy tree (see the SELinux section below);
#   * no VNDK -> prebuilts/vndk/v27 is not built and the HIDL interfaces
#     come from source (so android.system.net.netd@1.0 needs no package);
#   * no ENFORCE_VINTF_MANIFEST -> the HIDL transport falls back to the
#     passthrough loader, which is what the "passthrough" entries in
#     manifest.xml need;
#   * BOARD_BUILD_SYSTEM_ROOT_IMAGE is not demanded.  Setting
#     PRODUCT_SHIPPING_API_LEVEL >= 28 would make the build fail unless
#     BOARD_BUILD_SYSTEM_ROOT_IMAGE were also true (config.mk:880), i.e. it
#     would force a system-as-root layout.
#
# Do not set PRODUCT_SHIPPING_API_LEVEL, PRODUCT_FULL_TREBLE_OVERRIDE,
# PRODUCT_SEPOLICY_SPLIT or PRODUCT_ENFORCE_VINTF_MANIFEST anywhere.
#
# The vendor blobs go to /system/vendor (TARGET_COPY_OUT_VENDOR defaults to
# system/vendor, build/make/core/envsetup.mk:292) and init.rc links
# /vendor -> /system/vendor, so the /vendor paths in the HIDL service rc
# files and in hardware/libhardware/hardware.c still resolve.
#
# A/B (seamless updates) is likewise off: the device keeps a separate
# recovery partition, which holds TWRP during the bring-up.

#FORCE_64_BIT := true

# Platform
TARGET_BOARD_PLATFORM := msm8952
TARGET_BOARD_PLATFORM_GPU := qcom-adreno405

# Architecture
ifeq ($(FORCE_64_BIT),true)
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := cortex-a53

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv7-a-neon
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := cortex-a53

TARGET_BOARD_SUFFIX := _64
else
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_VARIANT := cortex-a53

TARGET_BOARD_SUFFIX := _32
endif

# Android 8.0+ HIDL requires the 64-bit binder protocol, also on this 32-bit kernel
# (kernel: CONFIG_ANDROID_BINDER_IPC_32BIT is not set)
TARGET_USES_64_BIT_BINDER := true

# Bootloader
TARGET_BOOTLOADER_BOARD_NAME := MSM8952
TARGET_NO_BOOTLOADER := true

# Kernel
BOARD_KERNEL_BASE := 0x80000000
BOARD_KERNEL_CMDLINE := androidboot.hardware=qcom msm_rtb.filter=0x237 ehci-hcd.park=3 androidboot.bootdevice=7824900.sdhci lpm_levels.sleep_disabled=1
# TEMPORARY bring-up: SELinux permissive until the HIDL sepolicy is complete
BOARD_KERNEL_CMDLINE += androidboot.selinux=enforcing
BOARD_KERNEL_PAGESIZE := 2048
BOARD_KERNEL_TAGS_OFFSET := 0x00000100
BOARD_RAMDISK_OFFSET     := 0x01000000
TARGET_KERNEL_SOURCE := kernel/lge/msm8952
# TARGET_KERNEL_APPEND_DTB: deprecated in 15.1; BOARD_KERNEL_IMAGE_NAME := zImage-dtb does the same
TARGET_KERNEL_CONFIG := lineageos_v521_defconfig
ifeq ($(FORCE_64_BIT),true)
TARGET_KERNEL_ARCH := arm64
TARGET_KERNEL_HEADER_ARCH := arm64
BOARD_KERNEL_IMAGE_NAME := Image.gz-dtb
else
TARGET_KERNEL_ARCH := arm
BOARD_KERNEL_IMAGE_NAME := zImage-dtb
endif

# Audio
BOARD_USES_ALSA_AUDIO := true
USE_CUSTOM_AUDIO_POLICY := 1
AUDIO_FEATURE_ENABLED_MULTI_VOICE_SESSIONS := true
AUDIO_FEATURE_ENABLED_KPI_OPTIMIZE := true
AUDIO_FEATURE_ENABLED_NEW_SAMPLE_RATE := true

# Bluetooth
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(DEVICE_PATH)/bluetooth
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_QCOM := true
BLUETOOTH_HCI_USE_MCT := true

# CMHW
# Lineage hardware (stock classes come from hardware/lineage/lineagehw/src)
BOARD_HARDWARE_CLASS += $(DEVICE_PATH)/lineagehw
BOARD_USES_CYANOGEN_HARDWARE := true

# CPUSets
ENABLE_CPUSETS := true
ENABLE_SCHEDBOOST := true

# HIDL
DEVICE_MANIFEST_FILE := $(DEVICE_PATH)/manifest.xml

# Display
BOARD_USES_ADRENO := true
NUM_FRAMEBUFFER_SURFACE_BUFFERS := 3
TARGET_USES_C2D_COMPOSITION := true
TARGET_USES_ION := true
TARGET_USES_OVERLAY := true
USE_OPENGL_RENDERER := true
MAX_EGL_CACHE_KEY_SIZE := 12*1024
MAX_EGL_CACHE_SIZE := 2048*1024
HAVE_ADRENO_SOURCE:= false
OVERRIDE_RS_DRIVER:= libRSDriver_adreno.so
TARGET_FORCE_HWC_FOR_VIRTUAL_DISPLAYS := true
VSYNC_EVENT_PHASE_OFFSET_NS := 2000000
SF_VSYNC_EVENT_PHASE_OFFSET_NS := 6000000

# Filesystem
BOARD_FLASH_BLOCK_SIZE := 131072 # (BOARD_KERNEL_PAGESIZE * 64)
BOARD_BOOTIMAGE_PARTITION_SIZE := 67108864
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 67108864
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 3095707648
BOARD_USERDATAIMAGE_PARTITION_SIZE := 10711093248
BOARD_CACHEIMAGE_PARTITION_SIZE := 914120704
TARGET_USERIMAGES_USE_EXT4 := true

# Init
TARGET_PLATFORM_DEVICE_BASE := /devices/soc.0/

# IR
TARGET_PROVIDES_CONSUMERIR := true

# Keymaster
TARGET_PROVIDES_KEYMASTER := true

# Media
TARGET_USES_MEDIA_EXTENSIONS := true

# Offline Charging
WITH_CM_CHARGER := true
BOARD_CHARGER_DISABLE_INIT_BLANK := true
BOARD_CHARGER_ENABLE_SUSPEND := false
BACKLIGHT_PATH := "/sys/class/leds/lcd-backlight/brightness"
BOARD_CHARGING_CMDLINE_NAME := "androidboot.mode"
BOARD_CHARGING_CMDLINE_VALUE := "chargerlogo"

# Power
TARGET_POWERHAL_VARIANT := qcom

# Properties
TARGET_SYSTEM_PROP := $(DEVICE_PATH)/system.prop

# Qualcomm
BOARD_USES_QCOM_HARDWARE := true
BOARD_USES_QC_TIME_SERVICES := true

# The IPA config manager comes from the device tree, not from the CAF tree
# of the LineageOS manifest.
#
# LineageOS 16.0 has the CAF data-ipa-cfg-mgr in
# vendor/qcom/opensource/data-ipa-cfg-mgr (LA.UM.7.3.r1-06900-sdm845.0),
# and the device tree has its own copy of it in data-ipa-cfg-mgr - the one
# the 15.1 build used and the one that works on this device.  Both define
# the module "ipacm", and the build stops with
#
#     error: vendor/qcom/opensource/data-ipa-cfg-mgr/ipacm/src:
#            MODULE.TARGET.EXECUTABLES.ipacm already defined by
#            device/lge/v521/data-ipa-cfg-mgr/ipacm/src
#
# The build system picks the tree through the path map of the project
# qcom-data-ipa-cfg-mgr (vendor/lineage/build/core/qcom_target.mk):
# USE_DEVICE_SPECIFIC_DATA_IPA_CFG_MGR makes it use the copy of the device
# tree, and the CAF tree is then skipped, because its Android.mk only
# builds when the path map points at it.
USE_DEVICE_SPECIFIC_DATA_IPA_CFG_MGR := true

# Recovery
# 15.1: boot fstab.qcom omits /system (first-stage mount owns it); recovery/OTA need it
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/recovery.fstab

# TWRP (WITH_TWRP=true, see bin/build-twrp): TWRP parses the fstab in its own
# column order, so it needs a TWRP-format fstab instead of the AOSP one.
# The TWRP bits are kept in their own repository
# (github.com/MicahOttah/android_device_lge_v521_twrp); clone them into
# device/lge/v521/ to build a TWRP image from this tree.
ifeq ($(WITH_TWRP),true)
  ifeq ($(wildcard $(DEVICE_PATH)/twrp.mk),)
    $(error TWRP build requested but $(DEVICE_PATH)/twrp.mk is missing - clone https://github.com/MicahOttah/android_device_lge_v521_twrp into device/lge/v521/)
  endif
  TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/twrp.fstab
  include $(DEVICE_PATH)/twrp.mk
endif

# RIL
TARGET_RIL_VARIANT := caf
# 15.1 had "BOARD_RIL_CLASS := ../../../$(DEVICE_PATH)/ril/" here, which pointed
# the build at ril/V521RIL.java.  Pie has no such mechanism any more:
# BOARD_RIL_CLASS is not read by build/make, ro.telephony.ril_class has no
# consumer in the Pie framework, and ril/V521RIL.java was never compiled even
# in 15.1 (there is no ril/Android.mk, and the class is absent from every jar
# in out/target/product/v521/system/framework).  The file is kept as a record
# of the intended CAF request translation, but it is inert.

# SELinux
# device/qcom/sepolicy is the Treble/vendor policy; this device has no vendor
# partition and ships the vendor blobs inside system/vendor, so it needs the
# legacy policy, which is the one with the upstream Pie rules for msm8952.
include device/qcom/sepolicy-legacy/sepolicy.mk
BOARD_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy

# Sensors
USE_SENSOR_MULTI_HAL := true

# Use Snapdragon LLVM, if available
TARGET_USE_SDCLANG := true

# Wifi
BOARD_HAS_QCOM_WLAN := true
BOARD_HAS_QCOM_WLAN_SDK := true
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_qcwcn
BOARD_WLAN_DEVICE := qcwcn
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_qcwcn
TARGET_USES_WCNSS_CTRL := true
TARGET_USES_QCOM_WCNSS_QMI := true
TARGET_PROVIDES_WCNSS_QMI := true
WIFI_DRIVER_FW_PATH_AP := "ap"
WIFI_DRIVER_FW_PATH_STA := "sta"
WPA_SUPPLICANT_VERSION := VER_0_8_X

# inherit from the proprietary version
-include vendor/lge/v521/BoardConfigVendor.mk
