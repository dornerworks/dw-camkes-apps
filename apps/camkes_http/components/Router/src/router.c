/*
 * Copyright 2014, NICTA
 * Copyright 2017, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

/* -------------------------------------------------------------------------------------------------
 * router.c
 *
 * This is the source code for the router component. This component interfaces with the lwip drivers
 * to decode ethernet packets and forward them to the proper component.
 *
 * In the pre_init function the component uses the configuration values to setup a network interface
 *
 * When signaled from the ethdriver component, the router knows there is an RX packet available. It
 * uses the ethdriver interface functions to read that data into a buffer which is decoded and
 * passed into the proper component.
 *
 * For a TX Event, the router component takes a fully formed packet and forwards it to the ethdriver
 * component through the shared ethdriver buffer. The ethdriver component then places it into the
 * proper physical address and starts the transfer
 *
 *------------------------------------------------------------------------------------------------*/

#include <autoconf.h>

#include <string.h>
#include <camkes.h>

#undef ERR_IF

#include <ethdrivers/lwip.h>
#include <netif/etharp.h>
#include <lwip/init.h>
#include <lwip/igmp.h>
#include <sel4/sel4.h>

#include <camkes/dataport.h>

#include <defines.h>

#include <lwip/tcp_impl.h>

#define EVENT_TIMER 0
#define TCP_TIMER   1

/* Timer counter to handle calling slow-timer from tcp_tmr() */
static uint8_t tcp_timer;

/* Called periodically to dispatch TCP timers. */
void tcp_timer_handle(void)
{
    /* Call tcp_fasttmr() every 250 ms */
    tcp_fasttmr();

    tcp_timer = (tcp_timer + 1) & 1;
    if (tcp_timer)
    {
        /* Call tcp_slowtmr() every 500 ms */
        tcp_slowtmr();
    }
}

/* low_level_init:
 *
 *    Sets the Max Tranmission Unit.
 *    Gets the MAC Address from the configuration using the Ethdriver Interface
 */
static void low_level_init(struct eth_driver *driver, uint8_t *mac, int *mtu)
{
    /* Check to ensure pointers are not NULL */
    if(!mac || !mtu)
    {
        printf("Passed in MAC or MTU are NULL\n");
        return;
    }

    *mtu = 1500;

    /* Interface function returns 1 on success. Failure relates to NULL Pointers */
    while(!ethdriver_mac(&mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]));
}

/* Buffer in the Ethdriver templates. Shared with the Ethdrvier component */
extern void *ethdriver_buf;

/* raw_poll:
 *
 *   Passed into the lwip library as the raw_poll function. Also called as the result of an
 *   interrupt, which occurs when the ethdriver component calls the client_emit function.
 *
 *   Calls the RX function, allocates a rx buffer in the lwip library, copies the packet from the
 *   shared ethernet dataport. During the rx_complete, the tcprecv function in the
 *   seL4TCPRecv-to template gets called, which sets up the proper components client structure so
 *   the `poll` call will return data available.
 */
static void raw_poll(struct eth_driver *driver)
{
    int len;
    int status = ethdriver_rx(&len);

    while (status != RX_FAILURE)
    {
        void *buf;
        void *cookie;

        buf = (void*)driver->i_cb.allocate_rx_buf(driver->cb_cookie, len, &cookie);

        /* If we get a buffer back, get the packet */
        if(buf)
        {
            memcpy(buf, (void*)ethdriver_buf, len);
            driver->i_cb.rx_complete(driver->cb_cookie, 1, &cookie, (unsigned int*)&len);

            /* Get another packet or break the loop b/c we've gotten every packet */
            if (RX_INCOMPLETE == status)
            {
                status = ethdriver_rx(&len);
            }
            else
            {
                /* if status is 0 we already saw the last packet */
                assert(status == RX_COMPLETE);
                status = RX_FAILURE;
            }
        }
        else
        {
            status = RX_FAILURE;
        }
    }
}

/* raw_tx:
 *
 *   Passed into the lwip library as the raw_tx function.
 *
 *   Copies the inputted packets to the shared dataport with the ethdriver. Call the tx function
 *   that interfaces with the HW.
 */
static int raw_tx(struct eth_driver *driver, unsigned int num, uintptr_t *phys, unsigned int *len,
                  void *cookie)
{
    if(!phys || !len || !cookie)
    {
        printf("Passed in NULL Pointers\n");
        return ETHIF_TX_FAILED;
    }

    unsigned int total_len = 0;
    void *p = (void*)ethdriver_buf;

    if(!p)
    {
        printf("Couldn't Allocate Shared Buffer\n");
        return ETHIF_TX_FAILED;
    }

    /* Ensure that the total length of the packet being sent is less than the buffer size */
    for (int i = 0; i < num; i++) {
        total_len += len[i];
    }
    if (total_len > sizeof(Buf))
    {
        return 0;
    }

    total_len = 0;

    for (int i = 0; i < num; i++) {
        memcpy(p + total_len, (void*)phys[i], len[i]);
        total_len += len[i];
    }

    return ethdriver_tx(total_len);
}

