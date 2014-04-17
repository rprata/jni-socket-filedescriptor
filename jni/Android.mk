LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cxx .cpp .cc
LOCAL_CPP_FEATURES := exceptions

FILE_LIST += socket.cpp

LOCAL_MODULE := socket
LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)

# for logging
LOCAL_LDLIBS    += -llog

include $(BUILD_SHARED_LIBRARY)