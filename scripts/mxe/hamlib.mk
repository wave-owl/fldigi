# This file is part of MXE. See LICENSE.md for licensing information.
# visit https://hamlib.github.io/ or https://github.com/N0NB/hamlib
# 2016-12-24 Lars Holger Engelhard DL5RCW

PKG             := hamlib
$(PKG)_WEBSITE  := https://github.com/
$(PKG)_DESCR    := HamLib
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 4.7.2
$(PKG)_CHECKSUM := ae1fcf2dbc80ea0786ea8f047b09399c3f7737d1930442f61a031708ed33e88f
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.gz
$(PKG)_URL      := https://github.com/Hamlib/Hamlib/releases/download/$($(PKG)_VERSION)/$($(PKG)_FILE)
$(PKG)_DEPS     := cc libusb1 libxml2 pthreads

define $(PKG)_BUILD
    cd '$(BUILD_DIR)' && '$(SOURCE_DIR)/configure' \
        $(MXE_CONFIGURE_OPTS) \
        LIBS='-lusb-1.0' \
        --disable-winradio \
        --disable-parallel
    $(MAKE) -C '$(BUILD_DIR)' -j '$(JOBS)' || $(MAKE) -C '$(BUILD_DIR)' -j 10
    $(MAKE) -C '$(BUILD_DIR)' -j 10 install $(MXE_DISABLE_CRUFT)

    '$(TARGET)-gcc' \
        -W -Wall -Werror -ansi -pedantic \
        '$(PWD)/src/$(PKG)-test.c' -o '$(PREFIX)/$(TARGET)/bin/test-$(PKG).exe' \
        `'$(TARGET)-pkg-config' --cflags --libs hamlib`
endef
