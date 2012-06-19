ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(BOARD_HAS_LARGE_FILESYSTEM),true)
LOCAL_CFLAGS += -DBOARD_HAS_LARGE_FILESYSTEM
endif

LOCAL_SRC_FILES := \
	mmcutils.c

LOCAL_MODULE := libmmcutils
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libcannibal_e2fsck libcannibal_tune2fs libcannibal_mke2fs libcannibal_ext2fs libcannibal_ext2_blkid libcannibal_ext2_uuid libcannibal_ext2_profile libcannibal_ext2_com_err libcannibal_ext2_e2p

include $(BUILD_STATIC_LIBRARY)

endif	# TARGET_ARCH == arm
endif	# !TARGET_SIMULATOR
