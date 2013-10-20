LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := cot
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/res

COT_RES_LOC := $(commands_recovery_local_path)/gui/devices
COT_RES_GEN := $(intermediates)/cot

$(COT_RES_GEN):
	mkdir -p $(TARGET_RECOVERY_ROOT_OUT)/res/theme
	cp -fr $(COT_RES_LOC)/$(DEVICE_RESOLUTION)/res/images/* $(TARGET_RECOVERY_ROOT_OUT)/res/theme/

LOCAL_GENERATED_SOURCES := $(COT_RES_GEN)
LOCAL_SRC_FILES := cot $(COT_RES_GEN)
include $(BUILD_PREBUILT)
