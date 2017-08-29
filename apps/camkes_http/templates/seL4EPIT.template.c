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

#include <platsupport/mach/epit.h>

/*- if timer_id > 2 or timer_id < 1 -*/
  /*? raise(TemplateError('illegal EPIT number')) ?*/
/*- endif -*/

epit_t */*? me.interface.name ?*/_drv;

void /*? me.interface.name ?*/_handle_interrupt(void)
{
    int err UNUSED;
    err = epit_handle_irq(/*? me.interface.name ?*/_drv);

    err = /*? me.interface.name ?*/_irq_acknowledge();
}

void /*? me.interface.name ?*/__init(void)
{
    int err UNUSED;
    /*? me.interface.name ?*/_drv = (epit_t *)malloc(sizeof(epit_t));

    /* EPIT: Enhanced Periodic Interrupt Timer */
    epit_config_t config = {
        .vaddr = (void*)/*? me.interface.name ?*/_reg,
        .irq = /*? irq ?*/,
        .prescaler = 0
    };

    err = epit_init(/*? me.interface.name ?*/_drv, config);
    err = epit_set_timeout(/*? me.interface.name ?*/_drv, /*? timer_val ?*/, true);

    err = /*? me.interface.name ?*/_irq_acknowledge();
}
