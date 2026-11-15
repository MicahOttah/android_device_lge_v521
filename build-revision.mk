# Human-readable build revision.
#
# LineageOS derives the build number (ro.build.version.incremental) from a
# SHA1 of the timestamp, the product name and the hostname:
#
#     export BUILD_NUMBER=$( (date +%s%N ; echo $LINEAGE_BUILD; hostname) \
#                             | openssl sha1 | sed -e 's/.*=//g; s/ //g' \
#                             | cut -c1-10 )
#
# (build/make/envsetup.sh:140)
#
# It looks like a git hash, but it is not one: every build gets an
# unrelated number, so two ROMs cannot be told apart and nothing says what
# is in them.  bin/build replaces it with a revision that says what the
# build is and which attempt it is:
#
#     v521-15.1-build14
#
# The revision is the build number, so it is also the name of the OTA
# package (lineage_v521-ota-v521-15.1-build14.zip) and the string that
# Settings -> About phone -> Build number shows.
#
# The values arrive on the make command line:
#
#     V521_BUILD_ATTEMPT   the attempt number          (14)
#     V521_BUILD_REVISION  the revision string         (v521-15.1-build14)
#     V521_BUILD_COMMIT    the device tree commit      (9fa038b)
#
# The properties are only added when the variables are set, because an
# empty property value is not valid in build.prop, and a plain "mka" that
# does not go through bin/build has no revision to report.
#
# PRODUCT_PROPERTY_OVERRIDES is the hook that works from a device tree:
# it is captured per product (build/make/core/product.mk:90) and ends up
# in build.prop through ADDITIONAL_BUILD_PROPERTIES
# (build/make/core/main.mk:124).  ADDITIONAL_BUILD_PROPERTIES itself cannot
# be used from here - build/core/main.mk:112 refuses a tree that sets it.
ifeq ($(V521_BUILD_ATTEMPT),)
    $(info v521: no build revision - build through bin/build to get one)
else
PRODUCT_PROPERTY_OVERRIDES += \
    ro.v521.build.attempt=$(V521_BUILD_ATTEMPT) \
    ro.v521.build.revision=$(V521_BUILD_REVISION) \
    ro.v521.build.commit=$(V521_BUILD_COMMIT)
endif
