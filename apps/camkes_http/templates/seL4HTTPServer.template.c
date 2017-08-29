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

#undef ERR_IF

#include <lwip/debug.h>
#include <lwip/stats.h>
#include <lwip/tcp.h>
#include <lwip/mem.h>
#include <lwip/raw.h>
#include <lwip/icmp.h>
#include <lwip/netif.h>
#include <lwip/sys.h>
#include <lwip/timers.h>
#include <lwip/inet_chksum.h>
#include <lwip/init.h>
#include <netif/etharp.h>

#include <string.h>

/*
 * Priority for tcp pcbs created by HTTPD (very low by default).
 * Lower priorities get killed first when running out of memroy.
 */
#define HTTPD_TCP_PRIO TCP_PRIO_MIN

/* The server port for HTTPD to use */
#define HTTPD_SERVER_PORT 80

/* The poll delay is X*500ms*/
#define HTTPD_POLL_INTERVAL 4

/*
 * Maximum retries before the connection is aborted/closed.
 * - number of times pcb->poll is called -> default is 4*500ms = 2s;
 * - reset when pcb->sent is called
 */
#define HTTPD_MAX_RETRIES 4

#define LWIP_HTTPD_MAX_CGI_PARAMETERS 16

/* Number of rx pbufs to enqueue to parse an incoming request (up to the first newline) */
#define LWIP_HTTPD_REQ_QUEUELEN 5

/* Number of (TCP payload-) bytes (in pbufs) to enqueue to parse and incoming
 * request (up to the first double-newline)
 */
#define LWIP_HTTPD_REQ_BUFSIZE LWIP_HTTPD_MAX_REQ_LENGTH

/* Defines the maximum length of a HTTP request line (up to the first CRLF, */
/* copied from pbuf into this a global buffer when pbuf- or packet-queues */
/* are received - otherwise the input pbuf is used directly) */
#define LWIP_HTTPD_MAX_REQ_LENGTH LWIP_MIN(1023, (LWIP_HTTPD_REQ_QUEUELEN * PBUF_POOL_BUFSIZE))

/* Minimum length for a valid HTTP/0.9 request: "GET /\r\n" -> 7 bytes */
#define MIN_REQ_LEN 7

#define CRLF "\r\n"

/* Return values for http_send_*() */
#define HTTP_DATA_TO_SEND_BREAK 2
#define HTTP_DATA_TO_SEND_CONTINUE 1
#define HTTP_NO_DATA_TO_SEND 0

#define HTTP_IS_DATA_VOLATILE(hs)                                            \
    (((hs->file != NULL) && (hs->handle != NULL) &&                          \
      (hs->file == (char *)hs->handle->data + hs->handle->len - hs->left)) ? \
         0 :                                                                 \
         TCP_WRITE_FLAG_COPY)

#define HTTP_ALLOC_HTTP_STATE() (struct http_state *) mem_malloc(sizeof(struct http_state))
#define /*? me.interface.name ?*/_find_error_file(hs, error_nr) ERR_ARG

/*- include 'seL4HTTPServer-cgi.template.c' -*/

typedef struct
{
    const char *name;
    u8_t shtml;
} default_filename;

const default_filename g_psDefaultFilenames[] = {
    {"/index.html", 0}
};

#define NUM_DEFAULT_FILENAMES (sizeof(g_psDefaultFilenames) / sizeof(default_filename))

static char httpd_req_buf[LWIP_HTTPD_MAX_REQ_LENGTH + 1];

struct http_state
{
    struct fs_file file_handle;
    struct fs_file *handle;
    char *file; /* Pointer to first unsent byte in buf. */

    struct tcp_pcb *pcb;
    struct pbuf *req;

    u32_t left;  /* Number of unsent bytes in buf. */
    u8_t retries;

    char *params[LWIP_HTTPD_MAX_CGI_PARAMETERS];     /* Params extracted from the request URI */
    char *param_vals[LWIP_HTTPD_MAX_CGI_PARAMETERS]; /* Values for each extracted param */
};

