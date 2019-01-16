#
# Copyright 2019, DornerWorks
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DORNERWORKS_BSD)
#

cmake_minimum_required(VERSION 3.7.2)

project(Router C)

DeclareCAmkESComponent(Router
  SOURCES src/router.c src/status_updates.c
  INCLUDES include
  LIBS sel4camkes ethdrivers lwip sel4vspace
  )

CAmkESAddCPPInclude("${CMAKE_CURRENT_LIST_DIR}/include/")
