#
# Copyright 2017, DornerWorks
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "LICENSE_GPLv2.txt" for details.
#
# @TAG(DORNERWORKS_GPL)
#

CURRENT_DIR := $(dir $(abspath $(lastword ${MAKEFILE_LIST})))

FileSystem_CFILES := $(wildcard ${CURRENT_DIR}/src/*.c)
FileSystem_HFILES := $(wildcard ${CURRENT_DIR}/include/*.h)
FileSystem_HFILES += $(wildcard ${CURRENT_DIR}/web/includes/*.h)
