#ifndef PA_MISC_H
#define PA_MISC_H
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

#undef O0
#undef O3
#undef PA_BUILT_OPTIMIZED
#ifdef __OPTIMIZE__
    #define O0 /* */
    #define O3 /* */
    #define PA_BUILT_OPTIMIZED (1)
#else
    #define O0 __attribute__((optimize("O0")))
    #define O3 __attribute__((optimize("O3")))
    #define PA_BUILT_OPTIMIZED (0)
#endif

#undef UNUSED_ARG
#define UNUSED_ARG __attribute__((unused))

#undef VOLATILE
#define VOLATILE  /* volatile */

#undef STR_MATCH
#define STR_MATCH (0)  /* readability for str*cmp() */

  /*
   *****************************************************************************
   * "Everybody" says this isn't a thing any more, but it's a good marker for
   * where it's used just in case...  Also, good luck w/PATH_MAX.
   */
#undef PA_PATH_SEP
#define PA_PATH_SEP "/"

typedef enum { RC_FALSE = 0, RC_TRUE = 1 } rc_e;

#define ROMAN_STR_SIZE (16)
typedef enum { XY_PAGE_XofY = 0, XY_DECIMAL, XY_ROMAN } xy_format_e;


/**
 ******************************************************************************
 * Memory management :: Simply quit if we're unable to allocate the memory.
 *
 * The 'DBG_REALLOC' flag is set in the Makefile.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#undef REALLOC_CHECK
#ifdef DBG_REALLOC
# define REALLOC_CHECK (11207)
#else
# define REALLOC_CHECK (0)
#endif

#undef pa_alloc
#define pa_alloc( p_in_, s_, WHAT_ ) ({           \
    size_t sz_ = (s_);                            \
    if ( REALLOC_CHECK ) {                        \
        /* Insert realloc() debug code here */    \
    }                                             \
    void  *p_ = (realloc)( (p_in_), sz_ );        \
    if ( NULL == p_ ) {                           \
        char error_buf[ 128 ];                    \
        int  sv_errno = errno;                    \
        snprintf(error_buf, sizeof (error_buf), "FATAL :: "  \
            WHAT_ "(%lu bytes) failed in %s:%s():%d, "       \
            "errno = %d", sz_, __FILE__, __func__, __LINE__, sv_errno);  \
        errno = sv_errno;                         \
        perror(error_buf);                        \
        abort();                                  \
    }                                             \
    p_;                                           \
})

#undef realloc   /* We check for this w/#ifndef in the '.c' sources */
#define realloc( p_in_, sz_ ) pa_alloc((p_in_), (sz_), "realloc")

#undef malloc
#define malloc( sz_ ) pa_alloc(NULL, (sz_), "malloc")

#undef calloc
#define calloc( nmemb_, sz_ ) ({                            \
     size_t size_ = (nmemb_) * (sz_);                       \
     memset(pa_alloc(NULL, size_, "calloc"), '\0', size_);  \
})

#undef free
#define free( p_ ) { void **x_ = (void *)&(p_); (free)(*x_); (*x_) = NULL; }

/**
 *******************************************************************************
 */
#undef BREAK_STR
#ifdef __OPTIMIZE__
  #define BREAK_STR(str_, p_ ) /* */
#else
  #define BREAK_STR(str_, p_   /* p_ should be a char CONST */ ) {  \
      char const *const s_ = (str_);                                \
      if ( (NULL != p_) && 0 == strncmp(s_, p_, strlen(p_)) ) {     \
          break_me(p_);                                             \
      }                                                             \
  }
#endif

/**
 *******************************************************************************
 */
rc_e  is_a_directory(char const *const path, int mode);
rc_e  new_from_filename(char **newpath, char const *const filename, char const *const dirname);
char const *zap_isspace(char const *arg);

char const *build_page_X_of_Y(size_t xy_x, size_t xy_y, xy_format_e);
char const *val_to_roman(size_t val, char *);

void break_me(char *str);

/*
 *******************************************************************************
 * https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html#Stringizing
 */
#define xstr(s) str(s)
#define str(s) #s

#endif   /* PA_MISC_H */