static err_t /*? me.interface.name ?*/_close_conn(struct tcp_pcb *pcb, struct http_state *hs);
static err_t /*? me.interface.name ?*/_close_or_abort_conn(struct tcp_pcb *pcb, struct http_state *hs, u8_t abort_conn);
static err_t /*? me.interface.name ?*/_find_file(struct http_state *hs, const char *uri, int is_09);
static err_t /*? me.interface.name ?*/_init_file(struct http_state *hs, struct fs_file *file, int is_09, const char *uri, u8_t tag_check);
static err_t /*? me.interface.name ?*/_poll(void *arg, struct tcp_pcb *pcb);

/* Like strstr but does not need 'buffer' to be NULL-terminated */
static char *strnstr(const char *buffer, const char *token, size_t n)
{
    const char *p;
    int tokenlen = (int)strlen(token);
    if (tokenlen == 0)
    {
        return (char *)buffer;
    }
    for (p = buffer; *p && (p + tokenlen <= buffer + n); p++)
    {
        if ((*p == *token) && (strncmp(p, token, tokenlen) == 0))
        {
            return (char *)p;
        }
    }
    return NULL;
}

/* Free a struct http_state. Also frees the file data if dynamic. */
static void /*? me.interface.name ?*/_state_eof(struct http_state *hs)
{
    if (hs->handle) {
        fs_close();
        hs->handle = NULL;
    }
}

/* Free a struct http_state. Also frees the file data if dynamic. */
static void /*? me.interface.name ?*/_state_free(struct http_state *hs)
{
    if (hs != NULL) {
        /*? me.interface.name ?*/_state_eof(hs);
        mem_free(hs);
    }
}

/* Initialize a struct http_state.*/
static void /*? me.interface.name ?*/_state_init(struct http_state *hs)
{
    /* Initialize the structure. */
    memset(hs, 0, sizeof(struct http_state));
}

/* Allocate a struct http_state. */
static struct http_state */*? me.interface.name ?*/_state_alloc(void)
{
    struct http_state *ret = HTTP_ALLOC_HTTP_STATE();
    if (ret != NULL) {
        /*? me.interface.name ?*/_state_init(ret);
    }
    return ret;
}

/*!
 * @brief Call tcp_write() in a loop trying smaller and smaller length
 *
 * @param pcb tcp_pcb to send
 * @param ptr Data to send
 * @param length Length of data to send (in/out: on return, contains the
 *        amount of data sent)
 * @param apiflags directly passed to tcp_write
 * @return the return value of tcp_write
 */
static err_t /*? me.interface.name ?*/_write(struct tcp_pcb *pcb, const void *ptr, u16_t *length, u8_t apiflags)
{
    u16_t len;
    err_t err;

    LWIP_ASSERT("length != NULL", length != NULL);
    len = *length;

    if (len == 0) {
        return ERR_OK;
    }
    do {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Trying go send %d bytes\r\n", len));
        err = tcp_write(pcb, ptr, len, apiflags);
        if (err == ERR_MEM) {
            if ((tcp_sndbuf(pcb) == 0) || (tcp_sndqueuelen(pcb) >= TCP_SND_QUEUELEN)) {
                len = 1;  /* no need to try smaller sizes */
            }
            else {
                len /= 2;
            }
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Send failed, trying less (%d bytes)\r\n", len));
        }
    } while ((err == ERR_MEM) && (len > 1));

    if (err == ERR_OK) {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Sent %d bytes\r\n", len));
    }
    else {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Send failed with err %d (\"%s\")\r\n", err, lwip_strerr(err)));
    }

    *length = len;
    return err;
}

/*!
 * @brief The connection shall be actively closed (using RST to close from fault states).
 * Reset the sent- and recv-callbacks.
 *
 * @param pcb the tcp pcb to reset callbacks
 * @param hs connection state to free
 */
