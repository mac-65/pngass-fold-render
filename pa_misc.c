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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>  /* getopt() */
#include <libgen.h>

#include "pa_misc.h"

#undef free
#define free(x) { (free)((void *) x), (x) = NULL; }


/**
 *******************************************************************************
 */
rc_e O3 is_a_directory(char const *const pathname, int mode)
{
    struct {
        rc_e  rc;
    } w = {
        .rc = RC_FALSE,
    };
    struct stat st;

    if (   0 == stat(pathname, &st)
        && S_ISDIR(st.st_mode)
        && 0 == access(pathname, R_OK | mode)
       ) {

        w.rc = RC_TRUE;
    }

    return ( w.rc );
}


/**
 *******************************************************************************
 * Build a "newpath" from an existing pathname using dirname as the path.
 *
 * The caller must free the string returned in 'newpath'.
 */
rc_e O3 new_from_filename(char **newpath, char const *const filename, char const *const dirname)
{
    struct {
        int   len;
        char *newpath;
        char *base_name;
        rc_e  rc;
    } w = {
        .rc      = RC_FALSE,
        .newpath = NULL,
    };
    struct stat st;


    if ( NULL != newpath ) {
        char name[ 512 ];
        w.len = snprintf(name, sizeof (name), "%s", filename);
        if ( w.len < (int) sizeof (name) ) {
            w.base_name = basename(name);
            w.len = strlen(dirname);
            if ( PA_PATH_SEP[0] == dirname[w.len - 1] ) {
                --w.len;
            }

            asprintf(&w.newpath, "%.*s" PA_PATH_SEP "%s", w.len, dirname, w.base_name);

            if (   0 == stat(w.newpath, &st)
                && S_ISREG(st.st_mode)
                && 0 == access(w.newpath, R_OK)) {

                w.rc = RC_TRUE;
            }
        }
        *newpath = w.newpath;
    }

    return ( w.rc );
}


/**
 *******************************************************************************
 * Zap leading isspace() from the argument, returns a pointer to 1st non-SPACE.
 */
char const * O3 zap_isspace(char const *arg)
{
    char const *str = arg;

    if ( NULL == str ) return ("");

    while ( isspace(*str) ) str++;

    return ( str );
}


/**
 *******************************************************************************
 * Build the Page X of Y string and return a free()-able pointer to it.
 */
char const * O3 build_page_X_of_Y(size_t x, size_t y, xy_format_e xy_format)
{
    char *ptr;

    switch ( xy_format ) {
    case XY_PAGE_XofY:
        asprintf(&ptr, "Page %lu / %lu", x, y);
        break;
    case XY_DECIMAL:
        asprintf(&ptr, "%lu / %lu", x, y);
        break;
    case XY_ROMAN: {
        char buf[ ROMAN_STR_SIZE ];
        char const *p1;

        asprintf(&ptr, "%s / %s", val_to_roman(x, buf), p1 = val_to_roman(y, NULL));
        (free)( (void *) p1 );
    } break;
    default:
        ptr = strdup("");
    }

    return ( ptr );
}


/**
 *******************************************************************************
 * Return a string containing the roman numeral equivalent of the non-zero
 * positive argument.
 *
 * Adapted from Edam Lee's solution:
 * https://stackoverflow.com/questions/23269143/c-program-that-converts-numbers-to-roman-numerals
 */
char const * O3 val_to_roman(size_t val, char /* ROMAN_STR_SIZE */ *ptr)
{
    static struct {
        unsigned short value;
        char const     digits[ 3 ];
    } const romanTable[] = {
            { 1000, "M"  },
            { 900,  "CM" },
            { 500,  "D"  },
            { 400,  "CD" },
            { 100,  "C"  },
            { 90,   "XC" },
            { 50,   "L"  },
            { 40,   "XL" },
            { 10,   "X"  },
            { 9,    "IX" },
            { 5,    "V"  },
            { 4,    "IV" },
            { 1,    "I"  }
    };
    struct {
        unsigned short val;
        unsigned short len;
        unsigned short idx;
        char           buf[ ROMAN_STR_SIZE ];
        char          *p;
    } w = {
        .val = val,
        .len = 0,
        .idx = 0,
    };

    w.p = w.buf;

    if ( w.val <= romanTable[ 0 ].value )
    while ( w.val ) {
        while ( w.val <  romanTable[ w.idx ].value )
            w.idx++;
        while ( w.val >= romanTable[ w.idx ].value ) {
            strncpy(&w.p[ w.len ], romanTable[ w.idx ].digits, (sizeof (w.buf) - 1) - w.len);
            w.len += strlen(romanTable[ w.idx ].digits);
            w.val -= romanTable[ w.idx ].value;
        }
    }
    else w.p = "Error XLII";

    if ( NULL != ptr ) strcpy(ptr, w.p);
    else         ptr = strdup(w.p);

    return ( ptr );
}


/*******************************************************************************
 */
// #pragma GCC diagnostic ignored "-Wunused-function"
void break_me(char *str)
{
    if (1) fprintf(stderr, "%s\n", str);
}

