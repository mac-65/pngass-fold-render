#ifndef RW_ARRAYS_H
#define RW_ARRAYS_H
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

#include "pa_misc.h"  /* for 'rc_e' */

typedef struct strptrary_t {
    char const  **pathnames;
    char const  **data;
    unsigned int  cnt;
} strptrary_t;

typedef struct kv_t {
    char const *key;
    char const *val;
} kv_t;

typedef struct comments_t {
    kv_t         *kvs;
    unsigned int  idx;
    unsigned int  cnt;
} comments_t;

rc_e         append_a_pathname  ( strptrary_t *const, char const *const pathname );
rc_e         access_a_pathname  ( strptrary_t *const, char const *const pathname );
char  const *add_single_template( strptrary_t *const, char const *const pathname );
size_t       add_strptrary      ( strptrary_t *const, char const *const pathname );
void         cleanup_strptrary  ( strptrary_t *const );

enum { PA_GLOB_RAW = 0, PA_GLOB_SORT = 1, };
int          add_glob_files     ( strptrary_t *const, char const *const pattern, int sort );
int          add_file_list      ( strptrary_t *const, char const *const filename );

size_t       add_comments    ( comments_t *const, char const *const key, char const *const val );
comments_t  *dup_comments    ( comments_t * );
void         cleanup_comments( comments_t **const );

#endif  /* RW_ARRAYS_H */

