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

Router_HFILES := $(wildcard ${CURRENT_DIR}/include/*.h)
Router_CFILES := $(wildcard ${CURRENT_DIR}/src/*.c)
Router_LIBS := sel4camkes ethdrivers lwip sel4vspace