static err_t /*? me.interface.name ?*/_close_or_abort_conn(struct tcp_pcb *pcb, struct http_state *hs, u8_t abort_conn)
{
    err_t err;
    LWIP_DEBUGF(HTTPD_DEBUG, ("Closing connection %p\r\n", (void *)pcb));

    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_err(pcb, NULL);
    tcp_poll(pcb, NULL, 0);
    tcp_sent(pcb, NULL);

    if (hs != NULL) {
        /*? me.interface.name ?*/_state_free(hs);
    }
    if (abort_conn) {
        tcp_abort(pcb);
        return ERR_OK;
    }
    err = tcp_close(pcb);
    if (err != ERR_OK) {
        LWIP_DEBUGF(HTTPD_DEBUG, ("Error %d closing %p\r\n", err, (void *)pcb));
        /* error closing, try again later in poll */
        tcp_poll(pcb, /*? me.interface.name ?*/_poll, HTTPD_POLL_INTERVAL);
    }
    return err;
}

/*!
 * @brief The connection shall be actively closed.
 * Reset the sent- and recv-callbacks.
 *
 * @param pcb the tcp pcb to reset callbacks
 * @param hs connection state to free
 */
static err_t /*? me.interface.name ?*/_close_conn(struct tcp_pcb *pcb, struct http_state *hs)
{
    return /*? me.interface.name ?*/_close_or_abort_conn(pcb, hs, 0);
}

/* End of file: either close the connection (Connection: close)
 * or close the file (Connection: keep-alive)
 */
static void /*? me.interface.name ?*/_eof(struct tcp_pcb *pcb, struct http_state *hs)
{
    /*? me.interface.name ?*/_close_conn(pcb, hs);
}

/*!
 * @brief Get the file struct for a 404 error page.
 * Tries some file names and returns NULL if none found.
 *
 * @param uri pointer that receives the actual file name URI
 * @return file struct for the error page or NULL no matching file was found
 */
static struct fs_file */*? me.interface.name ?*/_get_404_file(struct http_state *hs, const char **uri)
{
    err_t err;

    *uri = "/404.html";
    err = fs_open(*uri);

    if (err != ERR_OK)  {
        *uri = NULL;
        return NULL;
    }

    memcpy((void *)&hs->file_handle, (void *)fs_mem, sizeof(fs_file_t));
    return &hs->file_handle;
}

/*!
 * @brief Extract URI parameters from the parameter-part of an URI in the form
 * "test.cgi?x=y" @todo: better explanation!
 * Pointers to the parameters are stored in hs->param_vals.
 *
 * @param hs http connection state
 * @param params pointer to the NULL-terminated parameter string from the URI
 * @return number of parameters extracted
 */
static int /*? me.interface.name ?*/_extract_uri_parameters(struct http_state *hs, char *params)
{
    char *pair, *equals;
    int loop;

    /* If we have no parameters at all, return immediately. */
    if (!params || (params[0] == '\0')) {
        return 0;
    }

    pair = params;

    /*  Parse up to LWIP_HTTPD_MAX_CGI_PARAMETERS from the passed string
     *  and ignore the remainder (if any)
     */
    for (loop = 0; (loop < LWIP_HTTPD_MAX_CGI_PARAMETERS) && pair; loop++) {

        hs->params[loop] = pair;
        equals = pair;

        /* Find the start of the next name=value pair and replace the delimiter
         * with a 0 to terminate the previous pair string.
         */
        pair = strchr(pair, '&');
        if (pair) {
            *pair = '\0';
            pair++;
        }
        else {
            pair = strchr(equals, ' ');
            if (pair) {
                *pair = '\0';
            }

            /* Revert to NULL so that we exit the loop as expected. */
            pair = NULL;
        }
        equals = strchr(equals, '=');
        if (equals) {
            *equals = '\0';
            hs->param_vals[loop] = equals + 1;
        }
        else {
            hs->param_vals[loop] = NULL;
        }
    }
    return loop;
}

