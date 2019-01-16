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

project(Ethdriver C)

FILE(GLOB PlatSources src/plat/${KernelPlatform}/*.c)

DeclareCAmkESComponent(Ethdriver
  SOURCES src/ethdriver.c ${PlatSources}
  INCLUDES include
  LIBS sel4utils sel4vka sel4allocman sel4vspace sel4simple sel4platsupport ethdrivers
  )

CAmkESAddCPPInclude("${CMAKE_CURRENT_LIST_DIR}/include/plat/${KernelPlatform}/")
