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

# Common VM build definitions
CAmkESAddImportPath(components)
CAmkESAddImportPath(interfaces)
CAmkESAddTemplatesPath(templates)

# Add VM Components and Libraries
add_subdirectory(components/Ethdriver)
add_subdirectory(components/Router)
add_subdirectory(components/FileSystem)

DeclareCAmkESRootServer(http_server.camkes)

GenerateCAmkESRootserver()
