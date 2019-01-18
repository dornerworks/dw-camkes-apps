/*
 * Copyright 2014, NICTA
 * Copyright 2017, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <sel4/sel4.h>
#include <camkes/dataport.h>
#include <sync/sem-bare.h>
#include <string.h>
#include <camkes.h>

#include <defines.h>

#include <lwip/tcp_impl.h>

static err_t tcpsend(void *arg, struct tcp_pcb *pcb, uint16_t len);
static err_t tcprecv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static err_t tcpaccept(void *arg, struct tcp_pcb *pcb, err_t err);

/*- set ack_ep = alloc('ack_ep', seL4_EndpointObject, write=True, grant=True) -*/
/*- set ep = alloc('ep', seL4_EndpointObject, read=True, write=True) -*/

/* Assume a function exists to get a dataport */
void */*? me.interface.name?*/_send_buf_buf(unsigned int id);
void */*? me.interface.name?*/_recv_buf_buf(unsigned int id);

/*- set bufs = configuration[me.instance.name].get('num_client_recv_bufs') -*/
/*- set clients = [] -*/
/*- for c in me.parent.from_ends -*/
    /*- set port = configuration[c.instance.name].get('%s_port' % c.interface.name) -*/
    /*- set client = configuration[c.instance.name].get('%s_attributes' % c.interface.name) -*/
    /*- set client = client.strip('"') -*/
    /*- set is_reader = False -*/
    /*- set instance = c.instance.name -*/
    /*- set interface = c.interface.name -*/
    /*- include 'global-endpoint.template.c' -*/
    /*- set aep = pop('notification') -*/
    /*- do clients.append( (client, port, aep) ) -*/
/*- endfor -*/

/*- set cnode = alloc_cap('cnode', my_cnode, write=True) -*/
/*- set reply_cap_slot = alloc_cap('reply_cap_slot', None) -*/

typedef struct tcp_message {
    void *msg;
    unsigned int len;
    struct tcp_message *next;
}tcp_message_t;

typedef struct tcp_client {
    struct tcp_pcb *tpcb;
    int client_id;
    uint16_t port;
    seL4_CPtr aep;
    tcp_message_t *free_head;
    tcp_message_t *used_head;
    tcp_message_t *used_tail;
    tcp_message_t message_memory[ /*? bufs ?*/];
} tcp_client_t;

static tcp_client_t tcp_clients[/*? len(clients) ?*/] = {
/*- for client,port,aep in clients -*/
    {.tpcb = NULL, .client_id = /*? client ?*/, .port = /*? port ?*/, .aep = /*? aep ?*/, .used_head = NULL},
/*- endfor -*/
};

/* TCP Callback Function for when a Incoming Connection has been accepted */
static err_t tcpaccept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    tcp_client_t *client = (tcp_client_t*)arg;

    client->tpcb = pcb;

    /* Callback Functions for Send/Recv Events */
    tcp_sent(client->tpcb, tcpsend);
    tcp_recv(client->tpcb, tcprecv);

    return ERR_OK;
}

/* Callback function when our sent data has been acknowledged by receiver
 *
 * When you respond with a message after every packet,
 * things can get messy. Really long packets (files, etc) being sent to the application
 * get split into packets the size of TCP Maximum Segment Size. Getting that packet,
 * responding, and waiting for an ACK takes a long time, introducing delay into the system.
 * If the remote host finishes sending its packets, they get stored in the RX Buffers. When
 * the connection is closed, this won't get called again, and the application can get stuck
 * waiting on the ack_ep.
 *
 * Use with caution. You can remove the -from template's Wait call to
 * get rid of the send ACKS, or wait til the application has reconstructed the entire file
 * to send information back.
 */
static err_t tcpsend(void *arg, struct tcp_pcb *pcb, uint16_t len)
{
    seL4_SetMR(0, len);
    seL4_NBSend(/*? ack_ep ?*/, seL4_MessageInfo_new(0, 0, 0, 1));
    return ERR_OK;
}

/* Callback function when new data arrives. */
static err_t tcprecv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    tcp_client_t *client = (tcp_client_t*)arg;

    /* Host has closed the connection */
    if (NULL == p)
    {
        tcp_arg(client->tpcb, NULL);
        tcp_sent(client->tpcb, NULL);
        tcp_recv(client->tpcb, NULL);
        tcp_close(client->tpcb);

        /* JIC the Component is waiting on the ACK_EP... */
        seL4_SetMR(0, -1);
        seL4_NBSend(/*? ack_ep ?*/, seL4_MessageInfo_new(0, 0, 0, 1));
    }
    else if (err == ERR_OK && p != NULL && client->free_head != NULL)
    {
        /* Local copy of TCP Message Structure in Client Structure */
        tcp_message_t *m = client->free_head;
        client->free_head = client->free_head->next;

        int pbuf_length = p->tot_len;
        if(pbuf_length > sizeof(Buf))
        {
            pbuf_length = sizeof(Buf);
        }
        m->msg = malloc(pbuf_length);
        assert(m->msg);

        unsigned int len = 0;
        for (struct pbuf *q = p; q != NULL; q = q->next)
        {
            /* if we're going to overflow the buffer, don't do it! */
            if(len + q->len  > pbuf_length)
            {
                break;
            }
            memcpy(m->msg + len, q->payload, q->len);
            len += q->len;
        }

        m->len = len;
        m->next = NULL;

        if (!client->used_head)
        {
            client->used_head = client->used_tail = m;
        }
        else
        {
            client->used_tail->next = m;
            client->used_tail = m;
        }

        if (client->used_head) {
            seL4_Signal(client->aep);
        }

        pbuf_free(p);
    }
    else
    {
        pbuf_free(p);
        return ERR_BUF;
    }

    return ERR_OK;
}

