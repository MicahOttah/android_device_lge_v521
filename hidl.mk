#
# HIDL HALs for LineageOS 16.0 (Android 9 Pie).
#
# The set is the one that worked in 15.1 (see git history), with three
# changes that Pie forces:
#
#   * configstore is 1.1 in Pie, not 1.0.
#   * the camera provider is the LineageOS "legacy" adapter, because the
#     camera blob is a 2016 CAF camera1 module.  The AOSP provider@2.4
#     adapter drives the module through the camera3 metadata API, which
#     that blob does not implement; the legacy adapter bridges camera1 to
#     HIDL.  Same choice as LineageOS' own CAF trees (lge g2/g3, gpad8.3).
#   * the GNSS adapter is the LineageOS "legacy" one, because the CAF GPS
#     blob reports a bigger svinfo struct (HaxxSvStatus) that includes a
#     GLONASS usage mask; the AOSP adapter only fills the GPS mask.
#
# Passthrough HALs are loaded into the client process by the HIDL passthrough
# loader (system/libhidl/transport/ServiceManagement.cpp), so they only need
# the -impl library.  HALs declared as hwbinder in manifest.xml need their
# -service binary as well, otherwise nothing ever registers with
# hwservicemanager.
#
# Must stay in sync with manifest.xml.
#

# Audio (passthrough)
PRODUCT_PACKAGES += \
    android.hardware.audio@2.0-impl \
    android.hardware.audio.effect@2.0-impl

# Bluetooth
PRODUCT_PACKAGES += \
    android.hardware.bluetooth@1.0-impl \
    android.hardware.bluetooth@1.0-service \
    libbt-vendor

# Camera (passthrough, wraps the camera.msm8952 blob)
PRODUCT_PACKAGES += \
    android.hardware.camera.provider@2.4-impl-legacy \
    camera.device@1.0-impl-legacy

# ConfigStore
PRODUCT_PACKAGES += \
    android.hardware.configstore@1.1-service

# Display
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.composer@2.1-impl \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.graphics.mapper@2.0-impl \
    android.hardware.memtrack@1.0-impl \
    android.hardware.memtrack@1.0-service

# DRM
PRODUCT_PACKAGES += \
    android.hardware.drm@1.0-impl \
    android.hardware.drm@1.0-service

# Gatekeeper (wraps the gatekeeper.msm8952 blob)
PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0-impl \
    android.hardware.gatekeeper@1.0-service

# GNSS (passthrough, wraps the gps.default blob)
#
# The ".legacy" adapter is the LineageOS one (hardware/lineage/interfaces/
# gnss/1.0-legacy): it differs from the AOSP Pie adapter only in the
# satellite-status handling, where the CAF blob needs the GLONASS mask.
PRODUCT_PACKAGES += \
    android.hardware.gnss@1.0-impl.legacy

# Health
PRODUCT_PACKAGES += \
    android.hardware.health@1.0-impl \
    android.hardware.health@1.0-service

# IR (wraps the consumerir.msm8952 blob)
#
# The blob stays at system/lib/hw/consumerir.msm8952.so: Pie's
# hardware/libhardware/hardware.c still searches /system/lib/hw for legacy
# modules, and the HAL runs in the 32-bit android.hardware.ir@1.0-service.
PRODUCT_PACKAGES += \
    android.hardware.ir@1.0-impl \
    android.hardware.ir@1.0-service

# Keymaster (wraps the keystore.msm8952 blob)
PRODUCT_PACKAGES += \
    android.hardware.keymaster@3.0-impl \
    android.hardware.keymaster@3.0-service

# Lights
PRODUCT_PACKAGES += \
    android.hardware.light@2.0-impl \
    android.hardware.light@2.0-service

# Power
PRODUCT_PACKAGES += \
    android.hardware.power@1.0-impl \
    android.hardware.power@1.0-service

# Renderscript
PRODUCT_PACKAGES += \
    android.hardware.renderscript@1.0-impl

# Sensors (wraps the sensors.v521 multi-hal)
PRODUCT_PACKAGES += \
    android.hardware.sensors@1.0-impl \
    android.hardware.sensors@1.0-service

# Soundtrigger (passthrough, loaded by the audio server)
#
# Pie has no android.hardware.soundtrigger@2.0-service binary any more, only
# the adapter library; the sound trigger service in frameworks/av loads it
# through the passthrough loader.
PRODUCT_PACKAGES += \
    android.hardware.soundtrigger@2.0-impl

# USB
PRODUCT_PACKAGES += \
    android.hardware.usb@1.0-service

# Vibrator
PRODUCT_PACKAGES += \
    android.hardware.vibrator@1.0-impl \
    android.hardware.vibrator@1.0-service \
    vibrator.default

# Wi-Fi
PRODUCT_PACKAGES += \
    android.hardware.wifi@1.0-service \
    wificond

# Trust (LineageOS USB restriction, used by KeyguardStateMonitor)
PRODUCT_PACKAGES += \
    vendor.lineage.trust@1.0-service
