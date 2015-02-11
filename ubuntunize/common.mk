
#message colors
COLOR_BLACK=30
COLOR_RED=31
COLOR_GREEN=32
COLOR_YELLOW=33
COLOR_BLUE=34
COLOR_MAGENTA=35
COLOR_CYAN=36
COLOR_WHITE=97
COLOR_DEFAULT=39
INVERSE=1
NONINVERSE=0

#usage: $(call message,<text>,<tabs>,<color>,<inverse>)
define message
	( if [ -z $(NOCOLOR) ] ; then echo -ne "\e[$(3)m" ; fi ; \
	if [ "$(4)" = "$(INVERSE)" ] ; then if [ -z $(NOCOLOR) ] ; then echo -ne "\e[7m" ; fi ; echo -n ">>>" ; fi ; \
	if [ "$(2)" -gt 0 ] && [ "$(2)" -lt 10 ] ; then for a in `seq "$(2)"` ; do echo -n "   " ; done ; fi ; \
 	echo -n " $(1) " ; \
	if [ "$(4)" = "$(INVERSE)" ] ; then echo -n "<<<" ; if [ -z $(NOCOLOR) ] ; then echo -ne "\e[27m" ; fi ; fi ; \
	echo ; \
	if [ -z $(NOCOLOR) ] ; then echo -ne "\e[39m" ; fi ; \
	)
endef

#usage: $(call patch_source,<patch_dir>,<source_dir>)
define patch_source
	(for PATCH_FILE in `find $(1) -maxdepth 1 -name '*.patch' | sort 2>/dev/null` ; do \
	    patch -p1 -d $(2) < $${PATCH_FILE} ; \
	  done \
	)
endef

#usage: $(call upd-alternatives,<version>, <priority>, <cross-compile-prefix>)
define upd_alternatives
  (sudo update-alternatives --force --install   /usr/bin/$(3)gcc  $(3)gcc  /usr/bin/$(3)gcc-$(1)   $(2) \
  --slave /usr/bin/$(3)cpp         $(3)cpp         /usr/bin/$(3)cpp-$(1)          \
  --slave /usr/bin/$(3)g++         $(3)g++         /usr/bin/$(3)g++-$(1)          \
  --slave /usr/bin/$(3)gcc-ar      $(3)gcc-ar      /usr/bin/$(3)gcc-ar-$(1)       \
  --slave /usr/bin/$(3)gcc-nm      $(3)gcc-nm      /usr/bin/$(3)gcc-nm-$(1)       \
  --slave /usr/bin/$(3)gcc-ranlib  $(3)gcc-ranlib  /usr/bin/$(3)gcc-ranlib-$(1)   \
  --slave /usr/bin/$(3)gcov        $(3)gcov        /usr/bin/$(3)gcov-$(1)         \
  )
endef

#############

SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	else if [ -x /bin/bash ]; then echo /bin/bash; \
	else echo sh; fi; fi)

TOPDIR := $(shell pwd)

JOBS := $(shell echo $$((`getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1`)))

.DEFAULT_GOAL := all

BUILD_DIR ?= $(TOPDIR)/..
IMAGE_DIR  = $(TOPDIR)/linux-staging
BOOT_DIR   = $(IMAGE_DIR)/boot
DISTRO     = $(shell lsb_release -sc)

export ARCH = arm
export CROSS_COMPILE ?= arm-linux-gnueabihf-
export INSTALL_MOD_PATH ?= $(IMAGE_DIR)

