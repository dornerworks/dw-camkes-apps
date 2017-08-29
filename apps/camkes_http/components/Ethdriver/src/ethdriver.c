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
 * ethdriver.c
 *
 * This is the source code for the ethdriver component. This component interfaces with the low-level
 * HW drivers (libethdrivers), which is different than the router component that interfaces with
 * the lwip drivers (liblwip).
 *
 * In the pre_init function, the component gets a "seL4" environment in order to perform DMA
 * operations on the RX and TX Buffers.
 *
 * When there is an RX event, the IRQ configured in the .camkes file triggers which calls the
 * handler in the low-level drivers. That driver interfaces with this file to get the physical address
 * of the next RX buffer and place the incoming packet in the buffer. The driver can then read that
 * packet and notify the proper client that it has data available.
 *
 * For a TX Event, the router component utilizes the ethdriver interfaces to call the _tx function,
 * which assumes a completed network packet, to send the packet.
 *
 *------------------------------------------------------------------------------------------------*/

#include <autoconf.h>

#include <string.h>

#include <camkes.h>
#include <platsupport/io.h>
#include <vka/vka.h>
#include <vka/capops.h>
#include <simple/simple.h>
#include <simple/simple_helpers.h>
#include <allocman/allocman.h>
#include <allocman/bootstrap.h>
#include <allocman/vka.h>
#include <sel4utils/vspace.h>
#include <ethdrivers/raw.h>
#include <defines.h>
#include <sel4utils/page_dma.h>
#include <sel4platsupport/io.h>

#include <platform.h>

#define RX_BUFS        256
#define CLIENT_RX_BUFS 128
#define CLIENT_TX_BUFS 128
#define BUF_SIZE       2048

#define MAC_ADDRESS_LEN 6

#define BRK_VIRTUAL_SIZE 400000000

/* Global variables for seL4 Environment used in system_init */
static allocman_t *allocman;
static char allocator_mempool[8388608];
static simple_t camkes_simple;
static vka_t vka;
static vspace_t vspace;
static sel4utils_alloc_data_t vspace_data;

reservation_t muslc_brk_reservation;
void *muslc_brk_reservation_start;
vspace_t *muslc_this_vspace;
static sel4utils_res_t muslc_brk_reservation_memory;

static struct eth_driver eth_driver;

ps_io_ops_t ioops;

/* Available in component.simple.c */
void camkes_make_simple(simple_t *simple);

/*
 * Structure used for RX and TX Buffers.
 * Vbuf and Pbuf are the virtual and physical buffer address
 */
typedef struct eth_buf {
    void *vbuf;
    void *pbuf;
    int len;
    int client;
} tx_buf_t;

typedef struct eth_buf pending_rx_t;

typedef struct client {
    /* this flag indicates whether we or not we need to notify the client
     * if new data is received. We only notify once the client observes
     * the last packet */
    int should_notify;

    int rx_head;
    int rx_tail;
    pending_rx_t rx[CLIENT_RX_BUFS];

    int num_tx;
    tx_buf_t tx_mem[CLIENT_TX_BUFS];
    tx_buf_t *tx[CLIENT_TX_BUFS];

    uint8_t mac[MAC_ADDRESS_LEN]; /* mac address for this client */
    int client_id;  /* id for this client */
    void *dataport; /* dataport for this client */

} client_t;

typedef struct map_page_data {
    void *ret_addr;
    void *map_addr;
    size_t num_pages;
    size_t page_size_bits;
    struct map_page_data *next;
} map_page_data_t;

typedef struct ethernet_io_mapper_cookie {
    vspace_t vspace;
    vka_t vka;
    map_page_data_t *head;
} ethernet_io_mapper_cookie_t;

/* Variables to keep track of the number of components (clients) connect to the Ethdriver */
static int num_clients = 0;
static client_t *clients = NULL;

/* Arrays to keep track of virtual and physical RX buffers */
static int num_rx_bufs;
static void *rx_vbufs[RX_BUFS];
static void *rx_pbufs[RX_BUFS];

/* Flag so interface functions don't try anything before Ethdriver is ready */
static int done_init = 0;