/*!
 * When data has been received in the correct state, try to parse it
 * as a HTTP request.
 *
 * @param p the received pbuf
 * @param hs the connection state
 * @param pcb the tcp_pcb which received this packet
 * @return ERR_OK if request was OK and hs has been initialized correctly
 *         ERR_INPROGRESS if request was OK so far but not fully received
 *         another err_t otherwise
 */
static err_t /*? me.interface.name ?*/_parse_request(struct pbuf **inp, struct http_state *hs, struct tcp_pcb *pcb)
{
    char *data, *crlf;
    u16_t data_len, clen;
    struct pbuf *p = *inp;

    LWIP_UNUSED_ARG(pcb); /* only used for post */
    LWIP_ASSERT("p != NULL", p != NULL);
    LWIP_ASSERT("hs != NULL", hs != NULL);

    if ((hs->handle != NULL) || (hs->file != NULL)) {
        LWIP_DEBUGF(HTTPD_DEBUG, ("Received data while sending a file\r\n"));
        return ERR_USE;
    }

    LWIP_DEBUGF(HTTPD_DEBUG, ("Received %" U16_F " bytes\r\n", p->tot_len));

    if (hs->req == NULL) {
        LWIP_DEBUGF(HTTPD_DEBUG, ("First pbuf\r\n"));
        hs->req = p;
    }
    else {
        LWIP_DEBUGF(HTTPD_DEBUG, ("pbuf enqueued\r\n"));
        pbuf_cat(hs->req, p);
    }

    if (hs->req->next != NULL) {
        data_len = LWIP_MIN(hs->req->tot_len, LWIP_HTTPD_MAX_REQ_LENGTH);
        pbuf_copy_partial(hs->req, httpd_req_buf, data_len, 0);
        data = httpd_req_buf;
    }
    else {
        data = (char *)p->payload;
        data_len = p->len;
        if (p->len != p->tot_len) {
            LWIP_DEBUGF(HTTPD_DEBUG, ("Warning: incomplete header due to chained pbufs\r\n"));
        }
    }

    /* received enough data for minimal request? */
    if (data_len >= MIN_REQ_LEN) {
        /* wait for CRLF before parsing anything */
        crlf = strnstr(data, CRLF, data_len);
        if (crlf != NULL) {

            int is_09 = 0;
            char *sp1, *sp2;
            u16_t left_len, uri_len;
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("CRLF received, parsing request\r\n"));

            if (!strncmp(data, "GET ", 4)) {
                sp1 = data + 3;
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Received GET request\"\r\n"));
            }
            else {
                /* null-terminate the METHOD (pbuf is freed anyway wen returning) */
                data[4] = 0;
                /* unsupported method! */
                LWIP_DEBUGF(HTTPD_DEBUG, ("Unsupported request method (not implemented): \"%s\"\r\n", data));
                return /*? me.interface.name ?*/_find_error_file(hs, 501);
            }
            /* if we come here, method is OK, parse URI */
            left_len = data_len - ((sp1 + 1) - data);
            sp2 = strnstr(sp1 + 1, " ", left_len);
            if (sp2 == NULL) {
                /* HTTP 0.9: respond with correct protocol version */
                sp2 = strnstr(sp1 + 1, CRLF, left_len);
                is_09 = 1;
            }
            uri_len = sp2 - (sp1 + 1);
            if ((sp2 != 0) && (sp2 > sp1)) {
                /* wait for CRLFCRLF (indicating end of HTTP headers) before parsing anything */
                if (strnstr(data, CRLF CRLF, data_len) != NULL) {
                    char *uri = sp1 + 1;
                    /* null-terminate the METHOD (pbuf is freed anyway wen returning) */
                    *sp1 = 0;
                    uri[uri_len] = 0;
                    LWIP_DEBUGF(HTTPD_DEBUG, ("Received \"%s\" request for URI: \"%s\"\r\n", data, uri));
                    return /*? me.interface.name ?*/_find_file(hs, uri, is_09);
                }
            }
            else {
                LWIP_DEBUGF(HTTPD_DEBUG, ("invalid URI\r\n"));
            }
        }
    }

    clen = pbuf_clen(hs->req);
    if ((hs->req->tot_len <= LWIP_HTTPD_REQ_BUFSIZE) && (clen <= LWIP_HTTPD_REQ_QUEUELEN)) {
        /* request not fully received (too short or CRLF is missing) */
        return ERR_INPROGRESS;
    }
    else {
        LWIP_DEBUGF(HTTPD_DEBUG, ("bad request\r\n"));
        return /*? me.interface.name ?*/_find_error_file(hs, 400);
    }
}

