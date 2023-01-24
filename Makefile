
PWD=$(shell pwd)
AAP_JUCE_DIR=$(PWD)/external/aap-juce

APP_NAME=Dexed

APP_BUILD_DIR=$(PWD)
APP_SRC_DIR=$(PWD)/external/AndroidPluginHost
JUCE_DIR=$(APP_SRC_DIR)/external/JUCE

APP_SHARED_CODE_LIBS="Source/$(APP_NAME)_artefacts/lib$(APP_NAME)_SharedCode.a


#PATCH_FILE=$(PWD)/aap-juce-support.patch
PATCH_DEPTH=1

AAP_JUCE_CMAKE_PATCH_HOSTING=1

# JUCE patches if any
JUCE_PATCHES= \
	juce-modules.patch \
	$(shell pwd)/external/aap-juce/JUCE-support-Android-thread-via-dalvik-juce7.patch \
	$(shell pwd)/external/aap-juce/JUCE-support-Android-disable-detach-current-thread.patch
JUCE_PATCH_DEPTH=1

include $(AAP_JUCE_DIR)/Makefile.cmake-common
