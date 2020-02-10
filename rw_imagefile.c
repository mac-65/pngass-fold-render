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
 * Very simple support for reading PNG files and writing PNG and JPG files.
 * Probably has lots of "issues" and will need some cleanup down the road.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>
#include <ass/ass.h>
#include <png.h>

#include "pa_misc.h"
#include "rw_arrays.h"
#include "rw_imagefile.h"

#ifndef realloc
#warning "realloc() is not a macro..."
#endif

#if defined(ENABLE_JPEG_RW)
#  include <jpeglib.h>
#  include <setjmp.h>
#  define JPEG_ARGS_UNUSED /* */
#else
#  define JPEG_ARGS_UNUSED __attribute__((unused))
#endif


typedef struct pa_image_t {
    png_byte    *image_data;  // RGB24
    int          width;
    int          height;
    png_uint_32  row_bytes;
    png_byte     color_type;
    png_byte     bit_depth;
    int          number_of_passes;

    comments_t  *comments;      /* Derived from the 'png_text' record(s) */
    int          no_comments;   /* Controls the WRITING of image comments */

    int          jpeg_quality;  /* JPEG support, range 0..100 */
    char        *err_desc;
} pa_image_t;


static rc_e load_png_comments(png_struct *pngs_ptr, png_info *info_ptr, comments_t **, char const *const *keys);

static int save_pngfile(char const *const png_filename, pa_image_t *png_image);
static int save_jpgfile(char const *const jpg_filename, pa_image_t *png_image);

static pa_image_t *alloc_png_image();
static void prune_pa_image(pa_image_t **image);


/**
 ******************************************************************************
 */
#undef set_err_desc
#define set_err_desc( p_err, fmt, ...) {       \
    asprintf( &(p_err), fmt, ##__VA_ARGS__ );  \
}


/**
 ******************************************************************************
 * Read a PNG file into memory and return a 'pa_image_t' in 'imagep'.
 *
 * There seems to be NO way to read pieces of a PNG file without producing
 * some sort of warning from libpng.  For example, if you are only interested
 * in the comments from the PNG image, you have to read everything else.
 * Dunno if it's my (lack of) experience, but I wasn't able to find a clear
 * example of how to do something like this.
 *
 * \callgraph
 * \callergraph
 */
