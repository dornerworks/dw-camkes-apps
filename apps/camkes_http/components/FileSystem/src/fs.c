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

#include <lwip/opt.h>
#include <lwip/def.h>
#include <lwip/err.h>
#include <fs.h>
#include <fsdata.h>
#include <string.h>

u8_t fs_open(const char *name)
{
    const struct fsdata_file *f;

    if (name == NULL) {
        return ERR_ARG;
    }

    for (f = FS_ROOT; f != NULL; f = f->next) {
        if (!strcmp(name, (char *)f->name)) {
            int old_file_length = fs_mem->len;
            int new_file_length = f->len;

            /* Clear the last chunk of data */
            if(old_file_length > new_file_length) {
                memset((void *)(fs_mem->data + new_file_length), 0, old_file_length - new_file_length);
            }
            memcpy((void *)fs_mem->data, (void *)f->data, f->len);
            fs_mem->len = f->len;
            fs_mem->index = f->len;
            fs_mem->http_header_included = f->http_header_included;
            return ERR_OK;
        }
    }
    return ERR_VAL; /* file not found */
}

u8_t fs_update(const char *name, int len, const char *data)
{
    struct fsdata_file *f;

    if ((data == NULL) || (name == NULL) || (len > MAX_FS_FILE_SIZE)) {
        return ERR_ARG;
    }

    for (f = FS_ROOT; f != NULL; f = f->next) {
        if (!strcmp(name, (char *)f->name)) {

            /* Clear the last chunk of data */
            if (f->len > len) {
                memset((void *)(f->data + len), 0, f->len - len);
            }
            /* If the new file is larger than the existing file, everything
             * gets overwritten anyway
             */
            memcpy((void *)f->data, data, len);
            f->len = len;
            return ERR_OK;
        }
    }
    return ERR_VAL; /* file not found */
}

void fs_close(void)
{
    memset((void *)fs_mem, 0, sizeof(struct fsdata_file));
}
