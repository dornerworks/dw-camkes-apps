/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#ifndef __FSDATA_H__
#define __FSDATA_H__

#include <lwip/opt.h>
#include <fs.h>

#define file_NULL (struct fsdata_file *) NULL
#define FS_ROOT file__index_html
#define FS_NUMFILES 5

#define HTTP_HEAD "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n"

#define _REQUEST_JS_NAME "/request.js"
#define _404_PAGE_NAME "/404.html"
#define _INDEX_PAGE_NAME "/index.html"

#define _TIMER_CGI "/timerdata.sts"

/* Note:
 *   The fsdata_file structure has a pointer to data. If the CGI data is going to
 *   get updated, it might begin to overlap. So be careful!
 */
#define _TIMER_CGI_DATA "<span style=\"font-family: terminal, monaco, monospace; color: #525252;\">0 Days, 0 Hours, 0 Minutes, 0 Seconds"
#define _PRINT_CGI "/print.cgi"
#define _PRINT_CGI_DATA "PRINTED"

extern unsigned char data_404_html[];
extern unsigned char data_index_html[];
extern unsigned char data_request_js[];
extern unsigned char data_print_cgi[];
extern unsigned char data_time_cgi[];

extern unsigned char *time_data;

struct fsdata_file
{
    struct fsdata_file *next;
    unsigned char *name;
    unsigned char *data;
    int len;
    u8_t http_header_included;
};

extern struct fsdata_file file__index_html[];

#endif // __FSDATA_H__
