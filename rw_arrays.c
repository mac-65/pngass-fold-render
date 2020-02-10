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

/**
 *******************************************************************************
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>  /* getopt() */
#include <glob.h>

#include "pa_misc.h"
#include "rw_arrays.h"
#include "rw_textfile.h"

#ifndef realloc
#warning "realloc() is not a macro..."
#endif

static char const *append_strptrary_data(strptrary_t *const ptrs, char const *const data);


/**
 *******************************************************************************
 *
 * Returns:  0 - if there were no matching file from the pattern;
 *           # - the number of paths matching the pattern; and
 *          -1 - any other 'glob()' error.
 */
int O3 add_glob_files(strptrary_t *const ptr, char const *const pattern, int sort)
{
    struct {
        int     idx;
        int     flags;
        int     rc;
        char   *pathname;
    } w = {
        .idx   = 0,
        .flags = (sort ? 0 : GLOB_NOSORT),
    };
    glob_t globbuf = { .gl_offs = 0, };

    w.rc = glob(pattern, w.flags | GLOB_DOOFFS, NULL, &globbuf);

    if ( 0 == w.rc )
    for ( ; w.idx < (int) globbuf.gl_pathc; w.idx++ ) {
        w.pathname = globbuf.gl_pathv[ w.idx ];
        add_strptrary( ptr, w.pathname );
    }
    else if ( GLOB_NOMATCH != w.rc ) {
        w.idx = -1;  /* We're not going to qualify the actual glob() error */
    }

    globfree(&globbuf);

    return ( w.idx );
}


/**
 *******************************************************************************
 * Add a list of input text files read from a file.
 *
 * The files listed in the 'filename' are unadorned; not quoted or escaped and
 * should be UTF-8 encoding.  We only load the array, we don't 'access()' them.
 *
 * A comment can appear in this file.  Any line beginning with '#' is ignored.
 *
 * Returns:  0 - if there were no matching file from the pattern;
 *           # - the number of paths matching the pattern; and
 *          -1 - ERROR :: 'fopen()' or 'wget() failure, refer to errno.
 * Bugs: An error will not unwind any pathnames that may have been added.
 *
 * TODO :: needs some testing.
 */
int O0 add_file_list(strptrary_t *const ptr, char const *const filename)
{
    struct {
        FILE   *file;
        char   *p;
        short   cnt;
        short   lineno;
    } w = {
        .cnt   = -1,
    };
    char pathname[ PATH_MAX + 1 ];  /* PATH_MAX :: not perfect, but s/b okay */


    w.file = fopen(filename, "r");

    if ( NULL != w.file ) {
        w.cnt++;
        while ( NULL != fgets( pathname, sizeof ( pathname ), w.file ) ) {

            w.lineno++;
            w.p = pathname;
            while ( isblank(*w.p) ) w.p++;
            if ( '#' == *w.p )
                continue;

            size_t len = strlen(w.p);
            if ( len > 1 ) {
                if ( '\n' == w.p[ --len ] ) w.p[ len ] = '\0';
                if ( RC_FALSE == access_a_pathname( ptr, w.p ) ) {
                        fprintf(stderr, "FATAL :: I/O ERROR for '%s'\n"
                                        "      in '%s':%d\n", w.p, filename, w.lineno);
                        _exit(3);
                }
                w.cnt++;
            }
            else { /* We'll silently IGNORE empty lines */ }
        }

        if ( ferror( w.file ) ) {
            /* TODO error reporting */
            w.cnt = -1;
        }
        fclose( w.file );
    }

    return ( w.cnt );
}


/**
 *******************************************************************************
 *
 * Note, errors are managed by the caller.
 */
char const *add_single_template(strptrary_t *const strptrary, char const *const pathname)
{

    cleanup_strptrary( strptrary );   /* Last one always wins. */
    if ( RC_FALSE == access_a_pathname( strptrary, pathname ) ) {
        return ( NULL );
    }

    char *s1 = read_textfile_sz( pathname, NULL, 1280 );  // FIXME :: NULL return?
    char const *s2 = append_strptrary_data( strptrary, s1 );
    (free)(s1);

    return ( s2 );
}