rc_e O0 read_png_image(char const *const filename, pa_image_t **imagep, char const *const *keys, read_opts_e read_opts)
{
#undef PA_PNG_HEADER_SZ
#define PA_PNG_HEADER_SZ (8)  /**< *png.h* does NOT define this as a
                                   constant, but mentions its value. */
    auto void cleanup_row_pointers( png_byte *** );  /* gcc, has to be here */
    auto void user_error_fn(png_struct *, png_const_charp);
    struct {
        FILE       *png_file;
        png_struct *pngs_ptr;
        png_info   *info_ptr;
        rc_e        rc;
    } VOLATILE wr = {
        .rc        = RC_FALSE,
        .png_file  = NULL,
        .pngs_ptr  = NULL,
    };
    pa_image_t *image = NULL;


    if ( NULL != (image = alloc_png_image()) )
    do {
        image->err_desc = NULL;

        if ( NULL == filename || '\0' == *filename ) {
            set_err_desc( image->err_desc, "filename is empty or not provided" );
            break;
        }

        /*
         **********************************************************************
         * Open the file and verify that it is a PNG image.
         *   strerror() :: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=46267
         */
        wr.png_file = fopen( filename, "rb" );
        if( NULL == wr.png_file ) {
            set_err_desc( image->err_desc, "'%s' -- error %d, %s",
                                           filename, errno, strerror(errno)); /*+*/
            break;
        }
        png_byte header[ PA_PNG_HEADER_SZ ];  /* 8 is the size to check */
        if ( sizeof( header ) != fread( header, 1, sizeof (header), wr.png_file )
                        || png_sig_cmp( header, 0, sizeof (header) ) ) {
            set_err_desc( image->err_desc, "'%s' -- is not a PNG file", filename ); /*+*/
            break;
        }

        /*
         **********************************************************************
         * Get the two pointers that PNG needs to work with the file.
         * Note, setting the function pointers does call the function, but the
         * library still writes a message to stderr (it seems).
         */
        wr.pngs_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, user_error_fn, NULL );
        if( NULL == wr.pngs_ptr ) {
            set_err_desc( image->err_desc, "'%s' -- png_create_read_struct() failed", filename );
            break;
        }

        wr.info_ptr = png_create_info_struct( wr.pngs_ptr );
        if( NULL == wr.info_ptr ) {
            set_err_desc( image->err_desc, "'%s' -- png_create_info_struct() failed", filename );
            break;
        }

        /*
         **********************************************************************
         */
        if (setjmp(png_jmpbuf(wr.pngs_ptr))) {
            set_err_desc( image->err_desc, "'%s' -- png_init_io() failed", filename );
            break;
        }

        /*
         **********************************************************************
         * Set the 'file' and how many bytes we've already consumed.
         */
        png_init_io(wr.pngs_ptr, wr.png_file);
        png_set_sig_bytes(wr.pngs_ptr, PA_PNG_HEADER_SZ);

        /*
         **********************************************************************
         * Get a bunch of stuff about the image we just opened.
         */
        png_read_info(wr.pngs_ptr, wr.info_ptr);

        image->width            = png_get_image_width(wr.pngs_ptr, wr.info_ptr);
        image->height           = png_get_image_height(wr.pngs_ptr, wr.info_ptr);
        image->bit_depth        = png_get_bit_depth(wr.pngs_ptr, wr.info_ptr);
        image->row_bytes        = png_get_rowbytes(wr.pngs_ptr, wr.info_ptr);
        image->number_of_passes = png_set_interlace_handling(wr.pngs_ptr);
        image->color_type       = png_get_color_type(wr.pngs_ptr, wr.info_ptr);

        if ( image->color_type != PNG_COLOR_TYPE_RGB ) {
            set_err_desc( image->err_desc, "'%s' -- unsupported PNG image (color_type = %d)", filename, image->color_type );
            goto load_complete;
        }
        if ( READ_ONLY_METADATA == read_opts ) {
            wr.rc = RC_TRUE;
            goto load_complete;
        }

        png_read_update_info(wr.pngs_ptr, wr.info_ptr);

        if ( 1 || READ_WHOLE_IMAGE == read_opts ) {  /* I wish ... */
            /*
             ******************************************************************
             * Now, setup to read the PNG file into memory.
             */
            image->image_data = malloc( image->height * image->row_bytes );
            if( NULL == image->image_data ) {
                set_err_desc( image->err_desc, "'%s' -- no memory for PNG image", filename );
                break;
            }

            png_byte **row_pointers __attribute__ ((__cleanup__ (cleanup_row_pointers)))
                                    = malloc(image->height * sizeof (png_byte *));

            for ( int yy = 0; yy < image->height; yy++ ) {
                row_pointers[ yy ] = image->image_data + (yy * image->row_bytes);
            }

            /*
             ******************************************************************
             * ...and read the file.
             */
            if ( setjmp(png_jmpbuf(wr.pngs_ptr))) {
                set_err_desc( image->err_desc, "'%s' -- png_read_image() failed", filename ); /*+*/
                break;
            }

            png_read_image(wr.pngs_ptr, row_pointers);
            png_read_end(wr.pngs_ptr, wr.info_ptr);
        }

        /*
         **********************************************************************
         * Omit the call to 'png_read_end()' (above) and the comment results
         * will be truncated to the 1st record of the call to 'png_get_text()'.
         * No docs -- Dunno why...
         */
        wr.rc = load_png_comments( wr.pngs_ptr, wr.info_ptr, &image->comments, keys );
        if ( RC_FALSE == wr.rc ) {
            set_err_desc( image->err_desc, "error building comments record" );
        }
    } while ( 0 );
    else {
        /* This *simply* marks the case where memory allocation has failed */
    }


    load_complete:
    if ( wr.png_file != NULL ) {
        fclose( wr.png_file );
    }

    if ( wr.pngs_ptr != NULL ) {
        png_destroy_read_struct(&wr.pngs_ptr, &wr.info_ptr, NULL);
    }

    if ( wr.rc != RC_TRUE ) {
        if ( NULL != image )
        free( image->image_data );
    }

    if ( NULL == imagep ) {  /* Huh?  Then cleanup the allocated memory ... */
        cleanup_pa_image( &image );
    }
    else *imagep = image;


    return ( wr.rc );


    /***************************************************************************
     ***************************************************************************
     */
    void cleanup_row_pointers( png_byte ***ptr ) {
        (free)( *ptr );
    }

    void user_error_fn(UNUSED_ARG png_struct *pngs_ptr, UNUSED_ARG png_const_charp desc) {
        /* Getting called does NOT supress a corresponding stderr message */
    }
}