/* Functions provided by the Ethdriver template */
void client_emit(unsigned int client_id);
unsigned int client_get_sender_id(void);
unsigned int client_num_badges(void);
unsigned int client_enumerate_badge(unsigned int i);
void *client_buf(unsigned int client_id);
void client_get_mac(unsigned int client_id, uint8_t *mac);

/* Create the seL4 Management tools for DMA, Hardware Access, etc... */
static void init_system(void)
{
    int error UNUSED;

    /* Camkes adds nothing to our address space, so this array is empty */
    void *existing_frames[] = {
        NULL
    };

    /* Create the simple runtime environment. Has untyped, frames, etc... */
    camkes_make_simple(&camkes_simple);

    /* Create an allocator that handles the untyped management */
    allocman = bootstrap_use_current_simple(&camkes_simple, sizeof(allocator_mempool), allocator_mempool);
    assert(allocman);

    allocman_make_vka(&vka, allocman);

    /* Initialize the vspace */
    error = sel4utils_bootstrap_vspace(&vspace, &vspace_data,
                                       simple_get_init_cap(&camkes_simple, seL4_CapInitThreadPD), &vka,
                                       NULL, NULL, existing_frames);
    assert(!error);

    sel4utils_reserve_range_no_alloc(&vspace, &muslc_brk_reservation_memory, BRK_VIRTUAL_SIZE,
                                     seL4_AllRights, 1, &muslc_brk_reservation_start);
    muslc_this_vspace = &vspace;
    muslc_brk_reservation = (reservation_t){.res = &muslc_brk_reservation_memory};
}

/* Function to find Hardware Device Frames in the Simple Structure.
 *
 * This function allocates a slot in the CSpace to fill with the frame(s) found in the simple structure.
 * It also places the mapped address and size in front of a linked list, which is used to unmap the addresses
 * later.
 */
static void * eth_map_paddr_with_page_size(ethernet_io_mapper_cookie_t *cookie, uintptr_t paddr, size_t size, int page_size_bits, int cached)
{
    vka_t *vka = &cookie->vka;
    vspace_t *vspace = &cookie->vspace;
    simple_t *simple = &camkes_simple;

    /* search at start of page */
    int page_size = BIT(page_size_bits);
    uintptr_t start = ROUND_DOWN(paddr, page_size);
    uintptr_t offset = paddr - start;
    size += offset;

    /* Create a new structure for the data we're about to map */
    map_page_data_t *mapping = malloc(sizeof(map_page_data_t));
    assert(mapping);

    mapping->num_pages = BYTES_TO_SIZE_BITS_PAGES(size, page_size_bits);
    mapping->page_size_bits = page_size_bits;

    /* calculate number of pages */
    unsigned int num_pages = ROUND_UP(size, page_size) >> page_size_bits;
    assert((num_pages << page_size_bits) >= size);
    seL4_CPtr frames[num_pages];

    /* get all of the physical frame caps */
    for (unsigned int i = 0; i < num_pages; i++)
    {
        /* allocate a cslot */
        int error = vka_cspace_alloc(vka, &frames[i]);
        if (error) {
            ZF_LOGE("cspace alloc failed");
            assert(error == 0);
            /* we don't clean up as everything has gone to hell */
            return NULL;
        }

        /* create a path */
        cspacepath_t path;
        vka_cspace_make_path(vka, frames[i], &path);

        /* See if the frame exists in the simple structure */
        error = simple_get_frame_cap(simple, (void*)start + (i * page_size), page_size_bits, &path);
        if (error) {
            /* free this slot, and then do general cleanup of the rest of the slots.
             * this avoids a needless seL4_CNode_Delete of this slot, as there is no
             * cap in it */
            vka_cspace_free(vka, frames[i]);
            num_pages = i;
            goto error;
        }
    }

    /* Now map the frames in */
    void *vaddr = vspace_map_pages(vspace, frames, NULL, seL4_AllRights, num_pages, page_size_bits, cached);
    if (vaddr) {
        mapping->map_addr = vaddr;
        mapping->ret_addr = vaddr + offset;

        /* Set the 'Next' strcuture to the current head. Take that for DATA */
        mapping->next = cookie->head;
        cookie->head = mapping;

        return mapping->ret_addr;
    }

error:
    for (unsigned int i = 0; i < num_pages; i++) {
        cspacepath_t path;
        vka_cspace_make_path(vka, frames[i], &path);
        vka_cnode_delete(&path);
        vka_cspace_free(vka, frames[i]);
    }
    free(mapping);
    return NULL;
}

