LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src)

# The app registers a KeyHandler, which is a hidden API of the framework.
LOCAL_PRIVATE_PLATFORM_APIS := true

LOCAL_PACKAGE_NAME := QuickButton
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true

# KeyHandler is loaded by name from system_server (config_deviceKeyHandlerClasses)
LOCAL_PROGUARD_ENABLED := disabled

include $(BUILD_PACKAGE)
