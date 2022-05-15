.PHONY: all installhdrs install $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common Makefile

PROJECT := Libnotify
TEST_DIR := tests/

DEVELOPER_DIR ?= /Applications/Xcode.app/Contents/Developer/

ifneq ($(wildcard $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common),)

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

EXCLUDED_SOURCES := \
	random_test.c \
	notify_control.c \
	notify_test_helper.c \

OTHER_LDFLAGS := -ldarwintest

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets


# fall back if we can't find the Makefile to avoid an error in that case
else
all:
installhdrs:
install:
	mkdir -p $(DSTROOT)/AppleInternal/CoreOS/tests/$(PROJECT)
endif
