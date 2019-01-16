#
# Copyright 2019, DornerWorks
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DORNERWORKS_BSD)
#

include("../util_libs/liblwip/lwip_helpers.cmake")

# turn on lwip
set(LibLwip ON CACHE BOOL "" FORCE)

# Declare lwipopts.h include
AddLWIPConfiguration(${CMAKE_CURRENT_SOURCE_DIR}/lwip_include)
