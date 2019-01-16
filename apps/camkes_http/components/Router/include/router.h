/*
 * Copyright 2019, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DORNERWORKS_GPL)
 */

#pragma once

#define HARDWARE_ROUTER_INTERFACES                                              \
    uses HTTPServer http_server;                                                \

#define HARDWARE_ROUTER_COMPOSITION                                             \
    component Router_hw router_hw;                                              \
    connection seL4HTTPServer httpserver(from http_server, to router_hw.http);  \