/**
 ******************************************************************************
 * \callgraph
 * \callergraph
 */
char * O0 get_err_desc(pa_image_t const *const image)
{

    if ( NULL == image )
    return ( "FATAL MEMORY ERROR!" );
    return ( image->err_desc );
}


/**
 ******************************************************************************
 * Load the comments from the PNG image and save them in the comments_t array.
 *
 * \return     -1 on memory allocation error or comments is NULL.
 * \callgraph
 * \callergraph
 */
static rc_e O0 load_png_comments(png_struct *pngs_ptr, png_info *info_ptr, comments_t **comments, char const *const *keys)
{
    struct {
        png_text *texts;
        char     *key;
        int       len;
        int       idx;
        rc_e      rc;
    } w = {
        .len    = 0,
        .idx    = 0,
        .rc     = RC_FALSE,
    };


    while ( NULL != comments ) {
        if ( NULL == *comments ) {
            *comments = calloc(1, sizeof (comments_t));
        }

        png_get_text(pngs_ptr, info_ptr, &w.texts, &w.len);

        for ( ; w.idx < w.len; w.idx++ ) {
            w.key = w.texts[ w.idx ].key;
            if ( NULL != keys )
            for ( size_t ii = 0; NULL != keys[ ii ]; ii++ ) {
                char const *const p = keys[ ii ];
                if ( STR_MATCH == strncmp(w.key, p, strlen(p)) ) {
                    goto label;
                }
            }

            add_comments(*comments, w.key, w.texts[ w.idx ].text);

            label: ;
        }

        w.rc = RC_TRUE;
        break;
    }

    return ( w.rc );
}


/**
 ******************************************************************************
 * \callgraph
 * \callergraph
 */
comments_t *get_png_comments(pa_image_t const *image)
{

   return ( image->comments );
}


/**
 ******************************************************************************
 * \callgraph
 * \callergraph
 */
int set_no_comments(pa_image_t *image, int no_comments)
{

   int  olde_val = image->no_comments;
   image->no_comments = no_comments;

   return ( olde_val );
}


/**
 ******************************************************************************
 * \callgraph
 * \callergraph
 */
comments_t *replace_png_comments(pa_image_t *const image, comments_t *comments)
{

    cleanup_comments( &image->comments );
    image->comments = comments;

    return ( image->comments );
}


/**
 ******************************************************************************
 * \callgraph
 * \callergraph
 */
int set_jpeg_quality(pa_image_t *pa_image, int new_quality)
{
    int val = pa_image->jpeg_quality;

    pa_image->jpeg_quality = new_quality;  /* JPEG support, range 0..100 */

    return ( val );
}


/**
 ******************************************************************************
 * \callgraph
 * \callergraph
 */
static pa_image_t *alloc_png_image()
{
    pa_image_t *pa_image = calloc(1, sizeof (pa_image_t));

    return ( pa_image );
}


/**
 ******************************************************************************
 * \callgraph
 * \callergraph
 */
int get_image_width(pa_image_t *pa_image)
{
    return ( pa_image->width );
}


/**
 ******************************************************************************
 * \callgraph
 * \callergraph
 */
int get_image_height(pa_image_t *pa_image)
{
    return ( pa_image->height );
}


/**
 ******************************************************************************
 * \callgraph
 * \callergraph
 */
int O0 write_image_file(char const *const filename, pa_image_t *png_image, char const *const image_type)
{
    struct {
       int  rc;
    } w = {
       .rc = 0,
    };


    do if ( 0 == strcmp(image_type, "png") ) {
        w.rc = save_pngfile(filename, png_image);
        break;
    }
    else if ( 0 == strcmp(image_type, "jpg") ) {
        w.rc = save_jpgfile(filename, png_image);
        break;
    }
    else {
        fprintf(stderr, "UNSUPPORTED output type '%s' specified.\n", image_type);
    } while ( 0 );


    return ( w.rc );
}


/**
 ******************************************************************************
 * Write the image to a JPG file using libjpeg.
 *
 * Really not much different than writing a PNG file.
 * \callgraph
 * \callergraph
 *
 * https://github.com/LuaDist/libjpeg/blob/master/example.c
 */