/* Function to Map Hardware Devices to frames.
 *
 * Essentially, we are assuming that any and all hardware devices needed by the Ethdriver component
 * are declared in the ADL and capabilities for them have been created in the Simple template. Therefore,
 * we don't need to worry about finding any other frames.
 *
 * This function essentially calls the eth_map_paddr_with_page_size until that function finds a frame in
 * the simple structure.
 *
 * Function based on the camkes-arm-vm in the SEL4PROJ repositories, which I think is based off the
 * platsupport library function.
 */
static void * eth_map_paddr(void *cookie, uintptr_t paddr, size_t size, int cached, ps_mem_flags_t flags)
{
    ethernet_io_mapper_cookie_t* io_mapper = (ethernet_io_mapper_cookie_t*)cookie;
    int frame_size_index;

    /* find the largest reasonable frame size */
    for (frame_size_index = 0; frame_size_index + 1 < SEL4_NUM_PAGE_SIZES; frame_size_index++) {
        if (size >> sel4_page_sizes[frame_size_index + 1] == 0) {
            break;
        }
    }

    /* try mapping in this and all smaller frame sizes until something works */
    for (int i = frame_size_index; i >= 0; i--) {
        void *result = eth_map_paddr_with_page_size(io_mapper, paddr, size, sel4_page_sizes[i], cached);
        if (result) {
            return result;
        }
    }

    return NULL;
}

/* Function to unmap the device frames in our vspace
 *
 * In the map function, we create a linked list that keeps track of the virtual
 * address space and page size bits. The virtual address gets passed in, and we
 * use the vspace function to unmap that page and we remove it from our list.
 */
static void eth_unmap_vaddr(void * cookie, void *vaddr, UNUSED size_t size)
{
    ethernet_io_mapper_cookie_t* io_mapper = cookie;

    vspace_t *vspace = &io_mapper->vspace;
    vka_t *vka = &io_mapper->vka;
    map_page_data_t *current;
    map_page_data_t *prev = NULL;

    for(current = io_mapper->head; current != NULL; current = current->next) {
        if (current->ret_addr == vaddr) {
            break;
        }
        prev = current;
    }

    /* unmap the pages */
    vspace_unmap_pages(vspace, current->map_addr, current->num_pages, current->page_size_bits,
                       vka);

    if (prev->next != NULL)
    {
        /* Time to remove that node from our list! */
        prev->next = current->next;
    }

    free(current);
}

/* eth_tx_complete:
 *
 *   Update the client with a cookie that contains the virtual and physical address of the TX Buffer
 */
static void eth_tx_complete(void *iface, void *cookie)
{
    tx_buf_t *buf = (tx_buf_t*)cookie;
    if (buf == NULL)
    {
        printf("eth_tx_complete - TX Buffer NULL\n");
        return;
    }
    client_t *client = &clients[buf->client];
    client->tx[client->num_tx] = buf;
    client->num_tx++;
}

/* give_client_buf:
 *
 *   Pass in a cookie that contains the virtual and physical address of the RX Buffer which contains
 *   the ethernet packet we want. We create a local copy of that information, then set the vaddr and
 *   paddr in the cookie
 */
