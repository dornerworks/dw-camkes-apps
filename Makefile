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

TARGETS := gdb_test.cdl
ADL := gdb_test.camkes

include ${SOURCE_DIR}/components/Sender1/Sender1.mk
include ${SOURCE_DIR}/components/Receiver1/Receiver1.mk

include ${SOURCE_DIR}/../../tools/camkes/camkes.mk
