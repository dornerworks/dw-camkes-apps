#
# Copyright 2019, DornerWorks
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DORNERWORKS_BSD)
#

# Hardcode to Sabrelite
set(KernelArch arm CACHE STRING "" FORCE)
set(KernelARMPlatform "sabre" CACHE STRING "" FORCE)
set(KernelArmSel4Arch "aarch32" CACHE STRING "" FORCE)

if ((NOT CROSS_COMPILER_PREFIX) OR ("${CROSS_COMPILER_PREFIX}" STREQUAL ""))
    message(WARNING "Set -DCROSS_COMPILER_PREFIX=arm-linux-gnueabi-")
endif()

# Elfloader settings that correspond to how Data61 sets its boards up.
ApplyData61ElfLoaderSettings(${KernelARMPlatform} ${KernelArmSel4Arch})

ApplyCommonSimulationSettings(${KernelArch})
ApplyCommonReleaseVerificationSettings(FALSE FALSE)