static int O0 save_jpgfile(JPEG_ARGS_UNUSED char const *const jpg_filename, JPEG_ARGS_UNUSED pa_image_t *png_image)
{
    struct {
        int         rc;
        int         width;
        int         height;
        int         row_bytes;
        int         quality;
        char const *filename;
        JSAMPLE    *buf;

        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr       jerr;
        FILE                       *jpg_file;
        comments_t *comments;
        size_t      idx;
    } wj = {
        .width     = png_image->width,
        .height    = png_image->height,
        .row_bytes = png_image->row_bytes,
        .quality   = png_image->jpeg_quality,
        .filename  = jpg_filename,
        .buf       = png_image->image_data,

        .rc        = 0,
    };
    JSAMPROW row_pointer[1];

#if defined(ENABLE_JPEG_RW)

    wj.cinfo.err = jpeg_std_error( &wj.jerr );

    jpeg_create_compress( &wj.cinfo );

    wj.jpg_file = fopen(wj.filename, "wb");  // FIXME Error check
    jpeg_stdio_dest( &wj.cinfo, wj.jpg_file );

    wj.cinfo.image_width = wj.width;
    wj.cinfo.image_height = wj.height;
    wj.cinfo.input_components = 3;      /* # of color components per pixel */
    wj.cinfo.in_color_space = JCS_RGB;  /* colorspace of input image */

    jpeg_set_defaults( &wj.cinfo );

    jpeg_set_quality( &wj.cinfo, wj.quality, TRUE );

    jpeg_start_compress( &wj.cinfo, TRUE );

    if ( 0 == png_image->no_comments ) {
        char const *comment = strdup("");
        size_t      comment_len;
        char       *s1;

        wj.comments = get_png_comments( png_image );
        if ( NULL != wj.comments && wj.comments->cnt ) {
            for ( wj.idx = 0; wj.idx < wj.comments->cnt; wj.idx++ ) {
                comment_len = asprintf(&s1, "%s%s: %s\n", comment, wj.comments->kvs[ wj.idx ].key, wj.comments->kvs[ wj.idx ].val);
                free(comment);
                comment = s1;
            }
            jpeg_write_marker(&wj.cinfo, JPEG_COM, (JOCTET const *) comment, comment_len);
        }
        (free)((void *) comment);
    }

    while ( wj.cinfo.next_scanline < wj.cinfo.image_height ) {
        row_pointer[0] = &wj.buf[ wj.cinfo.next_scanline * wj.row_bytes ];

        jpeg_write_scanlines( &wj.cinfo, row_pointer, 1 );
    }

    jpeg_finish_compress( &wj.cinfo );
    fclose( wj.jpg_file );

    jpeg_destroy_compress( &wj.cinfo );

#else
    fprintf(stderr, "NOT COMPILED WITH libjpeg SUPPORT.\n", image_type);
#endif

    return ( wj.rc );
}


/**
 ******************************************************************************
 * TODO FIXME TODO needs checking + cleanup (it works by Grace right now).
 * \callgraph
 * \callergraph
 */
