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

project(FileSystem C)

DeclareCAmkESComponent(FileSystem
  SOURCES src/fs.c src/fsdata.c
  INCLUDES include web/includes
  LIBS lwip
  )
