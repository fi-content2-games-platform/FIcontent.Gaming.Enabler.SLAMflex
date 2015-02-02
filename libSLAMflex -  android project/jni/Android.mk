LOCAL_PATH:= $(call my-dir)
export MAINDIR:= $(LOCAL_PATH)

#LAPACK, BLAS, F2C compilation
include $(CLEAR_VARS)
include $(MAINDIR)/clapack/Android.mk
LOCAL_PATH := $(MAINDIR)
include $(CLEAR_VARS)
LOCAL_MODULE:= lapack
LOCAL_STATIC_LIBRARIES := tmglib clapack1 clapack2 clapack3 blas f2c
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_EXPORT_LDLIBS := $(LOCAL_LDLIBS)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := slamflex
LOCAL_SRC_FILES += SLAMflex.cpp \
				   SendMessage.cpp  \
ATANCamera.cpp \
Bundle.cpp	\
convolution.cpp	\
convolve_gaussian.cpp	\
cvd_timer.cpp	\
fast_10_detect.cpp	\
fast_10_score.cpp	\
fast_9_detect.cpp	\
fast_9_score.cpp	\
fast_corner.cpp	\
fast_corner_9_nonmax.cpp	\
HomographyInit.cpp	\
KeyFrame.cpp \
Map.cpp	\
MapMaker.cpp	\
MapPoint.cpp	\
MiniPatch.cpp	\
nonmax_suppression.cpp \
PatchFinder.cpp	\
posix_memalign.cpp	\
Relocaliser.cpp	\
ShiTomasi.cpp	\
slower_corner_10.cpp	\
slower_corner_9.cpp	\
SmallBlurryImage.cpp	\
synchronized.cpp	\
System.cpp	\
thread.cpp	\
Tracker.cpp	\
utility_byte_differences.cpp \
utility_double_int.cpp	\
utility_float.cpp	\

LOCAL_STATIC_LIBRARIES := lapack
LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)
