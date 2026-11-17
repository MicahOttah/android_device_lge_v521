$(call inherit-product, device/lge/v521/full_v521.mk)

# Inherit some common Lineage stuff.
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

PRODUCT_NAME := lineage_v521
BOARD_VENDOR := lge
TARGET_VENDOR := lge
PRODUCT_DEVICE := v521

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRODUCT_DEVICE="b3" \
    PRODUCT_NAME="b3_tmo_us"

# 16.0: a static stock BUILD_FINGERPRINT only changes ro.build.fingerprint, not
# ro.vendor.build.fingerprint -> Build.isBuildConsistent() fails -> "There's an
# internal problem with your device" at boot. Re-add via a vendor init
# (property_override_dual, like hiae) if certification ever matters.
#    BUILD_FINGERPRINT="lge/b3_tmo_us/b3:7.0/NRD90U/1634416244b13:user/release-keys"
#    PRIVATE_BUILD_DESC="b3_tmo_us-user 7.0 NRD90U 1634416244b13 release-keys"

# Bring-up settings (temporary, see bringup.mk)
$(call inherit-product, device/lge/v521/bringup.mk)
