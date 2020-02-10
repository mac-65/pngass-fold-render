/*
 *******************************************************************************
 * Copyright (C) 2018
 *
 * This file is part of pngass.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include "pa_misc.h"
#include "rw_textfile.h"

#ifndef realloc
#warning "realloc() is not a macro..."
#endif

/**
 *******************************************************************************
 * Read an entire file into memory, return a pointer and (optionally) it's size.
 *
 * Although klunky, using 'stat()/fread()' would be just as klunky for 'stdin'.
 */
char * O3 read_textfile_sz(char const *const filename, size_t *len, size_t buf_size)
{
    struct {
        char   *buf;
        char   *str;
        size_t  idx;
        size_t  line_len;
        size_t  buf_size;
        int     close;

        FILE   *in_file;
    } w = {
        .buf_size = buf_size,
        .buf      = NULL,
        .in_file  = NULL,
        .close    = 0,
    };


    w.in_file = (strcmp(filename, READ_STDIN))
              ? (w.close = 1, fopen(filename, "r"))
              : stdin;

    if ( w.in_file != NULL && (NULL != (w.buf = malloc(w.buf_size))) )
    while ( NULL != (w.str = fgets( w.buf + w.idx, w.buf_size - w.idx, w.in_file )) ) {
        w.line_len = strlen( w.str );
        w.idx += w.line_len;

        if ( w.idx >= (w.buf_size - 1) ) {
            w.buf_size += (buf_size / 2);
            w.buf = realloc( w.buf, w.buf_size );
        }

        /* In this particular case, we don't care about feof(). */
    }

    if ( ferror( w.in_file ) ) {
        /* TODO error reporting */
        free( w.buf );
    }
    else {
        if ( len != NULL ) {
            *len = w.idx;
        }

        if ( w.close != 0 && w.in_file != NULL ) {
            fclose( w.in_file );
        }

        /* Shrink the returned memory, this should _always_ succeed. */
        w.buf = realloc( w.buf, w.idx + 1 );
    }

    return ( w.buf );
}


/**
 *******************************************************************************
 * Read a stream into memory, return a pointer and (optionally) it's size.
 *
 * Note, unlike 'read_textfile_sz()', the stream is NOT closed when its EOF is
 * reached.  This is because we don't know how the stream was opened (e.g. if
 * it was opened using 'popen()', then it must be closed using 'pclose()').
 */
char * O3 read_stream_sz(FILE *file, size_t *len, size_t buf_size)
{
    struct {
        char   *buf;
        char   *str;
        size_t  idx;
        size_t  line_len;
        size_t  buf_size;

        FILE   *in_file;
    } w = {
        .buf_size = buf_size,
        .buf      = NULL,
        .in_file  = NULL,
    };


    w.in_file = file;

    if ( w.in_file != NULL && (NULL != (w.buf = malloc(w.buf_size))) )
    while ( NULL != (w.str = fgets( w.buf + w.idx, w.buf_size - w.idx, w.in_file )) ) {
        w.line_len = strlen( w.str );
        w.idx += w.line_len;

        if ( w.idx >= (w.buf_size - 1) ) {
            w.buf_size += (buf_size / 2);
            w.buf = realloc( w.buf, w.buf_size );
        }
    }

    if ( ferror( w.in_file ) ) {
        /* TODO error reporting */
        free ( w.buf );
    }
    else {
        if ( len != NULL ) {
            *len = w.idx;
        }

        /* Shrink the returned memory, this should _always_ succeed. */
        w.buf = realloc( w.buf, w.idx + 1 );
    }

    return ( w.buf );
}

