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

TARGETS := $(notdir ${SOURCE_DIR}).cdl
ADL := http_server.camkes

TEMPLATES := templates global-templates

include ${SOURCE_DIR}/components/Ethdriver/Ethdriver.mk
include ${SOURCE_DIR}/components/Router/Router.mk
include ${SOURCE_DIR}/components/FileSystem/FileSystem.mk

include ${PWD}/tools/camkes/camkes.mk