/*!
 * @brief Sub-function of http_send(): end-of-file (or block) is reached,
 * either close the file or read the next block (if supported).
 *
 * @returns: 0 if the file is finished or no data has been read
 *           1 if the file is not finished and data has been read
 */
static u8_t /*? me.interface.name ?*/_check_eof(struct tcp_pcb *pcb, struct http_state *hs)
{
    /*? me.interface.name ?*/_eof(pcb, hs);
    return 0;
}

/*!
 * @brief Sub-function of http_send(): This is the normal send-routine for non-ssi files
 *
 * @returns: - 1: data has been written (so call tcp_ouput)
 *           - 0: no data has been written (no need to call tcp_output)
 */
static u8_t /*? me.interface.name ?*/_send_data_nonssi(struct tcp_pcb *pcb, struct http_state *hs)
{
    err_t err;
    u16_t len;
    u16_t mss;
    u8_t data_to_send = 0;

    /* We are not processing an SHTML file so no tag checking is necessary. */
    /* Just send the data as we received it from the file. */

    /* We cannot send more data than space available in the send buffer. */
    if (tcp_sndbuf(pcb) < hs->left) {
        len = tcp_sndbuf(pcb);
    }
    else {
        len = (u16_t)hs->left;
        LWIP_ASSERT("hs->left did not fit into u16_t!", (len == hs->left));
    }
    mss = tcp_mss(pcb);
    if (len > (2 * mss)) {
        len = 2 * mss;
    }

    err = /*? me.interface.name ?*/_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
    if (err == ERR_OK) {
        data_to_send = 1;
        hs->file += len;
        hs->left -= len;
    }

    return data_to_send;
}

/*!
 * @brief Try to send more data on this pcb.
 *
 * @param pcb the pcb to send data
 * @param hs connection state
 */
static u8_t /*? me.interface.name ?*/_send(struct tcp_pcb *pcb, struct http_state *hs)
{
    u8_t data_to_send = HTTP_NO_DATA_TO_SEND;

    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE,
                ("http_send: pcb=%p hs=%p left=%d\r\n", (void *)pcb, (void *)hs, hs != NULL ? (int)hs->left : 0));

    /* If we were passed a NULL state structure pointer, ignore the call. */
    if (hs == NULL) {
        return 0;
    }

    /* Have we run out of file data to send? If so, we need to read the next block from the file. */
    if (hs->left == 0) {
        if (!/*? me.interface.name ?*/_check_eof(pcb, hs)) {
            return 0;
        }
    }

    data_to_send = /*? me.interface.name ?*/_send_data_nonssi(pcb, hs);

    if (hs->left == 0) {
        /* We reached the end of the file so this request is done. */
        /* This adds the FIN flag right into the last data segment. */
        LWIP_DEBUGF(HTTPD_DEBUG, ("End of file.\r\n"));
        /*? me.interface.name ?*/_eof(pcb, hs);
        return 0;
    }
    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("send_data end.\r\n"));
    return data_to_send;
}

