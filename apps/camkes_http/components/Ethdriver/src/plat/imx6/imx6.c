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

#include <ethdrivers/imx6.h>
#include <ethdrivers/raw.h>

void plat_configure_ethdriver(struct eth_plat_config *plat_config) {

    plat_config->init = ethif_imx6_init;
}
