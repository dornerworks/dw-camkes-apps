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

#include <stdio.h>

#include <platsupport/timer.h>

#include <camkes.h>

/*- set timer_val = configuration[me.instance.name].get("%s_period_ns" % me.interface.name) -*/
/*- set timer_id = configuration[me.instance.name].get('%s_id' % me.interface.name) -*/
/*- set irq = configuration[me.parent.to_instance.name].get('%s_irq_irq_number' % me.interface.name) -*/

uint64_t /*? me.interface.name ?*/_get_period_ns(void)
{
   return /*? timer_val ?*/;
}

/*- if os.environ.get('PLAT') == 'imx6' -*/
  /*- if configuration[me.instance.name].get('%s_epit' % me.interface.name) -*/
    /*- include 'seL4EPIT.template.c' -*/
  /*- elif configuration[me.instance.name].get('%s_gpt' % me.interface.name) -*/
    /*- include 'seL4GPT.template.c' -*/
  /*- else -*/
    /*? raise(Exception('Need to select a timer type')) ?*/
  /*- endif -*/
/*- elif os.environ.get('PLAT') == 'zynq7000' -*/
  /*- include 'seL4TTC.template.c' -*/
/*- else -*/
    /*? raise(Exception('Need to select a valid arm platform')) ?*/
/*- endif -*/
