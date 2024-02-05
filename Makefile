
PWD=$(shell pwd)
AAP_JUCE_DIR=$(PWD)/external/aap-juce

APP_NAME=SimpleHost

APP_BUILD_DIR=$(PWD)
APP_SRC_DIR=$(PWD)/external/AndroidPluginHost
JUCE_DIR=$(APP_SRC_DIR)/external/JUCE

APP_SHARED_CODE_LIBS="Source/$(APP_NAME)_artefacts/lib$(APP_NAME)_SharedCode.a


#PATCH_FILE=$(PWD)/aap-juce-support.patch
PATCH_DEPTH=1

AAP_JUCE_CMAKE_PATCH_HOSTING=1

# JUCE patches if any
JUCE_PATCHES= \
	$(PWD)/juce-modules.patch \
	$(shell pwd)/external/aap-juce/juce-patches/7.0.6/thread-via-dalvik.patch
JUCE_PATCH_DEPTH=1

include $(AAP_JUCE_DIR)/Makefile.cmake-common
