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

#ifndef CONFIG_ETHTYPE_H
#define CONFIG_ETHTYPE_H

#include "device_config.h"

typedef struct EthDriverMMIO {
    char buf[ETHDRIVER_MMIO_BUF_SZ];
} EthDriverMMIO_t;

#endif