/*
 * Purpose: Run the TCP Receive System
 *
 * Inputs: void
 *
 * Returns: void
 *
 */
void /*? me.interface.name ?*/__run(void)
{
    uint8_t tcp_call_type;
    uint16_t tcp_msg_len, ipc_ret_len;
    seL4_Word badge;

    while (1)
    {
        /* Wait for a client to call us */
        seL4_Wait(/*? ep ?*/, &badge);

        /* Use the badge to create a local copy of the proper clients TCP Structure */
        tcp_client_t *client = NULL;
        for (int i = 0; i < /*? len(clients) ?*/ && !client; i++) {
            if (tcp_clients[i].client_id == badge) {
                client = &tcp_clients[i];
            }
        }
        assert(client);

        /* Read the Message Registers BEFORE saving the Caller
         *  0: Send or Recv
         *  1: If Send, Length of Packet to Send
         */
        tcp_call_type = seL4_GetMR(0);
        assert(tcp_call_type == TCP_POLL || tcp_call_type == TCP_SEND);

        tcp_msg_len = ((tcp_call_type == TCP_SEND) ? seL4_GetMR(1) : 0);

        /* Because the from template uses seL4_Call, we have to save the one-time reply capability */
        seL4_CNode_SaveCaller(/*? cnode ?*/, /*? reply_cap_slot ?*/, 32);

        lwip_lock();

        ipc_ret_len = 0;

        if(tcp_call_type == TCP_SEND)
        {
            ipc_ret_len = 1;

            if(client->tpcb->state == ESTABLISHED)
            {
                void * buffer = /*? me.interface.name?*/_send_buf_buf(badge);
                int available_len = tcp_sndbuf(client->tpcb);

                /* Make sure we aren't sending more data than the PCB has available */
                if(available_len < tcp_msg_len)
                {
                    tcp_msg_len = available_len;
                }
                tcp_write(client->tpcb, buffer, tcp_msg_len, 0);
                tcp_output(client->tpcb);

                seL4_SetMR(0, 1);
            }
            else
            {
                printf("\nTCP Connection not ESTABLISHED\n");
                seL4_SetMR(0, -1);
            }
        }
        else
        {
            if (!client->used_head)
            {
                seL4_SetMR(0, -1);
                ipc_ret_len = 1;
            }
            else
            {
                void *p = /*? me.interface.name ?*/_recv_buf_buf(badge);
                tcp_message_t *m = client->used_head;

                tcp_recved(client->tpcb, m->len);

                client->used_head = client->used_head->next;

                memcpy(p, m->msg, m->len);
                free(m->msg);

                seL4_SetMR(0, client->used_head ? 0 : 1);
                seL4_SetMR(1, m->len);
                ipc_ret_len = 2;

                m->next = client->free_head;
                client->free_head = m;
            }
        }
        seL4_Send(/*? reply_cap_slot ?*/, seL4_MessageInfo_new(0, 0, 0, ipc_ret_len));
        lwip_unlock();
    }
}

/*
 * Purpose: Initialize each client.
 *
 * Inputs: void
 *
 * Returns: void
 *
 */
void /*? me.interface.name ?*/__init(void)
{
    int err UNUSED, i, j;

    err = lwip_lock();
    for (i = 0; i < /*? len(clients) ?*/; i++)
    {
        for (j = 0; j < /*? bufs ?*/; j++) {
            if (0 == j) {
                tcp_clients[i].message_memory[j] =
                    (tcp_message_t){.next = NULL};
            } else {
                tcp_clients[i].message_memory[j] =
                    (tcp_message_t){.next = &tcp_clients[i].message_memory[j - 1]};
            }
        }
        /* Initialize the rest of the structure (PCB - Protocol Control Block) */
        tcp_clients[i].free_head = &tcp_clients[i].message_memory[/*? bufs ?*/ - 1];

        tcp_clients[i].tpcb = tcp_new();
        assert(tcp_clients[i].tpcb);

        /* Bind PCB to Port (Not IP Address!) */
        err = tcp_bind(tcp_clients[i].tpcb, NULL, tcp_clients[i].port);
        assert(!err);

        /* Set PCB to accept incoming connections */
        tcp_clients[i].tpcb = tcp_listen(tcp_clients[i].tpcb);

        /* Set the Callback function's "arg" parameters */
        tcp_arg(tcp_clients[i].tpcb, &tcp_clients[i]);

        /* Set callback function for when Listening PCB has been connected to a host */
        tcp_accept(tcp_clients[i].tpcb, tcpaccept);
    }

    err = lwip_unlock();
}