/*!
 * @brief Try to find the file specified by uri and, if found, initialize hs
 * accordingly.
 *
 * @param hs the connection state
 * @param uri the HTTP header URI
 * @param is_09 1 if the request is HTTP/0.9 (no HTTP headers in response)
 * @return ERR_OK if file was found and hs has been initialized correctly
 *         another err_t otherwise
 */
static err_t /*? me.interface.name ?*/_find_file(struct http_state *hs, const char *uri, int is_09)
{
    size_t loop;
    fs_file_t *file = NULL;
    char *params;
    err_t err;

    int i, count;

    const u8_t tag_check = 0;

    /* Have we been asked for the default root file? */
    if ((uri[0] == '/') && (uri[1] == 0)) {

        /* Try each of the configured default filenames until we find one that exists. */
        for (loop = 0; loop < NUM_DEFAULT_FILENAMES; loop++) {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Looking for %s...\r\n", g_psDefaultFilenames[loop].name));
            uri = (char *)g_psDefaultFilenames[loop].name;
            err = fs_open(uri);

            if (err == ERR_OK) {
                memcpy((void *)&hs->file_handle, (void *)fs_mem, sizeof(fs_file_t));
                file = &hs->file_handle;
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Opened.\r\n"));
                break;
            }
        }
        if (file == NULL) {
            /* None of the default filenames exist so send back a 404 page */
            file = /*? me.interface.name ?*/_get_404_file(hs, &uri);
        }
    }
    else  {
        /* No - we've been asked for a specific file. */
        /* First, isolate the base URI (without any parameters) */
        params = (char *)strchr(uri, '?');
        if (params != NULL) {
            /* URI contains parameters. NULL-terminate the base URI */
            *params = '\0';
            params++;
        }

        /* Does the base URI we have isolated correspond to a CGI handler? */
        if (g_iNumCGIs && g_pCGIs) {
            for (i = 0; i < g_iNumCGIs; i++) {
                if (strcmp(uri, g_pCGIs[i].pcCGIName) == 0) {
                    /* We found a CGI that handles this URI so extract the */
                    /* parameters and call the handler. */
                    count = /*? me.interface.name ?*/_extract_uri_parameters(hs, params);
                    uri = g_pCGIs[i].pfnCGIHandler(i, count, hs->params, hs->param_vals);
                    break;
                }
            }
        }

        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Opening %s\r\n", uri));
        err = fs_open(uri);

        if (err == ERR_OK) {
            memcpy((void *)&hs->file_handle, (void *)fs_mem, sizeof(fs_file_t));
            file = &hs->file_handle;
        }
        else {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Couldn't Find %s\r\n", uri));
            file = /*? me.interface.name ?*/_get_404_file(hs, &uri);
        }
    }
    return /*? me.interface.name ?*/_init_file(hs, file, is_09, uri, tag_check);
}

/*!
 * @brief Initialize a http connection with a file to send (if found).
 * Called by http_find_file and http_find_error_file.
 *
 * @param hs http connection state
 * @param file file structure to send (or NULL if not found)
 * @param is_09 1 if the request is HTTP/0.9 (no HTTP headers in response)
 * @param uri the HTTP header URI
 * @param tag_check enable SSI tag checking
 * @return ERR_OK if file was found and hs has been initialized correctly
 *         another err_t otherwise
 */
static err_t /*? me.interface.name ?*/_init_file(struct http_state *hs, struct fs_file *file, int is_09, const char *uri, u8_t tag_check)
{
    if (file != NULL) {

        /* file opened, initialise struct http_state */
        LWIP_UNUSED_ARG(tag_check);
        hs->handle = file;
        hs->file = (char *)file->data;
        LWIP_ASSERT("File length must be positive!", (file->len >= 0));
        hs->left = file->len;
        hs->retries = 0;

        LWIP_ASSERT("HTTP headers not included in file system", hs->handle->http_header_included);
        if (hs->handle->http_header_included && is_09) {
            /* HTTP/0.9 responses are sent without HTTP header, search for the end of the header. */
            char *file_start = strnstr(hs->file, CRLF CRLF, hs->left);
            if (file_start != NULL) {
                size_t diff = file_start + 4 - hs->file;
                hs->file += diff;
                hs->left -= (u32_t)diff;
            }
        }
    }
    else {
        hs->handle = NULL;
        hs->file = NULL;
        hs->left = 0;
        hs->retries = 0;
    }
    LWIP_UNUSED_ARG(uri);

    return ERR_OK;
}

