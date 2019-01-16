/*
 * Copyright 2017, DORNERWORKS
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DORNERWORKS_GPL)
 */

#include <camkes.h>

#include <ethdrivers/zynq7000.h>
#include <ethdrivers/raw.h>

void plat_configure_ethdriver(struct arm_eth_plat_config *plat_config) {

    if(!ethif_init) {
        ethif_init = &ethif_zynq7000_init;
    }

    plat_config->buffer_addr = (void *)EthDriver;
}