static uintptr_t eth_allocate_rx_buf(void *iface, size_t buf_size, void **cookie)
{
    /* If the buffer to be received is too big or there aren't any receive buffers left  */
    if ((buf_size > BUF_SIZE) || (0 == num_rx_bufs)) {
        return 0;
    }

    struct dma_buf_cookie *bufs = (struct dma_buf_cookie *)cookie[0];
    if (bufs == NULL)
    {
        printf("eth_allocate_rx_buf - Buffer Location NULL\n");
        return 0;
    }

    num_rx_bufs--;

    /* The system is designed for ARM, for which some platforms don't have an IOMMU with
     * a 1:1 mapping, so vaddr /= paddr. We allocated another buffer array that stores the
     * physical address, which the HW needs. We pass the virtual address into the cookie
     * which gets passed back into this driver, while the paddr gets used by the wrapper
     * We also invalidate the existing cache of the virtual buffer to ensure proper data is used
     */
    void *pbuf = rx_pbufs[num_rx_bufs];
    void *vbuf = rx_vbufs[num_rx_bufs];

    if (pbuf == NULL || vbuf == NULL)
    {
        printf("Couldn't find Virtual or Physical Buffer in Global Arrays\n");
        return 0;
    }

    ps_dma_cache_invalidate(&ioops.dma_manager, vbuf, BUF_SIZE);

    bufs->vbuf = vbuf;
    bufs->pbuf = pbuf;

    return (uintptr_t)pbuf;
}

/* detect_client:
 *
 *   Uses the incoming RX buffer to detect whether its of any use to our clients
 *   by comparing the destination MAC addr
 */
static client_t *detect_client(void *buf, unsigned int len)
{
    if (buf == NULL)
    {
        printf("detect_client - Passed in MAC Address NULL\n");
        return NULL;
    }
    if (len < MAC_ADDRESS_LEN) {
        return NULL;
    }

    for (int i = 0; i < num_clients; i++) {
        if (0 == memcmp(clients[i].mac, buf, MAC_ADDRESS_LEN)) {
            return &clients[i];
        }
    }
    return NULL;
}

/* is_broadcast:
 *
 *   Compare the first 6 bytes of the RX buffer to check if they're broadcasting
 */
