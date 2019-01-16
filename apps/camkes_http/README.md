<!---
 * Copyright 2019, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DornerWorks_GPL)
 -->

# Overview of CAmkES HTTP Server
This project utilizes the CAmkES protocol to implement an HTTP Server that can be accessed by anyone
on the network.

## Getting the Source Code
```
$ mkdir camkes_http; cd camkes_http
$ repo init -u https://github.com/dornerworks/dw-camkes-manifests -m http_server.xml
$ repo sync
$ mkdir build && cd build
$ ../init-build.sh -DCROSS_COMPILER_PREFIX=arm-linux-gnueabi-
$ ninja
```
## System Setup
This system contains three major components that are used to setup the Server: FileSystem, Router, &
Ethdriver. This document will overview each component's purpose and how the Server functions.

### Router

The Router component is responsible for communicating with the lwIP ethernet drivers to create and
decode TCP Packets, and interfacing with the Ethdriver component to send/receive packets in order to
communicate with the HTTP protocol.

The router _uses_ the Ethdriver interface to interface with the Ethdriver component. This gives the
router access to the following functions:

- _rx: Ethdriver reads from the HW RX Buffer and stores the Packet in the shared ethdriver buffer for the Router to use.
- _tx: The Router sends a Packet to the Ethdriver for the Ethdriver to transmit
- _mac: The Router gets the configured MAC Address

To connect the interface that the Ethdriver provides with the Router, the __seL4Ethdriver__
connection (defined in _procedures.camkes_) is used. The templates are found in the _templates_
directory. The Router source code knows the capability number to the global endpoint defined in the
configuration file. This allows the Router to wait for the Ethdriver to signal that information is
available for parsing.

The Router also contains several important attributes, including the __ip_addr__ string. This
is the IP Address at which the Server can be found. The Port is predefined to be 80, the HTTP default.

The Router uses the HTTPServer.template.c file to create a Server that "interfaces" with a
non-existant hardware interface. When the user accessing the Server through their chosen browser, a
GET request is sent to the Router, which routes it to the __/_recv__ function for parsing, which
finds and sends the file requested by the client.

#### CGI

Common Gateway Interface (CGI) is a protocol for webservers to execute programs by requesting a file
on the server.

When the router receives a CGI request, it checks to see if the name matches any of the CGI handler
names, and if so, calls the handler function. This function can perform a task, update a file, and
eventually return a file to the client.

### FileSystem

The FileSystem component stores the files and interfaces with the Router to provide the webpage
data.

The FileSystem is implemented as a linked list, with a pointer to the next file, name, data, and
other information. The definitions for each file are found in __fsdata.h__, the implementation for
each file are found in __fsdata.c__, and the data can be found in the generated header files, which
contain arrays of each neccesarry web page.

To generate these arrays, the __makefs.py__ script is used. Any file in the __web__ directory will
have a C Array of its contents created for the FileSystem to use.

Each file has a maximum file size, defined in __fs.h__

```
#define MAX_FS_FILE_SIZE 4096

typedef struct fs_file
{
    char data[MAX_FS_FILE_SIZE];
```

For our simple webpages, this small size will suffice. For larger webpages, increase this
definition.

To get each file, the __Router__ must call the  __open__ interface function with the name of the
requested file as a input argument. The function searches through the linked list, and if it finds a
name match, it stores the file information in the shared buffer, which is defined as the file
structure in the component's configuration.

```
dataport fs_file_t fs_mem;
```

```
if (!strcmp(name, (char *)f->name)) {
    strncpy((char *)fs_mem->data, (const char *)f->data, f->len);
    fs_mem->len = f->len;
    fs_mem->index = f->len;
    fs_mem->pextension = NULL;
    fs_mem->http_header_included = f->http_header_included;
    return ERR_OK;
}
```

Shown below is the __get\_404\_file__ function which shows how the FileSystem interfaces with the
Router

```
    *uri = "/404.html";
    err = fs_open(*uri);
    memcpy(&hs->file_handle, (fs_file_t *)fs_mem, sizeof(fs_file_t));
```

### Ethdriver

The Ethdriver component is responsible for communicating with the HW Ethernet Drivers and the Router
to send and receive TCP Packets.

A **HWEthDriver** component exists for the Ethdriver component to communicate with the HW. This
components _emits_ an IRQ and has a dataport the size of *EthDriverMMIO_t*, which is 0x4000 since
the CAmkES default is 0x1000. The Ethdriver component _consumes_ this HW Interrupt and shares the dataport.

The Ethdriver _provides_ the Ethdriver interface to communicate with the Router
component. Therefore, the _rx, _tx, & _mac functions are provided in the ethdriver.c source code and
connected with the __seL4Ethdriver__ connection. The Ethdriver-to template contains functions to
interface with the configuration file. For example, the \_get\_mac function returns the MAC Address
in the configuration file. Since the Ethdriver is also designed to communicate with multiple
"Routers", there are also functions from the seL4MultiSharedData templates, like
\_num\_badges. These functions exist for the expansion of this system, yet are unneccesary in the
current setup.

## Multiple Servers

The Router is designed to handle multiple devices seamlessly; however, the HTTPServer-template.c
file predefines the port at which the server can be accessed at 80. If the user wants, they can
change the __HTTPD_SERVER_PORT__ definition in the template to an attribute defines in the camkes
configuration, since each server needs to be on a different port.

The other option is to setup multiple Routers, each with their own server, shown below.

Please note that neither of these options have been tested and are not supported.

The Ethdriver is designed to service multiple clients; however, doing so requires some simple
changes. In the configuration file, add another **seL4EthDriver** connection to the
*ethdriver.client* from the second Router component. Configure the components that will connect to
the router. Ensure that the attributes do not overlap; this will cause issues for the Ethdriver
knowing which client to forward packets to. The Router's IP Address and MAC Addresses must also be
different for packets to behave as expected.

```
router.ip_addr = "172.27.14.10";
router1.ip_addr = "172.27.14.11";
router.ethdriver_mac = [0x00, 0x19, 0xB8, 0x02, 0xC9, 0xEA];
router1.ethdriver_mac = [0x00, 0x19, 0xB8, 0x02, 0xC9, 0xEB];
```
Since one (or both) of these MAC Addresses is not the physical HW Address, the low-level ethernet
drivers must also be changed to run in *promiscuous* mode, which will forward packets into the
low-level drivers even if the packets are not destined for the hardware address. If that is not
done, the second router would not be able to receive packets.

The ethdriver component should be setup to automatically configure the low-level drivers to
promiscuous mode if it detects multiple clients. Please note that this will add overhead to the
system, since the ethdriver component needs to filter every received packet instead of the hardware
performing the same filtering process.