static int O0 save_pngfile(char const *const png_filename, pa_image_t *png_image)
{
    png_struct *pngs_ptr;
    struct {
        char const *const png_filename;
        pa_image_t *png_image;

        png_infop  info_ptr;
        FILE *file;
        rc_e  rc;
    //-d  enum    { RC_FALSE = 0, RC_TRUE = 1 } rc;
    } volatile ww = {
        .png_image = png_image,
        .png_filename = png_filename,
        .rc = RC_FALSE,

        .file = NULL, .info_ptr = NULL,
 };

/*       ww.png_image->err_desc = NULL; */

    do {
        /*
         * Get the two pointers that PNG needs to work with the file.
         */
        pngs_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
        if( NULL == pngs_ptr ) {
            set_err_desc( ww.png_image->err_desc, "png_create_read_struct() failed" );
            break;
        }

        ww.info_ptr = png_create_info_struct( pngs_ptr );
        if( NULL == ww.info_ptr ) {
            set_err_desc( ww.png_image->err_desc, "png_create_info_struct() failed" );
            break;
        }

        if( setjmp(png_jmpbuf( pngs_ptr ) ) ) {
// FIXME    png_destroy_write_struct(&pngs_ptr, &ww.info_ptr);
            break;
        }

        ww.file = fopen(ww.png_filename, "wb");
        if ( NULL == ww.file ) {
            printf("Error opening %s for writing! (%s)\n", ww.png_filename, strerror( errno ));
            break;
        }

        png_init_io(pngs_ptr, ww.file);
        png_set_compression_level(pngs_ptr, 7);

        png_set_IHDR( pngs_ptr,
                      ww.info_ptr,
                      png_image->width,
                      png_image->height,
                      8,
                      PNG_COLOR_TYPE_RGB,
                      PNG_INTERLACE_NONE,
                      PNG_COMPRESSION_TYPE_DEFAULT,
                      PNG_FILTER_TYPE_DEFAULT );

        /*
         * While we're at it, add some meaningful comments to the PNG image.
         */
//TODO  png_set_text(pngs_ptr, ww.info_ptr, png_image->texts, png_image->texts_len );

        png_write_info(pngs_ptr, ww.info_ptr);

        /* Use png_set_bgr(pngs_ptr) to set blue, green, red order for pixels. */

        png_bytep row_pointers[ png_image->height ];  /* Set the PNG's row pointers. */
        for ( int yy = 0; yy < png_image->height; yy++ ) {
            row_pointers[ yy ] = png_image->image_data + (yy * png_image->row_bytes);
        }

        png_write_image( pngs_ptr, row_pointers );
        png_write_end( pngs_ptr, ww.info_ptr );

        ww.rc = RC_TRUE;
    } while ( 0 );

    if( NULL != ww.file ) {
        fclose( ww.file );
    }

    if( pngs_ptr != NULL) {
        png_destroy_write_struct( &pngs_ptr, (png_infopp ) &ww.info_ptr );
    }

    return ( ww.rc );

    /***************************************************************************
     */
    void cleanup_row_pointers( png_byte ***ptr ) {
        (free)( *ptr );
    }
}


/**
 *******************************************************************************
 * \callgraph
 * \callergraph
 */
void cleanup_pa_image(pa_image_t **image)
{

    prune_pa_image( image );

    free ( *image );
    return ;
}


/**
 *******************************************************************************
 * \callgraph
 * \callergraph
 */
static void prune_pa_image(pa_image_t **image)
{

    if ( NULL != *image ) {
        (free)((*image)->image_data);
        (free)((*image)->err_desc);

        cleanup_comments( &(*image)->comments );
    }

    return ;
}


/**
 *******************************************************************************
 * Render the ASS_Image list onto the png image.
 *
 * \param  img        List of image segments returned from 'ass_render_frame()'.
 * \param  skip_last  This indicates if the last segment of the ASS_Image list
 *                    should NOT be rendered.  <br> This was mainly used in the
 *                    development of pngass and is not necessary afer the two
 *                    pass code was completed.  <br>
 *                    This flag should always be 0.
 * \callgraph
 * \callergraph
 */
int O3 blend_pa_image(pa_image_t *png_image, ASS_Image *img, int skip_last)
{
#define _r(c)   ((c) >> 24)
#define _g(c)  (((c) >> 16) & 0xFF)
#define _b(c)  (((c) >>  8) & 0xFF)
#define _a(c)   ((c)        & 0xFF)
    int  cnt = 0;

    while( NULL != img ) {
        unsigned char opacity = 255 - _a(img->color);
        unsigned char r = _r(img->color);
        unsigned char g = _g(img->color);
        unsigned char b = _b(img->color);

        unsigned char *src = img->bitmap;
        unsigned char *dst = png_image->image_data + img->dst_y * png_image->row_bytes + img->dst_x * 3;

        for ( int y = 0; y < img->h; y++ ) {
            unsigned char *p = dst;
            for ( int x = 0; x < img->w; x++ ) {
                unsigned k = ((unsigned) src[ x ]) * opacity / 255;
                *p = (k * b + (255 - k) * *p) / 255;
                p++;
                *p = (k * g + (255 - k) * *p) / 255;
                p++;
                *p = (k * r + (255 - k) * *p) / 255;
                p++;
            }
            src += img->stride;
            dst += png_image->row_bytes;
        }

        ++cnt;
        img = img->next;

        if ( NULL != img )
        if ( skip_last && (NULL == img->next) ) {
            break;
        }
    }

    return ( cnt );
}

