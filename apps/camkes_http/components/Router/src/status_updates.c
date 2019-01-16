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

/* CGI - Common Gateway Interface
 *
 * In the __init function, a structure which contains the file name and the
 * callback are registered using the _set_cgi_handlers function.
 *
 * When the Server has been asked for a specific file, it will call the registered
 * callback functions, which allow the Webserver to perform a specific task before
 * returning the file name to grab in the FileSystem.
 */

#include <stdio.h>
#include <camkes.h>

#define SECS_IN_DAY    86400
#define SECS_IN_HOUR   3600
#define SECS_IN_MINUTE 60

unsigned int int_num_seconds;
unsigned char time_data[150];

typedef const char *(*tCGIHandler)(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);

/*!
 * Structure defining the base filename (URL) of a CGI and the associated
 * function which is to be called when that URL is requested.
 */
typedef struct
{
    const char *pcCGIName;
    tCGIHandler pfnCGIHandler;
} tCGI;

extern void http_server_set_cgi_handlers(const tCGI *pCGIs, int iNumHandlers);

void event_handle(void)
{
    unsigned int secs = ++int_num_seconds;

    int ret = snprintf((char *)time_data, 150,
                       "<span style=\"font-family: terminal, monaco, monospace; color:"
                       "#525252;\">%d Days, %d Hours, %d Minutes, & %d Seconds",
                       secs/SECS_IN_DAY,
                       (secs % SECS_IN_DAY)/SECS_IN_HOUR,
                       (secs % SECS_IN_HOUR)/SECS_IN_MINUTE,
                       secs % SECS_IN_MINUTE);

    fs_update((char *)"/timerdata.sts", ret, (char *)time_data);
}

/*!
 * @brief Print to the Terminal.
 */
char *cgi_print(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    static int cnt = 0;
    printf("Request #%d from WebServer to Output this string on UART\n", cnt++);
    return "/print.cgi";
}

void http_server_get_cgi(void)
{
    static tCGI cgis[1] = {
        { "/print.cgi", (tCGIHandler)cgi_print },
    };

    http_server_set_cgi_handlers(cgis, 1);
}
