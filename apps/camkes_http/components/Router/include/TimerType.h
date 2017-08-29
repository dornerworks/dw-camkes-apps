/*
 * Copyright 2014, NICTA
 * Copyright 2017, DORNERWORKS
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */
#ifndef CONFIG_TIMERTYPE_H
#define CONFIG_TIMERTYPE_H

#include "device_config.h"

typedef struct TimerMMIO {
    char buf[TIMER_MMIO_BUF_SZ];
} TimerMMIO_t;

#endif