/*
 * The pcb had an error and is already deallocated.
 * The argument might still be valid (if != NULL).
 */
static void /*? me.interface.name ?*/_err(void *arg, err_t err)
{
    struct http_state *hs = (struct http_state *)arg;
    LWIP_UNUSED_ARG(err);

    LWIP_DEBUGF(HTTPD_DEBUG, ("http_err: %s", lwip_strerr(err)));

    if (hs != NULL) {
        /*? me.interface.name ?*/_state_free(hs);
    }
}

/*
 * Data has been sent and acknowledged by the remote host.
 * This means that more data can be sent.
 */
static err_t /*? me.interface.name ?*/_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;

    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_sent %p\r\n", (void *)pcb));

    LWIP_UNUSED_ARG(len);

    if (hs == NULL)
    {
        return ERR_OK;
    }

    hs->retries = 0;

    /*? me.interface.name ?*/_send(pcb, hs);

    return ERR_OK;
}

/*
 * The poll function is called every 2nd second.
 * If there has been no data sent (which resets the retries) in 8 seconds, close.
 * If the last portion of a file has not been sent in 2 seconds, close.
 *
 * This could be increased, but we don't want to waste resources for bad connections.
 */
static err_t /*? me.interface.name ?*/_poll(void *arg, struct tcp_pcb *pcb)
{
    struct http_state *hs = (struct http_state *)arg;
    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE,
                ("http_poll: pcb=%p hs=%p pcb_state=%s\r\n", (void *)pcb, (void *)hs, tcp_debug_state_str(pcb->state)));

    if (hs == NULL)
    {
        err_t closed;
        /* arg is null, close. */
        LWIP_DEBUGF(HTTPD_DEBUG, ("http_poll: arg is NULL, close\r\n"));
        closed = /*? me.interface.name ?*/_close_conn(pcb, NULL);
        LWIP_UNUSED_ARG(closed);
        return ERR_OK;
    }
    else
    {
        hs->retries++;
        if (hs->retries == HTTPD_MAX_RETRIES)
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("http_poll: too many retries, close\r\n"));
            /*? me.interface.name ?*/_close_conn(pcb, hs);
            return ERR_OK;
        }

        /* If this connection has a file open, try to send some more data. If */
        /* it has not yet received a GET request, don't do this since it will */
        /* cause the connection to close immediately. */
        if (hs && (hs->handle))
        {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_poll: try to send more data\r\n"));
            if (/*? me.interface.name ?*/_send(pcb, hs))
            {
                /* If we wrote anything to be sent, go ahead and send it now. */
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("tcp_output\r\n"));
                tcp_output(pcb);
            }
        }
    }
    return ERR_OK;
}

/*
 * Data has been received on this pcb.
 * For HTTP 1.0, this should normally only happen once (if the request fits in one packet).
 */
