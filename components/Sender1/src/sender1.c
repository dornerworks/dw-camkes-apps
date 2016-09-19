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

#include <camkes.h>
#include <stdio.h>

/* Static variable to store whether GDB is active */
static int is_gdb_enabled;

/* Global Variable for GDB Manipulation */
static volatile unsigned int send_key1 = 0;

/*
 * gdb_enabled exists in the GDB source code; however, it is a __weak__
 * function that always returns 'true'. It is up to the user to overwrite
 * that function with their own (which needs to exist in the same cnode)
 *
 * This function provides that framework, allowing the user to set whether
 * GDB is enabled however they want.
 */
int gdb_enabled(void)
{
    return is_gdb_enabled;
}

/*
 * Pre Init Function that enables GDB for this component.
 *
 * This function can be modified to check that status of a switch, a buffer,
 * or something else instead of just setting the variable
 */
void pre_init(void)
{
    is_gdb_enabled = 1;
}

int run(void)
{
    /* For GDB Manipulation, variables need to have a memory address */
    volatile unsigned int send_key = 0;
    volatile int count = 90;

    char sender1_buf[50] = "Hello World!";
    char *s = "CAmkES GDB Test - Done Initializing\n";
    send_print(s);

    printf("buf = %s\n", sender1_buf);

    send_key = 0xCAFEBABE;
    printf("The 1st key is: %08x\n", send_key);

    send_key = 0xDEADBEEF;
    printf("The 2nd key is: %08x\n", send_key);

    send_key = 0x12345678;
    printf("The 3rd key is: %08x\n", send_key);

    printf("buf = %s\n", sender1_buf);

    while(1)
    {
        send_key++;
        printf("The nth key is: %x\n", send_key);
        if (count > 100) {
            while(1);
        }
    }

    return 0;
}
