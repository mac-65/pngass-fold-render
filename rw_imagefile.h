#ifndef RW_IMAGEFILE_H
#define RW_IMAGEFILE_H
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

/*
 *******************************************************************************
 * These are PNG comment keys that we're NOT going to load / replicate.
 *     date:create: 2018-09-06T12:39:34-04:00
 *     date:modify: 2018-07-26T09:02:45-04:00    << KEEP THIS
 *
 * Comment tags seem to be vaguely defined.  It looks like the only tag added
 * by 'xv' is 'Software'.
 *
 * Notes:
 *   1. https://sno.phy.queensu.ca/~phil/exiftool/TagNames/PNG.html
 *   2. ImageMagick convert's timestamp tag
 */
#undef IGNORE_KEYS
#define IGNORE_KEYS (char const *const []) {  /* UNSORTED :: is okay */  \
    "Software",     /* 1 */                           \
    "date:create",  /* 2 */                           \
    "create-date",  /* 1 */                           \
    NULL,                                             \
}

/**
 *******************************************************************************
 * The libpng library doesn't allow a caller to read parts of a PNG image.
 *
 * So, to reliably read the comments, you have to read the entire image --
 * you can't cheat and skip reading the image data or just read a single
 * row and get meaningful comments (at least I couldn't get it to work).
 * The symptoms are usually only the 1st comment record is returned or a
 * warning will be printed --
 *
 *    'libpng warning: IDAT: Too much image data'
 *
 * So these values are mainly markers for the caller's intent; only
 * READ_ONLY_METADATA is implemented since that data is available soon
 * after opening the PNG image.
 */
typedef enum {
    READ_WHOLE_IMAGE    =    0,
    READ_ONLY_COMMENTS  = 0x01,
    READ_ONLY_METADATA  = 0x02,  /**< Stop reading after 'png_read_info()' */
} read_opts_e;

/**
 *******************************************************************************
 * This opaque structure is related to the load / save of the IMAGE file.
 */
typedef struct pa_image_t pa_image_t;

/**
 *******************************************************************************
 */
rc_e        read_png_image(char const *const png_filename, pa_image_t **image, char const *const *keys, read_opts_e);
int         write_image_file(char const *const filename, pa_image_t *, char const *const);

int         blend_pa_image  (pa_image_t *png_image, ASS_Image *img, int skip_last);

int         get_image_width (pa_image_t *pa_image);
int         get_image_height(pa_image_t *pa_image);
int         set_jpeg_quality(pa_image_t *pa_image, int new_quality);
int         set_no_comments (pa_image_t *pa_image, int no_comment_value);

char       *get_err_desc    (pa_image_t const *const image);

void        cleanup_pa_image(pa_image_t **png_image);
comments_t *get_png_comments    (pa_image_t const *image);
comments_t *replace_png_comments(pa_image_t *const image, comments_t *comments);

#endif  /* RW_IMAGEFILE_H */

