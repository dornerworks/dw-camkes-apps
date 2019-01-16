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

#define HARDWARE_ETHERNET_PROVIDES_INTERFACES                                         \
    emits IRQ irq;                                                                    \
    dataport Buf EthDriver;                                                           \

#define HARDWARE_ETHERNET_INTERFACES                                                  \
    consumes IRQ irq;                                                                 \
    dataport Buf EthDriver;                                                           \

#define HARDWARE_ETHERNET_COMPOSITION                                                 \
    component HWEthDriver hwethdriver;                                                \
    connection seL4HardwareInterrupt hwethirq(from hwethdriver.irq, to irq);          \
    connection seL4HardwareMMIO eth_reg(from EthDriver, to hwethdriver.EthDriver);    \

#define HARDWARE_ETHERNET_CONFIG                                                      \
    hwethdriver.irq_irq_number = 54;                                                  \
    hwethdriver.EthDriver_paddr = 0xE000B000;                                         \
    hwethdriver.EthDriver_size = 0x1000;                                              \

#define HARDWARE_ETHDRIVER_MMIOS                                                      \
    ethdriver.mmios =  ["0xE000B000:0x1000:12",                                       \
                        "0xF8000000:0x1000:12"];                                      \
