/*
 * Copyright 2019, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DORNERWORKS_GPL)
 */

#pragma once

#define HARDWARE_ETHERNET_INTERFACES                                                  \
    consumes IRQ irq;

#define HARDWARE_ETHERNET_COMPOSITION                                                 \
    component HWEthDriver hwethdriver;                                                \
    connection seL4HardwareInterrupt hwethirq(from hwethdriver.irq, to irq); \

#define HARDWARE_ETHERNET_CONFIG                                                      \
    hwethdriver.irq_irq_number = 150;                                                 \

#define HARDWARE_ETHDRIVER_MMIOS                                                      \
    ethdriver.mmios =  ["0x02188000:0x4000:12",  /* ENET_PADDR */                     \
                        "0x021BC000:0x4000:12",  /* OCOTP_CTRL_PADDR */               \
                        "0x020E0000:0x4000:12",  /* IOMUXC_PADDR */                   \
                        "0x020A4000:0x4000:12",  /* GPIO3_PADDR */                    \
                        "0x020B0000:0x4000:12",  /* GPIO6_PADDR */                    \
                        "0x020C4000:0x4000:12",  /* CCM_PADDR */                      \
                        "0x020C8000:0x1000:12"]; /* ANALOG_PADDR */                   \
