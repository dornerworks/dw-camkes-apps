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

/*
 * Copyright 2017, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DORNERWORKS_GPL)
 */

#include <camkes.h>

#include <lwip/def.h>

#include <string.h>

#include <fs.h>
#include <fsdata.h>

#include <web_files.h>

unsigned char data_print_cgi[] = _PRINT_CGI_DATA;
unsigned char data_time_cgi[MAX_FS_FILE_SIZE];

struct fsdata_file file_request_js[] = {{
        file_NULL, (unsigned char *)_REQUEST_JS_NAME, data_request_js, sizeof(data_request_js), 1,
}};

struct fsdata_file file_404_html[] = {{
        file_request_js, (unsigned char *)_404_PAGE_NAME, data_404_html, sizeof(data_404_html), 1,
}};

struct fsdata_file file_print_cgi[] = {{
        file_404_html, (unsigned char *)_PRINT_CGI, data_print_cgi, sizeof(data_print_cgi), 1,
}};

struct fsdata_file file_time_cgi[] = {{
        file_print_cgi, (unsigned char *)_TIMER_CGI, data_time_cgi, sizeof(data_time_cgi), 1,
}};

struct fsdata_file file__index_html[] = {{
    file_time_cgi, (unsigned char *)_INDEX_PAGE_NAME, data_index_html, sizeof(data_index_html), 1,
}};

void fs__init(void) {

    strncpy((char *)data_time_cgi, _TIMER_CGI_DATA, strlen(_TIMER_CGI_DATA));
}
