/*
 * Copyright 2017, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DORNERWORKS_GPL)
 *
 * This data was produced by DornerWorks, Ltd. of Grand Rapids, MI, USA under
 * a DARPA SBIR, Contract Number D16PC00107.
 *
 * Approved for Public Release, Distribution Unlimited.
 *
 */

/* Device specific configurations that is passed into components */

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <camkes.h>

/* Buf size for eth driver. Should match HWEthDriver.mmio_attributes */

#ifdef CONFIG_PLAT_ZYNQ7000
 #define ETHDRIVER_MMIO_BUF_SZ 0x4000
#else
 #define ETHDRIVER_MMIO_BUF_SZ 0x1000
#endif

#endif
