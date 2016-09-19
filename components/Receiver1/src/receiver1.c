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

void recv_print(const char *message) {
    printf("Receiver 1 received message: %s\n", message);
}

int run(void)
{
    /* For GDB Manipulation, variables need to have a memory address */
    volatile unsigned int recv_key = 0;
    volatile int count = 50;

    char  receive_buf[50] = "Receiver 1 String";
    printf("R1: %s\n", receive_buf);

    recv_key = 0xdead;
    printf("%08x\n", recv_key);

    recv_key = 0xcafe;
    printf("%08x\n", recv_key);

    recv_key = 0xface;
    printf("%08x\n", recv_key);

    while(count++ < 100)
    {
        printf("R1 Key: %d\n", recv_key++);
    }
    printf("R1: %s\n", receive_buf);
    return 0;
}
