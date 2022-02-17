CXXFLAGS = -g -Wall -O -std=c++11 -I. -fPIC -fexceptions -pthread  #-Wextra
CFLAGS = -g -Wall -O -std=c99 -I. -fPIC -pthread  #-Wextra

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:= android_native_app_glue
LOCAL_SRC_FILES:= android_native_app_glue.c
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)
LOCAL_EXPORT_LDLIBS := -llog -landroid
# The linker will strip this as "unused" since this is a static library, but we
# need to keep it around since it's the interface for JNI.
LOCAL_EXPORT_LDFLAGS := -u ANativeActivity_onCreate
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := tsproxyd
LOCAL_SRC_FILES := tsproxy.cpp adapter.c
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/include
LOCAL_CPPFLAGS += $(CXXFLAGS)
LOCAL_CFLAGS += $(CFLAGS)
LOCAL_LDLIBS := -llog -landroid -L$(LOCAL_PATH)/../jniLibs/$(APP_ABI)  -lhv
LOCAL_STATIC_LIBRARIES := android_native_app_glue
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := tsproxydservice
LOCAL_SRC_FILES := com_ts_tsproxyd_TsproxydService.c
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/include
LOCAL_CPPFLAGS += $(CXXFLAGS)
LOCAL_CFLAGS += $(CFLAGS)
LOCAL_LDLIBS := -llog -landroid
LOCAL_SHARED_LIBRARIES := tsproxyd
include $(BUILD_SHARED_LIBRARY)