/**
 *******************************************************************************
 *
 * Note, errors are managed by the caller.
 */
rc_e O3 access_a_pathname(strptrary_t *const ptrs, char const *const pathname)
{
    struct {
        rc_e rc;
    } w = {
        .rc = RC_FALSE,
    };

    if ( 0 == access(pathname, F_OK | R_OK) ) {
        add_strptrary( ptrs, pathname );
        w.rc = RC_TRUE;
    }

    return ( w.rc );
}


/**
 *******************************************************************************
 *
 * FIXME :: Always returns 'RC_TRUE'.
 */
rc_e O3 append_a_pathname(strptrary_t *const ptrs, char const *const pathname)
{
    struct {
        rc_e rc;
    } w = {
        .rc = RC_TRUE,
    };

    add_strptrary( ptrs, pathname );

    return ( w.rc );
}


/**
 *******************************************************************************
 */
static char const * O3 append_strptrary_data(strptrary_t *const ptrs, char const *data)
{
    unsigned int idx = ptrs->cnt;

    if ( 0 == idx ) {
        fprintf(stderr, "FATAL PROGRAMMING ERROR :: %s::%d\n", __func__, __LINE__);
        _exit( 3 );
    }
    if ( NULL == data )
        return ( NULL );

    return ( ptrs->data[ --idx ] = strdup(data) );
}


/**
 *******************************************************************************
 */
size_t add_strptrary(strptrary_t *const ptrs, char const *const pathname)
{
    unsigned int idx = ptrs->cnt;

    ptrs->cnt++;
    ptrs->pathnames = realloc(ptrs->pathnames, ptrs->cnt * sizeof (char *));
    ptrs->pathnames[ idx ] = strdup(pathname);

    ptrs->data = realloc(ptrs->data, ptrs->cnt * sizeof (char *));
    ptrs->data[ idx ] = NULL;

    return ( ptrs->cnt );
}


/**
 *******************************************************************************
 */
void cleanup_strptrary(strptrary_t *ptrs)
{

    if ( NULL != ptrs ) {
        for ( unsigned int idx = 0; idx < ptrs->cnt; idx++ ) {
            (free)( (void *) ptrs->pathnames[ idx ] );
            (free)( (void *) ptrs->data[ idx ] );
        }
        free ( ptrs->pathnames );
        free ( ptrs->data );
        ptrs->cnt = 0;
    }

    return ;
}


/**
 *******************************************************************************
 *
 * FIXME :: works, but interface doesn't make sense
 */
size_t add_comments(comments_t *ptr, char const *const key, char const *const val)
{
    unsigned int idx = ptr->cnt;

    ptr->cnt++;
    ptr->kvs = realloc(ptr->kvs, ptr->cnt * sizeof (kv_t));
    ptr->kvs[ idx ].key = strdup(key);
    ptr->kvs[ idx ].val = strdup(val);

    return ( ptr->cnt );
}


/**
 *******************************************************************************
 *
 * Note, errors are managed by the caller.
 */
comments_t *dup_comments(comments_t *src)
{
    struct {
        comments_t *comments;
        size_t      idx;
    } w = {
        .comments = NULL,
        .idx = 0,
    };

    if ( NULL != src ) {
        w.comments = calloc(1, sizeof (comments_t));
        for ( ; w.idx < src->cnt; w.idx++ ) {
            add_comments( w.comments, src->kvs[ w.idx ].key, src->kvs[ w.idx ].val );
        }
    }

    return ( w.comments );
}


/**
 *******************************************************************************
 */
void cleanup_comments(comments_t **ptr)
{

    if ( NULL != ptr ) {
        comments_t *comments = *ptr;

        if ( NULL != comments ) {
            for ( size_t idx = 0; idx < comments->cnt; idx++ ) {
                (free)( (void *) comments->kvs[ idx ].key );
                (free)( (void *) comments->kvs[ idx ].val );
            }
            free (comments->kvs);
        }
        free (*ptr);
    }

    return ;
}

