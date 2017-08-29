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

#include <platsupport/mach/gpt.h>

gpt_t */*? me.interface.name ?*/_drv;

void /*? me.interface.name ?*/_handle_interrupt(void)
{
    int err UNUSED;
    err = gpt_handle_irq(/*? me.interface.name ?*/_drv);

    err = /*? me.interface.name ?*/_irq_acknowledge();
}

void /*? me.interface.name ?*/__init(void)
{
    int err UNUSED;
    /*? me.interface.name ?*/_drv = (gpt_t *)malloc(sizeof(gpt_t));

    /* GPT: General Purpose Timer */
    gpt_config_t config = {
        .vaddr = (void*)/*? me.interface.name ?*/_reg,
        .prescaler = 0
    };

    err = gpt_init(/*? me.interface.name ?*/_drv, config);
    err = gpt_set_timeout(/*? me.interface.name ?*/_drv, /*? timer_val ?*/, true);

    err = /*? me.interface.name ?*/_irq_acknowledge();
}
