/*
 * Copyright 2019, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DORNERWORKS_GPL)
 */

#include <camkes.h>

#include <autoconf.h>

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sel4/sel4.h>

#define MAX_STRING_LENGTH 100

/* ready_callback:
 *
 *    Callback function for the Notification ready.
 *
 *    The callback function is called from the Router component when Data has come
 *    on the 'recv_port' declared in the configuration. 'tcp_poll' should return
 *    a proper value, so the buffer should contain the data from the ethernet packet
 *
 *    Functionally, this clears the buffer, then sends a
 *    message back to the sender, so it's an echo server.
 */
void ready_callback(seL4_Word badge)
{
    unsigned int len;
    int status = 0;
    static int num_mesg = 0;

    /* This example assumes a string is sent, therefore, create a local copy of shared buffer as a string */
    char * tst = (void *)echo_tcp_recv_buf;
    assert(tst);

    char ret[MAX_STRING_LENGTH];

    while(0 == status) {
        status = echo_tcp_poll(&len);
        if (status != -1) {
            printf("%s - %s\n", get_instance_name(), tst);
            memset(tst, 0, len);
        }
    }

    snprintf(ret, MAX_STRING_LENGTH, "[ECHO SERVER] - Received Message %d\n", ++num_mesg);
    len = echo_tcp_send((uintptr_t)ret, strlen(ret));
    if (len != -1)
    {
        assert(len == strlen(ret));
    }
}