static void handle_irq(struct eth_driver *driver, int irq)
{
    raw_poll(driver);
}

static struct raw_iface_funcs iface_fns = {
    .raw_handleIRQ = handle_irq,
    .print_state = NULL,
    .low_level_init = low_level_init,
    .raw_tx = raw_tx,
    .raw_poll = raw_poll
};

static int ethdriver_init(struct eth_driver *eth_driver, ps_io_ops_t io_ops, void *config)
{
    eth_driver->eth_data = NULL;
    eth_driver->dma_alignment = 1;
    eth_driver->i_fn = iface_fns;
    return 0;
}

static void* malloc_dma_alloc(void *cookie, size_t size, int align, int cached,
                              ps_mem_flags_t flags)
{
    assert(cached);
    int error;
    void *ret;
    error = posix_memalign(&ret, align, size);

    return (error ? NULL : ret);
}

static void malloc_dma_free(void *cookie, void *addr, size_t size)
{
    free(addr);
}

static uintptr_t malloc_dma_pin(void *cookie, void *addr, size_t size)
{
    return (uintptr_t)addr;
}

static void malloc_dma_unpin(void *cookie, void *addr, size_t size) {
}

static void malloc_dma_cache_op(void *cookie, void *addr, size_t size, dma_cache_op_t op) {
}

/* Global structures for Interfaces. Local copies initiailized and passed to functions */
static ps_io_ops_t io_ops;
static struct netif _netif;
static lwip_iface_t _lwip_driver;

/* pre_init:
 *
 *   Uses configuration values to setup a network interface. This does NOT interface with the
 *   low-level HW. That is what the ethdriver_tx, rx, and mac functions are for. But this DOES take
 *   the network packets and convert them to usable information
 *
 */
void pre_init(void) {
    struct ip_addr netmask, ipaddr, gw, multicast;
    struct netif *netif;
    lwip_iface_t *lwip_driver;
    memset(&io_ops, 0, sizeof(io_ops));
    io_ops.dma_manager = (ps_dma_man_t) {
        .cookie = NULL,
        .dma_alloc_fn = malloc_dma_alloc,
        .dma_free_fn = malloc_dma_free,
        .dma_pin_fn = malloc_dma_pin,
        .dma_unpin_fn = malloc_dma_unpin,
        .dma_cache_op_fn = malloc_dma_cache_op
    };
    lwip_driver = ethif_new_lwip_driver_no_malloc(io_ops, &io_ops.dma_manager, ethdriver_init, NULL,
                                                  &_lwip_driver);
    assert(lwip_driver);

    ipaddr_aton(gw_addr, &gw);
    ipaddr_aton(ip_addr, &ipaddr);
    ipaddr_aton(multicast_addr, &multicast);
    ipaddr_aton(mask_addr, &netmask);

    lwip_init();
    netif = netif_add(&_netif, &ipaddr, &netmask, &gw, lwip_driver,
                      ethif_get_ethif_init(lwip_driver), ethernet_input);
    assert(netif);

    netif_set_up(netif);
    netif_set_default(netif);

    if (ip_addr_ismulticast(&multicast)) {
        igmp_joingroup(&ipaddr, &multicast);
    }

    /* 1s Timer Event for updating webpage */
    event_timer_periodic(EVENT_TIMER, NS_IN_S);

    /* 250ms TCP Timer */
    event_timer_periodic(TCP_TIMER, NS_IN_S / 4);
}

/* Provided by the Ethdriver template.
 * Returns the Endpoint the Ethdriver will call when there is data for the router
 */
seL4_CPtr ethdriver_notification(void);

void event_handle(void);

/* run:
 *
 *   Waits on the ethdriver global endpoint, then calls the lwip sw interrupt (raw_poll) to handle
 *   the RX event. The ethdriver global endpoint is triggered by the ethdriver component through the
 *   client_emit function.
 */
int run(void)
{
    int err UNUSED;

    int global_ep = ethdriver_notification();
    seL4_Word badge;

    while(1)
    {
        seL4_Wait(global_ep, &badge);
        /* Handle Ethernet IRQ */
        if (badge == 0) {
            err = lwip_lock();
            ethif_lwip_handle_irq(&_lwip_driver, 0);
            err = lwip_unlock();
        }
        else if (badge == 1) {
            uint32_t completed = event_timer_completed();
            if (completed & BIT(TCP_TIMER)) {
                tcp_timer_handle();
            }
            if (completed & BIT(EVENT_TIMER)) {
                event_handle();
            }
        }
    }
}