static err_t /*? me.interface.name ?*/_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    err_t parsed = ERR_ABRT;
    struct http_state *hs = (struct http_state *)arg;
    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE,
                ("http_recv: pcb=%p pbuf=%p err=%s\r\n", (void *)pcb, (void *)p, lwip_strerr(err)));

    if ((err != ERR_OK) || (p == NULL) || (hs == NULL)) {
        if (p != NULL) {
            tcp_recved(pcb, p->tot_len);
            pbuf_free(p);
        }
        if (hs == NULL) {
            LWIP_DEBUGF(HTTPD_DEBUG, ("Error, http_recv: hs is NULL, close\r\n"));
        }
        /*? me.interface.name ?*/_close_conn(pcb, hs);
        return ERR_OK;
    }

    tcp_recved(pcb, p->tot_len);

    if (hs->handle == NULL) {
        parsed = /*? me.interface.name ?*/_parse_request(&p, hs, pcb);
        LWIP_ASSERT("http_parse_request: unexpected return value",
                    parsed == ERR_OK || parsed == ERR_INPROGRESS || parsed == ERR_ARG || parsed == ERR_USE);
    }
    else {
        LWIP_DEBUGF(HTTPD_DEBUG, ("http_recv: already sending data\r\n"));
    }

    if (parsed != ERR_INPROGRESS) {
        /* request fully parsed or error*/
        if (hs->req != NULL) {
            pbuf_free(hs->req);
            hs->req = NULL;
        }
    }

    if (parsed == ERR_OK) {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE,
                        ("http_recv: data %p len %" S32_F "\r\n", hs->file, hs->left));
        /*? me.interface.name ?*/_send(pcb, hs);
    }
    else if (parsed == ERR_ARG) {
        /*? me.interface.name ?*/_close_conn(pcb, hs);
    }

    return ERR_OK;
}

/*
 * A new incoming connection has been accepted.
 */
static err_t /*? me.interface.name ?*/_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    struct http_state *hs;
    struct tcp_pcb_listen *lpcb = (struct tcp_pcb_listen *)arg;
    LWIP_UNUSED_ARG(err);
    LWIP_DEBUGF(HTTPD_DEBUG, ("http_accept %p / %p \r\n", (void *)pcb, arg));

    tcp_accepted(lpcb);                   /* Decrease the listen backlog counter */
    tcp_setprio(pcb, HTTPD_TCP_PRIO);     /* Set priority */

    /* Allocate memory for the structure that holds the state of the
     * connection - initialized by that function.
     */
    hs = /*? me.interface.name ?*/_state_alloc();
    if (hs == NULL) {
        LWIP_DEBUGF(HTTPD_DEBUG, ("http_accept: Out of memory, RST\r\n"));
        return ERR_MEM;
    }
    hs->pcb = pcb;

    /* Set up the various callback functions and structures */
    tcp_arg(pcb, hs);

    tcp_recv(pcb, /*? me.interface.name ?*/_recv);
    tcp_err(pcb,  /*? me.interface.name ?*/_err);
    tcp_poll(pcb, /*? me.interface.name ?*/_poll, HTTPD_POLL_INTERVAL);
    tcp_sent(pcb, /*? me.interface.name ?*/_sent);

    return ERR_OK;
}

/*
 * Initialize the httpd with the specified local address.
 */
static void /*? me.interface.name ?*/_init_addr(ip_addr_t *local_addr)
{
    struct tcp_pcb *pcb;
    err_t err;

    pcb = tcp_new();
    LWIP_ASSERT("httpd_init: tcp_new failed", pcb != NULL);
    tcp_setprio(pcb, HTTPD_TCP_PRIO);
    err = tcp_bind(pcb, local_addr, HTTPD_SERVER_PORT);
    LWIP_ASSERT("httpd_init: tcp_bind failed", err == ERR_OK);
    pcb = tcp_listen(pcb);
    LWIP_ASSERT("httpd_init: tcp_listen failed", pcb != NULL);

    /* initialize callback arg and accept callback */
    tcp_arg(pcb, pcb);
    tcp_accept(pcb, /*? me.interface.name ?*/_accept);
}

void /*? me.interface.name ?*/__init(void)
{
    int err UNUSED;
    err = lwip_lock();
    LWIP_DEBUGF(HTTPD_DEBUG, ("httpd_init\r\n"));

    /*? me.interface.name ?*/_get_cgi();
    /*? me.interface.name ?*/_init_addr(IP_ADDR_ANY);
    err = lwip_unlock();
}
