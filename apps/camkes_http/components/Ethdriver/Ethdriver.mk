#
# Copyright 2017, DornerWorks
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "LICENSE_GPLv2.txt" for details.
#
# @TAG(DORNERWORKS_GPL)
#
# This data was produced by DornerWorks, Ltd. of Grand Rapids, MI, USA under
# a DARPA SBIR, Contract Number D16PC00107.
#
# Approved for Public Release, Distribution Unlimited.
#

CURRENT_DIR := $(dir $(abspath $(lastword ${MAKEFILE_LIST})))

Ethdriver_CFILES := $(wildcard ${CURRENT_DIR}/src/*.c)
Ethdriver_CFILES += $(wildcard ${CURRENT_DIR}/src/plat/$(PLAT)/*.c)
Ethdriver_HFILES := $(wildcard $(CURRENT_DIR)/include/*.h)

Ethdriver_LIBS := ethdrivers sel4platsupport sel4vka sel4simple sel4allocman sel4utils utils
