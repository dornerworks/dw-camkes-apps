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

# Find the Global Stuff First
find_file(CAMKES_CMAKE camkes.cmake PATHS ${CMAKE_SOURCE_DIR}/projects/camkes-tool/ CMAKE_FIND_ROOT_PATH_BOTH)
mark_as_advanced(FORCE CAMKES_CMAKE)
if("${CAMKES_CMAKE}" STREQUAL "CAMKES_CMAKE-NOTFOUND")
    message(FATAL_ERROR "Failed to find camkes.cmake. Consider cmake -DCAMKES_CMAKE=/path/to/camkes.cmake")
endif()
include(${CAMKES_CMAKE})

find_file(GLOBAL_COMPONENTS_PATH global-components.cmake PATHS ${CMAKE_SOURCE_DIR}/projects/global-components/ CMAKE_FIND_ROOT_PATH_BOTH)
mark_as_advanced(FORCE GLOBAL_COMPONENTS_PATH)
if("${GLOBAL_COMPONENTS_PATH}" STREQUAL "GLOBAL_COMPONENTS_PATH-NOTFOUND")
    message(FATAL_ERROR "Failed to find global-components.cmake. Consider cmake -DGLOBAL_COMPONENTS_PATH=/path/to/global-components.cmake")
endif()
include(${GLOBAL_COMPONENTS_PATH})

# Include our local helper files
include("${CMAKE_CURRENT_LIST_DIR}/app_settings.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/lwip_settings.cmake")

# Finally include the application subdirectory
add_subdirectory(apps/camkes_http)