static int is_broadcast(void *buf, unsigned int len)
{
    if (buf == NULL)
    {
        printf("is_broadcast - Passed in MAC Address NULL\n");
        return 0;
    }

    static uint8_t broadcast[MAC_ADDRESS_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    if (len < MAC_ADDRESS_LEN)
    {
        return 0;
    }
    if (0 == memcmp(buf, broadcast, MAC_ADDRESS_LEN))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/* give_client_buf:
 *
 *   Pass in a cookie that contains the virtual and physical address of the RX Buffer which contains
 *   the ethernet packet we want. We create a local copy of that information, then set the client RX
 *   buffer to the proper information. If the client is set to notify, we use the template
 *   function "_emit" to notify the client that information is available on the shared buffer
 */
static void give_client_buf(client_t *client, void *cookie, unsigned int len)
{
    if (cookie == NULL)
    {
        printf("give_client_buf - Passed in Buffers NULL\n");
        return;
    }
    struct dma_buf_cookie *bufs = (struct dma_buf_cookie *)cookie;
    client->rx[client->rx_head] = (pending_rx_t){bufs->vbuf, bufs->pbuf, len, 0};

    client->rx_head = (client->rx_head + 1) % CLIENT_RX_BUFS;
    if (client->should_notify)
    {
        client_emit(client->client_id);
        client->should_notify = 0;
    }
}

/* eth_rx_complete:
 *
 *   Pass in a pointer to a cookie that contains the virtual and physical address of the RX Buffer
 *   which contains the ethernet packet we want. We create a local copy of that information, then
 *   detect the client by comparing the MAC address of the packet to the clients MAC address. If the
 *   client exists and the buffer is in the right position, we then give the buffer to the proper
 *   client!
 */
static void eth_rx_complete(void *iface, unsigned int num_bufs, void **cookies, unsigned int *lens)
{
    /* insert filtering here. currently everything just goes to one client */
    if (1 != num_bufs) {
        goto error;
    }

    struct dma_buf_cookie *bufs = (struct dma_buf_cookie *)cookies[0];
    if (bufs == NULL)
    {
        printf("eth_rx_complete - RX Buffers NULL\n");
        return;
    }

    client_t *client = detect_client(bufs->vbuf, lens[0]);
    if (!client) {
        if (is_broadcast(bufs->vbuf, lens[0])) {
            /* in a broadcast duplicate this buffer for every other client, we will fallthrough
             * to give the buffer to client 0 at the end */
            for (int i = 1; i < num_clients; i++) {
                client = &clients[i];
                if ((client->rx_head + 1) % CLIENT_RX_BUFS != client->rx_tail) {
                    struct dma_buf_cookie cookie_bufs;
                    void *cookie = &cookie_bufs;
                    void *buf = (void*)eth_allocate_rx_buf(iface, lens[0], &cookie);
                    if (buf) {
                        memcpy(cookie_bufs.vbuf, bufs->vbuf, lens[0]);
                        give_client_buf(client, cookie, lens[0]);
                    }
                }
            }
        } else {
            goto error;
        }
        client = &clients[0];
    }
    if ((client->rx_head + 1) % CLIENT_RX_BUFS == client->rx_tail) {
        goto error;
    }
    give_client_buf(client, bufs, lens[0]);
    return;

error:
    /* abort and put all the bufs back */
    for (int i = 0; i < num_bufs; i++) {
        bufs = (struct dma_buf_cookie *)cookies[i];
        rx_vbufs[num_rx_bufs] = bufs->vbuf;
        rx_pbufs[num_rx_bufs] = bufs->pbuf;
        num_rx_bufs++;
    }
}


/* ethdriver_callbacks (function typedefs) stored in raw.h, passed into the low-level HW. */
static struct raw_iface_callbacks ethdriver_callbacks = {
    .tx_complete = eth_tx_complete,
    .rx_complete = eth_rx_complete,
    .allocate_rx_buf = eth_allocate_rx_buf
};

/* get_client:
 *
 *   Pass in an ID and this function loops through and returns the address of client pointer
 */
static client_t * get_client(int id)
{
    for (int i = 0; i < num_clients; i++) {
        if (id == clients[i].client_id) {
            return &clients[i];
        }
    }
    return NULL;
}

/* client_rx:
 *
 *   Interface function; can be called through ethdriver_rx(&len) in the router component
 *
 *   Determines the Sender ID, then allocates a tx_buf (with vaddr & paddr) that is claimed by
 *   the client. Copies the clients dataport to the virtual address. Also copies the clients MAC
 *   address (not sure if neccessary, but its a good backup plan in case the packet isn't properly
 *   formed. The virtual buf's cache is cleaned and the raw transmit function is called with the
 *   corresponding **physical** buffer.
 */
int client_rx(int *len) {

    int err UNUSED;

    if (!done_init) {
        return -1;
    }

    if (len == NULL)
    {
        printf("Passed in length pointer NULL\n");
        return RX_FAILURE;
    }

    int ret;

    client_t *client = get_client(client_get_sender_id());
    if (client == NULL)
    {
        printf("Could not associate Sender ID w/ Client\n");
        return RX_FAILURE;
    }

    void *packet = client->dataport;
    if (packet == NULL)
    {
        printf("Shared Dataport not available\n");
        return RX_FAILURE;
    }

    err = ethdriver_lock();

    if (client->rx_head == client->rx_tail)
    {
        client->should_notify = 1;
        err = ethdriver_unlock();
        return RX_FAILURE;
    }

    pending_rx_t rx = client->rx[client->rx_tail];
    client->rx_tail = (client->rx_tail + 1) % CLIENT_RX_BUFS;
    memcpy(packet, rx.vbuf, rx.len);
    *len = rx.len;

    if (client->rx_tail == client->rx_head)
    {
        client->should_notify = 1;
        ret = RX_COMPLETE;
    }
    else
    {
        ret = RX_INCOMPLETE;
    }

    rx_vbufs[num_rx_bufs] = rx.vbuf;
    rx_pbufs[num_rx_bufs] = rx.pbuf;
    num_rx_bufs++;

    memset(rx.vbuf, 0, BUF_SIZE);
    ps_dma_cache_clean_invalidate(&ioops.dma_manager, rx.vbuf, BUF_SIZE);

    err = ethdriver_unlock();

    return ret;
}

/* client_tx:
 *
 *   Interface function; can be called through ethdriver_tx(len) in the router component
 *
 *   Determines the Sender ID, then allocates a tx_buf (with vaddr & paddr) that is claimed by
 *   the client. Copies the clients dataport to the virtual address. Also copies the clients MAC
 *   address (not sure if neccessary, but its a good backup plan in case the packet isn't properly
 *   formed. The virtual buf's cache is cleaned and the raw transmit function is called with the
 *   corresponding **physical** buffer.
 */
int client_tx(int len) {

    int err UNUSED;

    /* Cannot tx if initialization isn't complete/packet length doesn't have src+dst addrs */
    if (!done_init || (len < 12)) {
        return ETHIF_TX_FAILED;
    }
    len = ((len > BUF_SIZE) ? BUF_SIZE : len);

    client_t *client = get_client(client_get_sender_id());
    if (client == NULL)
    {
        printf("client_tx - Could not associate Sender ID w/ Client\n");
        return ETHIF_TX_FAILED;
    }

    void *packet = client->dataport;
    if (packet == NULL)
    {
        printf("client_tx - Shared Dataport not available\n");
        return ETHIF_TX_FAILED;
    }

    err = ethdriver_lock();

    if (0 != client->num_tx)
    {
        client->num_tx--;
        tx_buf_t *tx_buf = client->tx[client->num_tx];
        if (tx_buf == NULL)
        {
            printf("client_tx - Transmit Buffer NULL\n");
            err = ethdriver_unlock();
            return ETHIF_TX_FAILED;
        }

        memcpy(tx_buf->vbuf, packet, len);

        /* Copy client's MAC Address into packet */
        uint8_t *hdr = (uint8_t *)tx_buf->vbuf;
        if (hdr == NULL)
        {
            printf("client_tx - Virtual TX Buffer NULL\n");
            err = ethdriver_unlock();
            return ETHIF_TX_FAILED;
        }

        hdr += MAC_ADDRESS_LEN;
        for(int i = 0; i < MAC_ADDRESS_LEN; i++)
        {
            hdr[i] = client->mac[i];
        }
        ps_dma_cache_clean(&ioops.dma_manager, tx_buf->vbuf, BUF_SIZE);

        /* queue up transmit */
        eth_driver.i_fn.raw_tx(&eth_driver, 1, (uintptr_t*)&(tx_buf->pbuf),
                               (unsigned int*)&len, tx_buf);
    }
    err = ethdriver_unlock();
    return ETHIF_TX_COMPLETE;
}

/* client_mac:
 *
 *   Interface function; can be called through ethdriver_mac(...) in the router component
 *
 *   Determines the Sender ID, then grabs the mac address stored in the client structure
 *   and sets the pointer values appropriately.
 *
 */
int client_mac(uint8_t *b1, uint8_t *b2, uint8_t *b3, uint8_t *b4, uint8_t *b5, uint8_t *b6)
{
    /* Ensure pointers are valid */
    if (!b1 || !b2 || !b3 || !b4 || !b5 || !b6)
    {
        printf("MAC Address passed in Void Pointer\n");
        return 0;
    }

    if (!done_init) {
        return 0;
    }

    client_t *client = get_client(client_get_sender_id());
    if (!client)
    {
        printf("Could not associate Sender ID w/ Client\n");
        return 0;
    }

    int err UNUSED;
    err = ethdriver_lock();
    *b1 = client->mac[0];
    *b2 = client->mac[1];
    *b3 = client->mac[2];
    *b4 = client->mac[3];
    *b5 = client->mac[4];
    *b6 = client->mac[5];
    err = ethdriver_unlock();

    return 1;
}

/* irq_handle:
 *
 *    Locks the ethdriver, calls the handleIRQ function in imx6.c, then re-registers
 *    itself as the IRQ callback function. Unlocks the mutex.
 */
void irq_handle(void)
{
    int UNUSED err;
    err = ethdriver_lock();
    eth_driver.i_fn.raw_handleIRQ(&eth_driver, 0);
    err = irq_acknowledge();
    err = ethdriver_unlock();
}

/* Post Init:
 *
 *   Initializes the ".simple" system, which allows for DMA operations on
 *   the RX & TX Buffers
 *
 *   Initializes the RX & TX Buffers as virtual/physical DMA pairings without
 *   a 1->1  mapping. Therefore, vbuf != pbuf which has to be accounted for throughout
 *   both this file and the low-level HW drivers (imx6.c).
 *
 *   Initializes the low-level HW & the ISR
 */
void post_init(void)
{
    int error UNUSED;

    error = ethdriver_lock();

    /* initialize seL4 allocators and give us a half sane environment */
    init_system();

    /*
     * When seL4 got upgraded to 4.0, device memory gets allocated from the allocator,
     * not simple. So the allocation manager doesn't know about the capabilities of the
     * hardware frames decalred as mmios in the ADL. Therefore, we need to handle the
     * hardware mapping and unmapping ourselves.
     */
    ethernet_io_mapper_cookie_t *cookie = malloc(sizeof(ethernet_io_mapper_cookie_t));
    assert(cookie);

    cookie->vka = vka;
    cookie->vspace = vspace;
    cookie->head = NULL;

    ioops.io_mapper.cookie = cookie;
    ioops.io_mapper.io_map_fn = eth_map_paddr;
    ioops.io_mapper.io_unmap_fn = eth_unmap_vaddr;

#ifdef CONFIG_PLAT_ZYNQ7000
    clock_sys_init(&ioops, &ioops.clock_sys);
#endif

    error = sel4utils_new_page_dma_alloc(&vka, &vspace, &ioops.dma_manager);
    assert(!error);

    /* preallocate buffers */
    for (int i = 0; i < RX_BUFS; i++) {

        /* Allocate cacheable buffer, pin to physical address */
        void *vbuf = ps_dma_alloc(&ioops.dma_manager, BUF_SIZE, 4, 1, PS_MEM_NORMAL);
        assert(vbuf);
        void *phys = (void *)ps_dma_pin(&ioops.dma_manager, vbuf, BUF_SIZE);
        assert(phys);

        /* Store vaddr & paddr in global arrays */
        rx_vbufs[num_rx_bufs] = vbuf;
        rx_pbufs[num_rx_bufs] = phys;
        num_rx_bufs++;

        /* Clean & invalidate the buffer to ignore any information that exists there already */
        ps_dma_cache_clean_invalidate(&ioops.dma_manager, vbuf, BUF_SIZE);
    }

    /* Determine the number of clients that need TX Buffers */
    num_clients = client_num_badges();
    clients = calloc(num_clients, sizeof(client_t));

    /* Loop through & initialize each client with TX Buffers & client information */
    for (int client = 0; client < num_clients; client++) {

        /* Get client information from configuration files */
        clients[client].should_notify = 1;
        clients[client].client_id = client_enumerate_badge(client);
        clients[client].dataport = client_buf(clients[client].client_id);
        client_get_mac(clients[client].client_id, clients[client].mac);

        /* Initialize TX Buffers */
        for (int i = 0; i < CLIENT_TX_BUFS; i++) {

            /* Allocate cacheable buffer, pin to physical address */
            void *vbuf = ps_dma_alloc(&ioops.dma_manager, BUF_SIZE, 4, 1, PS_MEM_NORMAL);
            assert(vbuf);
            void *phys = (void *)ps_dma_pin(&ioops.dma_manager, vbuf, BUF_SIZE);
            assert(phys);

            /* Store TX Buffer in Client Structure */
            tx_buf_t *tx_buf = &clients[client].tx_mem[i];
            *tx_buf = (tx_buf_t){.vbuf = vbuf, .pbuf = phys, .len = BUF_SIZE, .client = client};
            clients[client].tx[i] = tx_buf;
            ps_dma_cache_clean_invalidate(&ioops.dma_manager, vbuf, BUF_SIZE);
        }
        clients[client].num_tx = CLIENT_TX_BUFS - 1;
    }

    eth_driver.cb_cookie = NULL;
    eth_driver.i_cb = ethdriver_callbacks;

    struct eth_plat_config *config = (struct eth_plat_config *)malloc(sizeof(struct eth_plat_config));
    plat_configure_ethdriver(config);

    if (num_clients > 1) {
        config->prom_mode = 1;
    }
    else {
        client_get_mac(clients[0].client_id, config->mac_addr);
        config->prom_mode = 0;
    }

    error = config->init(&eth_driver, ioops, (void *)config);
    assert(!error);

    done_init = 1;

    error = irq_acknowledge();
    error = ethdriver_unlock();
}
