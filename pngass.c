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

/*******************************************************************************
 * Apply (one or more of) a subtitle file to the specified PNG image.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>    /* clock() */
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>  /* getopt() */
#include <glob.h>

#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <locale.h>
#include <ass/ass.h>

#include "pa_misc.h"
#include "rw_arrays.h"
#include "rw_textfile.h"
#include "rw_imagefile.h"
#include "pa_edits.h"

#include "pngass.h"

#ifndef realloc
#warning "realloc() is not a macro..."
#endif

#undef COUNT_ASS_Images
#ifdef __OPTIMIZE__
  #define COUNT_ASS_Images(p_) (0)
#else
  #define COUNT_ASS_Images(p_)  ({        \
      ASS_Image *i_ = (p_);               \
      short     c_ = 0;                   \
      if ( NULL != i_ )                   \
      while ( c_++, NULL != i_->next ) {  \
          i_ = i_->next;                  \
      }                                   \
      c_;                                 \
    })
#endif


/**
 *******************************************************************************
 * + https://www.utf8-chartable.de/unicode-utf8-table.pl?start=8064&names=-&utf8=string-literal
 *   https://www.fileformat.info/info/unicode/char/3000/index.htm
 */
#undef UTF8_LEFT_CORNER_BRACKET            /* Japanese left single */   /*+*/
#define UTF8_LEFT_CORNER_BRACKET    "\xE3\x80\x8C"
#undef UTF8_RIGHT_CORNER_BRACKET                                        /*+*/
#define UTF8_RIGHT_CORNER_BRACKET   "\xE3\x80\x8D"
#undef UTF8_LEFT_WHITE_CORNER_BRACKET      /* Japanese left double */   /*+*/
#define UTF8_LEFT_WHITE_CORNER_BRACKET    "\xE3\x80\x8E"
#undef UTF8_RIGHT_WHITE_CORNER_BRACKET                                  /*+*/
#define UTF8_RIGHT_WHITE_CORNER_BRACKET   "\xE3\x80\x8F"
#undef BOM_SEQUENCE
#define BOM_SEQUENCE  "\xEF\xBB\xBF"  /* BOM sequence */
#undef UTF8_FRACTION_SLASH                                              /*+*/
#define UTF8_FRACTION_SLASH "\xE2\x81\x84"  /* '/' (used for filenames) */
#undef UTF8_NON_BREAK_SPACE
#define UTF8_NON_BREAK_SPACE "\xC2\xA0"  /* Can *probably* appear as '\xA0' */
/**
 ******************************************************************************
 * For the '--pad-paragraph' option, we'll insert a zero-width SPACE character
 * at the start of each paragraph.  This SPACE will have a larger font size
 * (indicated by the option) to increase the spacing between paragraphs
 * without adding a full line of whitespace.
 *
 * FACTOID :: libass will "see" the UTF8_ZERO_WIDTH_SPACE character (i.e., if
 * it's not used w/the '--pad-paragraph' option, there's no effect) but will
 * not treat it as a "fold-able" SPACE character (like it's spec'd out for in
 * html documents).  Bug, dunno..?
 *
 * In vi, enter insert mode and control-v u200b to add a zero-width SPACE.
 * This works well and really improves readability of tightly packed text -->
 * ("Isekai wa Smartphone to Tomo ni.").
 */
#undef UTF8_ZERO_WIDTH_SPACE                                            /*+*/
#define UTF8_ZERO_WIDTH_SPACE "\xE2\x80\x8B"
#undef UTF8_IDEOGRAPHIC_SPACE                                           /*+*/
#define UTF8_IDEOGRAPHIC_SPACE "\xE3\x80\x80"
#undef SPACE_CHAR
#define SPACE_CHAR      " "

/**
 ******************************************************************************
 * After I detect a token that causes the rendering to clip, the ASS_Images
 * are scanned backwards to find all on the ASS_Images that are clipped.
 * The theory is to back up to the start of the line so that the whole line
 * will be moved to the next template for rendering.  This is checked by
 * testing the lower right corner against the bottom margin.
 *
 * However...
 *
 * Not all ASS_Images for the line may meet this test - there may be gaps.
 * See EXAMPLEs/snapshot-02-PITA.png for a picture is worth a thousand words.
 ******************************************************************************
 */
#undef ONE_PIXELs_FUDGE
#define ONE_PIXELs_FUDGE (2)  /* 10% of the font's height, or this default */

/**
 ******************************************************************************
 * This appeared in "Isekai wa Smartphone to Tomo ni."  What to do...?
 * See 'ASS_TRY_SANDBOX_TOKEN' for how these are handled.
 */
#undef STRANGE_CHAR_1
#define STRANGE_CHAR_1  "\xC2\xAD"

/**
 ******************************************************************************
 * We take the chapter's title from the first line of the input text file.
 * We limit its size to this value and will truncate it if it's longer.
 * Used by 'get_chapter_title()'.
 */
#undef MAX_CHAPTER_TITLE_SZ
#define MAX_CHAPTER_TITLE_SZ  (100)

/**
 ******************************************************************************
 * This is used to build a "padding" string for the '--pad-paragraph' option.
 */
#undef PAD_FMT
#define PAD_FMT "{\\fs%d}" UTF8_ZERO_WIDTH_SPACE "{\\fs%d}"


typedef enum {
    /**
     **************************************************************************
     * This is where we can't tell if the ASS_Image from the last token is
     * off the image, or contains all characters which do not render.
     */
  ASS_TRY_SANDBOX_TOKEN = 0,
    /**
     **************************************************************************
     * The last token has rendered completely in the image.
     */
  ASS_IN_IMAGE,
    /**
     **************************************************************************
     * Not a BUG and should NOT see.  It's due to libass comments in the text.
     *
     * Example Dialogue:  Hello {this will not be rendered by libass.}
     *
     * The token "Hello" is added.  Then the token "{this" is added and so on.
     * libass renders each token including the comment start "{this" since
     * we're "feeding" libass one token at a time.  When the token containing
     * the '}' comment close character is added, libass parses a complete
     * comment and doesn't render any of it now.  This will _sometimes_ cause
     * the # of ASS_Images to *decrease*.
     *
     * It's _sometimes_ because it depends on where the comment is, template
     * breaks (if the '}' comment close sequence folds to the next template,
     * the comment will be rendered) and other factors about the input text.
     *
     * Since these usually aren't comments, the opening '{' should be escaped
     * in the text before processing by pngass (TODO :: maybe an option?)
     */
  ASS_DROPPED,
    /**
     **************************************************************************
     * Indicates that the _last_ ASS_Image is partially rendered (clipped).
     * Since libass will build these, detecting them is a little troublesome.
     */
  ASS_LAST_IS_CLIPPED,
    /**
     ***************************************************************************
     * At the start of an image, if there are NO renderable characters,
     * 'ass_render_frame()' won't render anything.  In this RARE case, we need
     * to continue (discovered w/Isekai_wa_Smartphone_to_Tomo_ni.).
     *
     ***************************************************************************
     * If these characters appear at the end of a file at the start of a page, it
     * (untested -- probably at worst) will produce a blank image.  I don't think
     * that'll happen as they should be consumed completely by the previous page
     * (just like any trailing SPACEs or '\N's are consumed).
     */
    ASS_NO_IMAGE_ERROR,
} ass_cmpr_e;


/**
 *******************************************************************************
 * This is the maximum number of libass template steps we support for a single
 * text input file.  A libass template step is defined as a section of text
 * that a libass template is applied, and an image can contain multiple libass
 * templates (loosely thought of as text columns).  So if a text file is
 * rendered as double-columns, then each image requires two libass templates,
 * and the number of supported images would be half of 'MAX_TEMPLATE_STEPS'.
 *
 * The header, footer, and graphics are libass templates, but are not counted.
 */
#undef MAX_TEMPLATE_STEPS
#define MAX_TEMPLATE_STEPS (1024)  // TODO :: make dynamic
typedef struct text_segments_t {
    unsigned int text_start_idx;
    unsigned int work_text_delta;
    short  clipped;    /* FYI use, indicates the last ASS_Image was clipped */
    short  pieces;     /* # of ASS_Images from this text segment's render */
                /**
                 ***************************************************************
                 * FUTURE?  In case the text has to split in a column I think
                 *          I was thinking of using this for the ironwork fonts
                 *          to move them up vertically in the column because
                 *          their height leaves too much space above the glyph.
                 * Dunno if it's worth all of the complexity to manage this...
                 */
    unsigned short target_y;
} text_segments_t;


typedef enum {
    FOLD_PASS_1 = 0,
    FOLD_PASS_2,
    FOLD_PASS_END
} fold_pass_e;         /* In 'pa_opts' for 'ass_set_message_cb()' */


typedef struct details_t {
    char const *chapter_filename;  /* NOT free()-able, from 'basename()' */
    char const *in_png_name;       /* NOT free()-able, in_png_list.pathnames */
    char const *png_Title;         /* PNG :: set by 'get_chapter_title()' */
    char const *png_Arc;           /* Isekai wa Smartphone to Tomo ni. */ // TODO
    char const *image_prefix;

    struct {
        size_t  chapter_image_number;
        size_t  chapter_images;
        size_t  global_image_sequence_number;  /* for all of the chapters processed */

        unsigned int dup_lines_found;   /* Initialized through its API */
        unsigned int dup_groups_found;  /* Initialized through its API */
    };

    char const *dest_dir;   /* This must always be set (can't be NULL) */
    char        image_type[ sizeof ("jpg") ];
} details_t;


typedef struct pa_opts {
    int         png_width;
    int         png_height;
    int         jpeg_quality;
    int         margin_bottom;   /* the bottom margin */
    double      line_spacing;    /* support 'ass_set_line_spacing()' */

    xy_format_e xy_format;
            /**
             ******************************************************************
             * The purpose of these are to provide a simple reset mechanism for
             * the user to reset some of the text character attributes if they
             * were edited by a sed script.  These are the most common.
             *
             * s/\<Liza\>/{\\b1\\1c\&H003366\&}Liza{\\1c\&H080808\&\\b0}/g
             *
             * Here the color is reset manually (&H080808), but the user has to
             * match the text color used in the template otherwise the before
             * and after text colors won't match.  So rather than those manual
             * edits, if the user uses this script segment -->
             *
             * s/\<Liza\>/{\\b1\\1c\&H003366\&}Liza{@@1c@@\\b0}/g
             *
             * then this program will automatically replace the sequence
             * '@@1c@@' with the sequence '\\1c\&H080808\&'.  It'll use a
             * 'sed -e 's/@@1c@@/color/g' *after* the '--file arguments'.
             *
             * Also supported are: '@@size@@' and '@@face@@'.
             *
             * I'd love for libass to have a PostScript-like state-stack :).
             * The libass library doesn't provide a state machine such that
             * the current character attributes can be pushed and popped.
             * This isn't a bug in libass, just the rational for this feature.
             */
    struct {
        char    colour_1c[ sizeof("080808") ];  /* Template's text colour */
        char    alpha_1a [ sizeof("08") ];
        int     text_size;
        char   *text_face;

        short   pad_paragraph;
        short   pixel_fudge;
    };

    struct {
        char const *debug_sed_dir;  // TODO
        char const *debug_text_dir;
        char const *debug_work_dir;
    };

    size_t      chop_chars;    /* Remove this # of chars from the prefix */
    char       *chop_prefix;   /* Remove this prefix "string" from the name */

            /**
             ******************************************************************
             * Specify the original directory for the filtered images.  This
             * is to copy the PNG comments to the new image because 'convert'
             * does NOT copy the existing PNG comments.  I don't know if it's
             * an option "fix" and it's possible that I could've missed it.
             */
    char       *original_dir;

    fold_pass_e fold_pass;

    details_t   details;

    strptrary_t in_chapters;
    strptrary_t templates;
    strptrary_t font_dirs;    /* See ass_set_fonts_dir(ASS_Library *, ...) */
    strptrary_t in_png_list;
    strptrary_t sed_script_files;
    strptrary_t header_template;

    char pad_str[ sizeof (PAD_FMT) + 6 ];

    /**
     **************************************************************************
     * https://sno.phy.queensu.ca/~phil/exiftool/TagNames/PNG.html
     *
     * Initially, we're supporting PNG specific tag(s):
     *  - Software
     */
    char  *png_Software;     /* PNG :: set by _this_ application */

    attr_stats_t  *attr_stats;
    attr_edits_e   attr_edits : 16;

    enum {
           VERBOSE_QUIET = 0, /* Nothing except hard errors */
           VERBOSE_1,         /* Summary: chapter, # images and warnings */
           VERBOSE_2,         /* pngass progression details */
           VERBOSE_MAX,       /* details from libass (fonts selected, etc.) */
           VERBOSE_MASK  = 0x07,
         } verbose_level : 4;

    unsigned int  remove_dup_groups : 4;  /**< Remove duplicate blocks */
    unsigned int  remove_dup_group_spaces : 2;  /**< Remove spaces between
                                                     duplicate groups */

    int  no_comments : 2;
    int  regression : 2;   /**< Regression -- for testing / development.
                            * This will exclude comments values that would
                            * change for the same image - e.g. timestamp and
                            * render time.  This is to allow two images to
                            * be compared using 'md5sum' for code changes'
                            * regression testing -- easy peasy!
                            */
    int  url_zwsp : 2;     /**< Sometimes a URL appears in either the text or
                            * a TL note.  Since libass does NOT force-break
                            * a line, we'll insert the string '{\fs1} {\fs}'
                            * after the specified characters (default '/').
                            * Substitutions will only happen within a line
                            * (constrained by the use of 'memmem()').
                            */
    int  hyphenation : 2;  /**< Like 'url_zwsp' but for '-'s.
                            */
} pa_opts_t;


/**
 ******************************************************************************
 * TODO :: description
 */
typedef struct pa_ass_t {
    short          pieces;
    int            w;
    unsigned int  *piece_starts;
    unsigned int   token_start_idx;
} pa_ass_t;


/******************************************************************************/
static rc_e   pa_parse_cmdline ( pa_opts_t *pa_opts, int argc, char **argv );
static rc_e   pa_verify_opts   ( pa_opts_t *pa_opts );
static void   set_opt_defaults ( pa_opts_t *pa_opts );
static void   post_opt_defaults( pa_opts_t *pa_opts );
static void   cleanup_pa_opts  ( pa_opts_t **pa_opts );
static char  *get_Software     ( void );

static size_t add_font_search_dirs( ASS_Library *ass_library, strptrary_t *font_dirs );

static pa_image_t *load_pa_image( pa_opts_t const *pa_opts, char const *const filename );
static rc_e      save_pa_image( pa_image_t *pa_image, pa_opts_t *pa_opts, clock_t );

static char *process_textfile( char const *const filename, pa_opts_t * );
static void  write_debug_text( char const *const filename, char const *const str, pa_opts_t * );
static char *make_work_text  ( char const *const in_text, char const *const pad );
static void  trim_work_text  ( pa_opts_t const *, char const *const work_text, text_segments_t *const, unsigned int template_pass );
static char *debug_work_text ( char const *const work_text, char const *const dir, char const *const chapter_filename );

static void   apply_template_complex( pa_image_t *, pa_opts_t *, char const *const template, char *const work_text_2, text_segments_t *const );
static size_t apply_template_simple ( pa_image_t *, pa_opts_t * );

static size_t skip_non_text_tokens(char const *const in_text, size_t in_idx, int, int );

static ass_cmpr_e cmpr_last_ASS_Image( pa_ass_t *const dst, ASS_Image *src, int height, short, char const *const );

static int      verify_template( char const *const template, char const *types, char const *const final_type );

static char const *get_chapter_title( char *const ptr, size_t max );
static void        cleanup_details( details_t *details );
static char       *build_name_from_details( details_t *const details );

static char *remove_attributes( char *const );
static char *make_legal_name  ( char *const );
static char const *chop_prefix( char const *name, char const *, size_t );

static void libass_msg_callback( int level, const char *fmt, va_list args, void *vp );

/**
 *******************************************************************************
 *******************************************************************************
 * Simple program to render an ASS file onto a PNG image.
 *
 * Usage :: pngass _options_
 *
 * TODO :: still have to provide a --help and document the features.
 */
int main(int argc, char *argv[])
{
    auto void cleanup_text_segments( text_segments_t ** );
    pa_opts_t *pa_opts = calloc( 1, sizeof (pa_opts_t) );

    pa_parse_cmdline( pa_opts, argc, argv );


    if ( NULL == pa_opts->details.dest_dir ) {
        fprintf(stderr, "ERROR - no suitable destination directory was specified (--dest-dir)!\n");
        goto quit;
    }

    if ( 0 == pa_opts->in_png_list.cnt ) {
        fprintf(stderr, "ERROR - no PNG images were specified on the command line (--png-glob)!\n");
        goto quit;
    }

    if ( pa_opts->verbose_level > VERBOSE_QUIET ) {
        char  in_chapters[ 16 ] = "'stdin'";
        char *desc = "";
        if ( pa_opts->in_chapters.cnt ) {
            snprintf(in_chapters, sizeof (in_chapters), "%u", pa_opts->in_chapters.cnt);
            desc = " files";
        }
        fprintf(stderr, "Processing %s input text%s ...\n", in_chapters, desc);
    }

    if ( 0 == pa_opts->in_chapters.cnt ) {
        append_a_pathname( &pa_opts->in_chapters, READ_STDIN );
    }

    if ( pa_opts->templates.cnt > 0 ) {
        struct {
            size_t    image_page_total;
            unsigned int  template_pass;
            unsigned int  png_filename_idx;
            unsigned int  save_png_filename_idx;

            char     *work_text;

            unsigned int  text_start_idx;
            unsigned int  work_text_delta;

            char     *in_text;

            clock_t   my_clock;
        } w = {
            .text_start_idx = 0
        };

        text_segments_t *text_segments __attribute__ ((__cleanup__ (cleanup_text_segments))) = NULL;
        text_segments = realloc(text_segments, (MAX_TEMPLATE_STEPS * sizeof (text_segments_t)));

        /*
         ***************************************************************************
         * Build the paragraph pad string, if needed, else, it's an EMPTY string.
         */
        if ( pa_opts->pad_paragraph ) {
            snprintf(pa_opts->pad_str, sizeof (pa_opts->pad_str), PAD_FMT, pa_opts->pad_paragraph, pa_opts->text_size);
        }

        for ( size_t chapter_idx = 0; chapter_idx < pa_opts->in_chapters.cnt; chapter_idx++ ) {

            /*
             *******************************************************************
             * We rely on 'text_segments_t->text_start_idx' being ZERO.
             */
            memset(text_segments, '\0', (MAX_TEMPLATE_STEPS * sizeof (text_segments_t)));

            w.in_text = process_textfile( pa_opts->in_chapters.pathnames[ chapter_idx ], pa_opts );

            pa_opts->details.chapter_filename = pa_opts->in_chapters.pathnames[ chapter_idx ];
            if ( pa_opts->verbose_level > VERBOSE_QUIET ) {
                fprintf(stderr, "Adding :: '%s' ...\n", pa_opts->details.chapter_filename);
            }
            free ( pa_opts->details.png_Title );

            w.work_text = make_work_text( w.in_text, pa_opts->pad_str );
            debug_work_text( w.work_text, pa_opts->debug_work_dir, pa_opts->details.chapter_filename );
            free ( w.in_text );

            pa_opts->details.png_Title = get_chapter_title( w.work_text, MAX_CHAPTER_TITLE_SZ );
            //g BREAK_STR(pa_opts->details.png_Title, "1-6. The Marketplace");

            w.save_png_filename_idx = w.png_filename_idx;

            for ( pa_opts->fold_pass = FOLD_PASS_1; pa_opts->fold_pass < FOLD_PASS_END; pa_opts->fold_pass++ ) {

                w.png_filename_idx = w.save_png_filename_idx;
                w.template_pass = 0;
                w.text_start_idx = 0;
                pa_opts->details.chapter_image_number = 0;

                while ( *(w.work_text + w.text_start_idx) ) {

                    pa_opts->details.chapter_image_number++;

                    if ( FOLD_PASS_2 == pa_opts->fold_pass ) {
                        pa_opts->details.global_image_sequence_number++;
                    }

                    size_t jdx = w.png_filename_idx % pa_opts->in_png_list.cnt;
                    pa_opts->details.in_png_name = pa_opts->in_png_list.pathnames[ jdx ];
                    w.png_filename_idx++;

                    w.my_clock = clock();
                    pa_image_t *pa_image = load_pa_image( pa_opts, pa_opts->details.in_png_name );

                    /*
                     ***************************************************************
                     * Apply the header template now before the "text" template(s).
                     */
                    if ( FOLD_PASS_2 == pa_opts->fold_pass && 1 == pa_opts->header_template.cnt ) {
                        apply_template_simple( pa_image, pa_opts );
                    }

                    /*
                     ***************************************************************
                     * Iterate through and apply all of the "text" templates.
                     */
                    for ( size_t idx = 0; idx < pa_opts->templates.cnt; idx++ ) {
                        size_t      template_len;
                        char const *template = read_textfile_sz( pa_opts->templates.pathnames[ idx ], &template_len, 1280 );

                        if ( RC_FALSE == verify_template( template, "ddsdss", ".*s" ) ) {
                           fprintf(stderr, "\nFATAL :: invalid template '%s'\n", pa_opts->templates.pathnames[ idx ]);
                           _exit( 1 );
                        }

                        trim_work_text( pa_opts, w.work_text, text_segments, w.template_pass );
                        apply_template_complex( pa_image,
                                                pa_opts,
                                                template,
                                                w.work_text,
                                               &text_segments[ w.template_pass ]
                                              );
                        free (template);

                        /*
                         ***********************************************************
                         * Calculate the next start in the string.  If we're at the
                         * end, then quite the 'pa_opts->templates.cnt' loop.
                         */
                        w.text_start_idx = text_segments[ w.template_pass ].text_start_idx
                                         + text_segments[ w.template_pass ].work_text_delta;
                        if ( '\0' == *(w.work_text + w.text_start_idx) ) {
                            break;
                        }

                        w.template_pass++;
                        text_segments[ w.template_pass ].text_start_idx = w.text_start_idx;
                    }

                          // TODO :: TEST :: will we ever get here w/nothing to render?
                    if ( FOLD_PASS_2 == pa_opts->fold_pass ) {
                        save_pa_image( pa_image,
                                       pa_opts,
                                       w.my_clock
                                     );
                    }

                    cleanup_pa_image( &pa_image );
                }

                /*
                 ***************************************************************
                 * All of that 2-pass stuff just to accurately calculate this -
                 */
                pa_opts->details.chapter_images = pa_opts->details.chapter_image_number;
            } // end FOLD_PASS loop

            if ( pa_opts->verbose_level >= VERBOSE_MAX ) {
                fprintf( stderr, "TEMPLATE PASSES = %u, CHAPTER IMAGES = %lu\n", w.template_pass, pa_opts->details.chapter_image_number);
                for ( unsigned int ii = 0; ii < w.template_pass; ii++ ) {
                    fprintf(stderr, "  TEMPLATE PASS #%-2u :: %4u -> %4u bytes\n", ii + 1, text_segments[ ii ].text_start_idx, text_segments[ ii ].work_text_delta);
                }
                fflush(stderr);
            }

            free (w.work_text);

            /*
             *******************************************************************
             * Even though we're "in-scope" here, the 'cleanup_text_segments()' is
             * be called _before_ the terminating condition of the for-loop is
             * tested (hope this is spec'd this way, gcc isn't very clear) ...
             */
        }
    }
    else {
        fprintf(stderr, "NOTE - no templates were specified on the command line!!!\n");
    }

quit:
    cleanup_pa_opts( &pa_opts );

    return ( 0 );


    void cleanup_text_segments( text_segments_t **p )
    {
        (free)( (void *) *p );
    }
}


/**
 *******************************************************************************
 */
static pa_image_t *load_pa_image(pa_opts_t const *pa_opts, char const *const filename)
{
    auto void cleanup_pathname( char ** );
    struct {
        pa_image_t   *image;
        comments_t *comments;
        char       *desc;
        rc_e        rc;
    } w = {
        .image    = NULL,
        .comments = NULL,
        .rc       = RC_FALSE,
    };


    if ( FOLD_PASS_2 == pa_opts->fold_pass ) {
        if ( NULL != pa_opts->original_dir ) {
            char  *pathname __attribute__ ((__cleanup__ (cleanup_pathname))) = NULL;

            /*
             *******************************************************************
             * An original dir was specified, try to load those PNG comments.
             */
            if ( RC_TRUE == new_from_filename( &pathname, filename, pa_opts->original_dir ) ) {
                w.rc = read_png_image( pathname, &w.image, IGNORE_KEYS, READ_ONLY_COMMENTS );
                if ( RC_TRUE == w.rc ) {
                    w.comments = dup_comments( get_png_comments( w.image ) );
                }
                else if ( NULL != w.image ) {
                    if ( pa_opts->verbose_level >= VERBOSE_1 ) {
                        w.desc = get_err_desc( w.image );
                        fprintf(stderr, "WARNING :: can't access comments from '%s' in %s\n",
                                        filename, w.desc);  /*+*/
                    }
                }
            }
            else if ( pa_opts->verbose_level >= VERBOSE_1 ) {
                fprintf(stderr, "WARNING :: can't access comments in '%s' from '%s' -- error %d, %s\n",
                                pathname, filename, errno, strerror(errno)); /*+*/
            }
            cleanup_pa_image( &w.image );
        }

        w.rc = read_png_image( filename, &w.image, IGNORE_KEYS, READ_WHOLE_IMAGE );
        if ( RC_TRUE != w.rc ) {
            goto check_err_desc;
        }
        if ( NULL != w.comments ) {
            replace_png_comments( w.image, w.comments );
        }
    }
    else {
        w.rc = read_png_image( filename, &w.image, NULL, READ_ONLY_METADATA );
        if ( RC_TRUE != w.rc ) {
            goto check_err_desc;
        }
    }

    return ( w.image );

    check_err_desc:
    if ( NULL != w.image ) {
        w.desc = get_err_desc( w.image );
        fprintf(stderr, "[pngass] Error :: %s", w.desc);
        cleanup_pa_image( &w.image );
    }

    return ( w.image );

    void cleanup_pathname( char **ptr )
    {
        (free)( *ptr );
    }
}


/**
 *******************************************************************************
 * Convert a text blob to a libass format that is TRIMMED up to the 1st line.
 *
 * This is one of the MAIN functions of this conversion program.
 *
 * We're going to use the libass layout engine (which does a pretty nice job),
 * but since we're using it in a non-conventional way, we have to present the
 * text in a predictable way.  If we want the text to be rendered in "order",
 * we have to combine all of the text into a single 'Dialogue:' line with the
 * "hard" line breaks identified by libass's '\N' sequence (and some other line
 * formatting options).  If we use multiple 'Dialogue:'s, the layout engine may
 * move things around when it looks for the best fit (remember, we're kind of
 * using it outside of its design specifications).
 *
 * Some of the things libass has difficulty performing are:
 *  - orphan word control,
 *  - no justified text (but who uses that nowadays - seriously),
 *  - only a single vertical margin,
 *  - no indication of clipping during rendering (has to be manually checked),
 *  - forcing a particular line height,
 *  - a few other minor items (e.g., won't hard-break a line).
 *
 * Despite the above, and this application's very, very unconventional use of
 * libass, it does a very nice job especially with automatic font substitution
 * for missing glyphs in a font (something that is missing w/other solutions).
 *
 * This performs a simple conversion of the input text to a single text blob,
 * that is, it:
 * - replaces all of the newlines with the character sequence '\N', which
 *   is the libass hard line-break character sequence;
 * - if there are multiple newlines, replaces the subsequent newlines with
 *   ' \N' so that they are not *ignored* by the libass layout engine (so
 *   that even if this changes in a future version of libass, the extra SPACE
 *   shouldn't have any affect).  For example, the sequence "\n\n\n" will
 *   be converted to '\N \N \N' (adding 5 characters total to the sequence,
 *   two of which are the "special" SPACEs).
 * - adds paragraph padding, if specified by the user using '--pad-paragraph'.
 * - TODO :: add a '--url' option.  This option will search for crowded
 *   characters and add a zero-width SPACE character after each.  I don't know
 *   if libass will break on those (have to test), but this is mainly for
 *   embedded URLs in the text (usually in TL notes) which tend to be long and
 *   will not naturally wrap (libass doesn't force-break lines).
 *
 * Footnotes -
 *   On Pango w/ImageMagick :: http://www.imagemagick.org/Usage/text/#pango
 *   "Pango is is not meant to be a whole scale text page formatting engine."
 *
 *   Even though libass is more of an annimation layout engine, libass has many
 *   interesting font features that'd be difficult to quickly implement, and it
 *   does a great job at rendering all of the glyphs in the 'Dialogue:'s Text.
 *
 *   The "definitive" Sub Station Alpha v4.00+ Script Format document defines
 *   the '\fs' style override code taking an argument; it does NOT indicate that
 *   this is an optional value.  However, in practice I've seen it used w/o an
 *   argument as a "reset" with the result that the default value / size for
 *   the corresponding 'Style:' is used (I think - I haven't tried using it as
 *   a stack - {\fsX}..{\fsY}..{\fs}..Font size X..{\fs}..Original font size).
 *   I always use the value.
 *   See --> http://moodub.free.fr/video/ass-specs.doc
 *******************************************************************************
 */
static char *make_work_text(char const *const in_text, char const *const pad_str)
{
#undef LIBASS_NL_FIX
#define LIBASS_NL_FIX ' '
#undef NL_BUMP_SZ
#define NL_BUMP_SZ (320)  /**< This value reflects the number of NLs that may
                               be in the input text file as an average for a
                               typical chapter. */
    struct {
        char const *const in_text;
        size_t            in_text_len;
        size_t            in_idx;

        char              ch;          /* This is *also* a flag / BUG fix */
        size_t            nl_idx;
        size_t            nl_max;

        unsigned short    pad_len;

        char             *out_text;
        size_t            out_idx;

        size_t            dbg_sz;      /* realloc() size, w/b optimized out */
    } w = {
        .in_text     = in_text,
        .in_text_len = 1 + strlen(in_text),
        .in_idx      = 0,

        .nl_idx      = 0,
        .nl_max      = NL_BUMP_SZ,

        .pad_len     = strlen(pad_str),

        .out_text    = NULL,
    };


    if ( w.in_text != NULL ) { // FIXME :: Do I need +1 here?
        w.out_text = realloc( w.out_text, w.dbg_sz = (1 + w.in_text_len + w.nl_max) );  /*+*/

        /**
         ***********************************************************************
         * Skip over any SPACEs to get the the FIRST _real_ line of text.
         * Note, this is the _only_ time we expect to deal with a raw '\n'.
         */
        w.in_idx = skip_non_text_tokens(w.in_text, w.in_idx, 1, 0);  /*+*/
    }

    /*
     ***************************************************************************
     * Copy / convert the input text to be used with a libass 'Dialogue:'.
     */
    while ( (w.ch = (*(w.out_text + w.out_idx) = *(w.in_text + w.in_idx))) ) {

        /*
         ***********************************************************************
         * See the FACTOID about libass and UTF8_ZERO_WIDTH_SPACE characters.
         * So, the point is that these probably should just be removed, and NOT
         * replaced with a SPACE, based on their usage in html documents.
         */
        if ( STR_MATCH == strncmp((w.in_text + w.in_idx), UTF8_ZERO_WIDTH_SPACE, sizeof (UTF8_ZERO_WIDTH_SPACE) - 1) ) {
            w.in_idx += (sizeof (UTF8_ZERO_WIDTH_SPACE) - 1);
            continue;
        }

        w.in_idx++;

        if ( '\n' == w.ch ) {
            /*
             *******************************************************************
             * This seems to be a *feature* in libass - consecutive newlines
             * seem to be reduced by 1 (they're not all "rendered").  Padding
             * a single SPACE before the '\N' sequence corrects this.
             */
            if ( (w.in_idx > 1) && '\n' == *(w.in_text + w.in_idx - 2) ) {
                w.nl_idx++;
                w.ch = LIBASS_NL_FIX;  /* ... prefix a SPACE character */
            }

            if ( w.nl_idx++ >= w.nl_max ) {
                w.nl_max += NL_BUMP_SZ;
                w.out_text = realloc( w.out_text, w.dbg_sz = (w.in_text_len + w.nl_max) );  /*+*/
            }

            if ( LIBASS_NL_FIX == w.ch ) {
                *(w.out_text + w.out_idx) = ' ';
                w.out_idx++;
            }
            *(w.out_text + w.out_idx) = '\\';
            w.out_idx++;
            *(w.out_text + w.out_idx) = 'N';

            /*
             *******************************************************************
             * Okay, we want to "detect" a sequence like this - 'A\nA'.  It's
             * a no-go if we had consecutive newlines OR the next character is
             * a '\n'.  Note that 'w.in_idx' is pointing at the NEXT character
             * which could be the '\0', but we don't care as that's NOT a '\n'.
             */
            if ( w.pad_len )
            if ( '\n' == w.ch && '\n' != *(w.in_text + w.in_idx)
                              && '\0' != *(w.in_text + w.in_idx) ) {
                w.nl_idx += w.pad_len;
                if ( w.nl_idx++ >= w.nl_max ) {
                    w.nl_max += NL_BUMP_SZ;
                    w.out_text = realloc( w.out_text, w.dbg_sz = (w.in_text_len + w.nl_max) );  /*+*/
                }
                strcpy((w.out_text + w.out_idx + 1), pad_str);
                w.out_idx += w.pad_len;
            }
        }

        w.out_idx++;
    }

    /*
     ***************************************************************************
     * Resize the text to its final size (including the '\0' string terminator).
     */
    w.out_text = realloc( w.out_text, ++w.out_idx );  /*+*/

    return ( w.out_text );
}


/**
 *******************************************************************************
 * Initially used to remove the '--pad-paragraph' option from the top of a page,
 * this function could perform other (future) edits as well.
 */
static void trim_work_text( pa_opts_t const *pa_opts, UNUSED_ARG char const *const work_text, text_segments_t *const text_segments, unsigned int template_pass )
{
    struct {
        char const *const  work_text_2;
        unsigned int const len;
    } w = {
        .work_text_2 = work_text + text_segments[ template_pass ].text_start_idx,
        .len         = strlen(pa_opts->pad_str),
    };

    if ( STR_MATCH == strncmp(w.work_text_2, pa_opts->pad_str, w.len) ) {
        text_segments[ template_pass ].text_start_idx += w.len;
        if ( template_pass ) {
            text_segments[ template_pass - 1 ].work_text_delta += w.len;
        }
    }

    return ;
}


/**
 *******************************************************************************
 */
static char *debug_work_text(char const *const work_text, char const *const dir, char const *const chapter_filename)
{

    if ( NULL != dir ) {

        char *ptr;
        char  str[ 1 + strlen(chapter_filename) ];

        strcpy(str, chapter_filename);
        asprintf(&ptr, "%s%s%s.txt", dir, PA_PATH_SEP, basename(str));

        FILE *file = fopen(ptr, "w");
        if ( NULL != file ) {
            fprintf(file, "%s\n", work_text);
            fclose(file);
        }
        (free)(ptr);
    }

    return ( (char *) work_text );
}


/**
 *******************************************************************************
 * Read and process (through /bin/sed with 'popen()') an input text file.
 *
 * The /bin/sed stream editor is really powerful / flexible (duh) and it'd be
 * insane to try to recreate its functionality (or even a simple subset) here.
 * So we'll just call it directly instead!  This is happy code, and if all of
 * the pieces are in place, should be robust enough for this application.
 *
 * This functionality is provided so the used can use automation to add
 * character attributes (and possibly other markups) to an input text file.
 * For example, adding a bold attribute to a character's name in a light novel.
 * See the example sed scripts provided with the demo for some ideas.
 *
 */
static char *process_textfile( char const *const filename, pa_opts_t *pa_opts )
{
    auto char *remove_dups(char *);
    struct {
        char         c1[ 16 ];       /* The libass primary fill colour */
        char         a1[ 16 ];       /* The libass primary alpha value */
    } w = { '\0' };


    if ( 0 == pa_opts->sed_script_files.cnt ) {
        char *str = read_textfile( filename, NULL );

        str = remove_dups( str );

        str = apply_attr_edits( str, pa_opts->attr_edits, NULL );
        write_debug_text( filename, str, pa_opts );

        return ( str );
    }

    snprintf(w.c1, sizeof (w.c1), "\\\\1c\\&H%s\\&", pa_opts->colour_1c);
    snprintf(w.a1, sizeof (w.a1), "\\\\1a\\&H%s", pa_opts->alpha_1a);

    char *cmd;
    char *s1;

    asprintf(&cmd, "%s", PA_SED_EXEC " --regexp-extended");
    for ( size_t idx = 0; idx < pa_opts->sed_script_files.cnt; idx++ ) {
        asprintf(&s1, "%s --file='%s'", cmd, pa_opts->sed_script_files.pathnames[ idx ]);
        (free)( cmd );
        cmd = s1;
    }

    /*
     ***************************************************************************
     * Put all of the "reset" regexps _after_ the scripts on the command line.
     */
    asprintf(&s1, "%s -e 's/@@1a@@/%s/g'", cmd, w.a1);
    (free)( cmd );
    cmd = s1;

    asprintf(&s1, "%s -e 's/@@1c@@/%s/g'", cmd, w.c1);
    (free)( cmd );
    cmd = s1;

    asprintf(&s1, "%s -e 's/@@face@@/\\\\fn%s/g'", cmd, pa_opts->text_face);
    (free)( cmd );
    cmd = s1;

    asprintf(&s1, "%s -e 's/@@size@@/\\\\fs%d/g'", cmd, pa_opts->text_size);
    (free)( cmd );
    cmd = s1;

    /*
     ***************************************************************************
     * And lastly, append the filename that all of these scripts are applied.
     * If the file is 'stdin', then do nothing as sed will read from 'stdin'.
     */
    if ( 0 != strcmp(filename, READ_STDIN) ) {
        asprintf(&s1, "%s \"%s\"", cmd, filename);
        (free)( cmd );
        cmd = s1;
    }

    fprintf(stderr, "%s\n", cmd);
    FILE *file = popen(cmd, "r");
    (free)( cmd );

    char *str = NULL;
    if ( NULL != file ) {
        str = read_stream( file, NULL );
        pclose(file);

        str = remove_dups( str );

        str = apply_attr_edits( str, pa_opts->attr_edits, NULL );
        write_debug_text( filename, str, pa_opts );
    }

    return ( str );


    /**
     ***************************************************************************
     ***************************************************************************
     *
     */
    char *remove_dups( char *s2 ) {
        if ( pa_opts->remove_dup_groups ) {
            s2 = remove_dup_groups(s2, pa_opts->remove_dup_groups, pa_opts->remove_dup_group_spaces, &pa_opts->details.dup_groups_found);
            if ( pa_opts->details.dup_groups_found )
            if ( pa_opts->verbose_level > VERBOSE_QUIET ) {
                fprintf(stderr, "DUPLICATE groups :: %u\n", pa_opts->details.dup_groups_found);
            }
            pa_opts->details.dup_groups_found -= pa_opts->details.dup_lines_found;   // HACK test, delete later
        }

        if ( 0 == PA_BUILT_OPTIMIZED ) {
            char *ptr, buf[ 1 + strlen(filename) ];

            strcpy(buf, filename);
            asprintf(&ptr, "." PA_PATH_SEP "DEBUG%s%s", PA_PATH_SEP, basename(buf));

            FILE *f = fopen(ptr, "w");
            if ( NULL != f ) {
                fprintf(f, "DUPLICATE lines :: %u\n", pa_opts->details.dup_lines_found);
                fprintf(f, "DUPLICATE groups :: %u\n", pa_opts->details.dup_groups_found);
                fprintf(f, "%s", s2);
                fclose(f);
            }
            (free)(ptr);
        }

        return ( s2 );
    }
}


/**
 *******************************************************************************
 *
 */
static void write_debug_text(UNUSED_ARG char const *const filename, UNUSED_ARG char const *const str, UNUSED_ARG pa_opts_t *pa_opts)
{
/*  pa_opts->debug_text */
/*     if ( 0 != strcmp(filename, READ_STDIN) ) { */

    return ;
}


/**
 *******************************************************************************
 * getconf ARG_MAX == 2,097,152.  Anything more then the user should use xargs.
 *
 * TODO :: - a realpath option for the the input text filename in comments
 *         - a web site option to include the source's website.  maybe allow
 *           multiple - e.g. original and translated sites?
 *         - an option to specify the writer(s).
 */
static rc_e O0 pa_parse_cmdline(pa_opts_t *pa_opts, int argc, char **argv)
{
    #undef WARNING
    #define WARNING "WARNING ::"

    /* TODO :: REWRITE :: I think there'a a better way using 'w.this_option_optind' ... */
    #undef ERR_IGNORE   /* A couple of edge cases don't print properly... */
    #define ERR_IGNORE( argv, ind, arg, fmt, ... )                           \
    {                                               /* Super-crazy macro */  \
        char *sp = " ";                                                      \
        char *a_ = (arg);                                                    \
        int   i_ = (ind);                                                    \
        char *r_, p_[ 1 + strlen(argv[--i_]) ];                              \
        strcpy(p_, argv[i_]);                                                \
        char *q_ = strchr(p_, '=');                                          \
        if ( NULL != q_ ) { r_ = p_; *++q_ = '\0'; } /* keep ' /=' char */   \
        else {                                                               \
            if ( '-' != argv[i_][0] ) {                                      \
                i_--;                                                        \
            }                                                                \
            else {                                                           \
                a_ = ""; sp = "";                                            \
            }                                                                \
            r_ = argv[i_];                                                   \
        }  /* otherwise, get the actual option */                            \
        fprintf(stderr, "%s option '%s%s%s' is invalid, IGNORED.\n",         \
                        (0 == w.die) ? WARNING : "ERROR ::",                 \
                        r_, (r_ == p_) ? "" : sp, a_);                       \
        fprintf(stderr, "%*s", (int) strlen(WARNING), "");                   \
        fprintf(stderr, (fmt), ##__VA_ARGS__);                               \
    }
    enum {
        ARG_NO_COMMENTS     = 2,  /* 2 is NOT arbitrary ... */
        ARG_INPUT_GLOB,
        ARG_INPUT_FILE_LIST,
        ARG_PNG_GLOB,
        ARG_PNG_GLOB_NOSORT,
        ARG_TEXT_SIZE,
        ARG_SED_SCRIPT_FILE,
        ARG_IMAGE_PREFIX,
        ARG_CHOP_PREFIX,
        ARG_CHOP_CHARS,
        ARG_ORIGINAL_DIR,
        ARG_XY_FORMAT,
        ARG_REGRESSION,
        ARG_ATTR_EDITS,
        ARG_REMOVE_DUP_GROUPS,
        ARG_REMOVE_DUP_GROUP_SPACES,
        ARG_MARGIN_BOTTOM   = 'B',
        ARG_TEXT_FACE       = 'F',
        ARG_HEADER_TEMPLATE = 'H',
        ARG_PAD_PARAGRAPH   = 'P',
        ARG_DEBUG_SED       = 'S',  // TODO
        ARG_DEBUG_TEXT      = 'T',
        ARG_VERBOSE_QUIET   = 'Q',
        ARG_VERSION         = 'V',
        ARG_DEBUG_WORK      = 'W',
        ARG_HYPHENATION     = 'Y',
        ARG_1A_ALPHA        = 'a',
        ARG_1C_COLOUR       = 'c',
        ARG_DEST_DIR        = 'd',
        ARG_FONTS_DIR       = 'f',
        ARG_INPUT_TEXT      = 'i',
        ARG_LINE_SPACING    = 'l',
        ARG_JPEG_QUALITY    = 'q',
        ARG_PNG_IMAGE_TYPE  = 'p',
        ARG_TEMPLATE_FILE   = 't',
        ARG_URL_ZWSP        = 'u',
        ARG_VERBOSE_LEVEL   = 'v',
    };
    static char const short_options[] = "a:c:d:t:f:i:l:q:puv:B:F:H:P:S:T:QVWY";
    static struct option const long_options[] = {
        { "png",             no_argument,       0, ARG_PNG_IMAGE_TYPE },
        { "image-prefix",    required_argument, 0, ARG_IMAGE_PREFIX },
        { "template",        required_argument, 0, ARG_TEMPLATE_FILE },
        { "fonts-dir",       required_argument, 0, ARG_FONTS_DIR },
        { "line-spacing",    required_argument, 0, ARG_LINE_SPACING },
        { "input-file",      required_argument, 0, ARG_INPUT_TEXT },
        { "input-glob",      required_argument, 0, ARG_INPUT_GLOB },
        { "input-file-list", required_argument, 0, ARG_INPUT_FILE_LIST },
        { "png-glob",        required_argument, 0, ARG_PNG_GLOB },
        { "png-glob-nosort", required_argument, 0, ARG_PNG_GLOB_NOSORT },
        { "header-template", required_argument, 0, ARG_HEADER_TEMPLATE },
        { "script-file",     required_argument, 0, ARG_SED_SCRIPT_FILE },
        { "jpeg-quality",    required_argument, 0, ARG_JPEG_QUALITY },
        { "pad-paragraph",   required_argument, 0, ARG_PAD_PARAGRAPH },
        { "text-size",       required_argument, 0, ARG_TEXT_SIZE },
          { "1a-alpha",      required_argument, 0, ARG_1A_ALPHA },
          { "alpha",         required_argument, 0, ARG_1A_ALPHA },
          { "1c-colour",     required_argument, 0, ARG_1C_COLOUR },
          { "colour",        required_argument, 0, ARG_1C_COLOUR },
        { "text-face",       required_argument, 0, ARG_TEXT_FACE },
        { "dest-dir",        required_argument, 0, ARG_DEST_DIR },
        { "chop-prefix",     required_argument, 0, ARG_CHOP_PREFIX },
          { "chop-chars",    required_argument, 0, ARG_CHOP_CHARS },
          { "chop-width",    required_argument, 0, ARG_CHOP_CHARS },
        { "margin-bottom",   required_argument, 0, ARG_MARGIN_BOTTOM },
        { "no-comments",     no_argument,       0, ARG_NO_COMMENTS },
        { "regression",      no_argument,       0, ARG_REGRESSION },
        { "original-dir",    required_argument, 0, ARG_ORIGINAL_DIR },
        { "xy-format",       required_argument, 0, ARG_XY_FORMAT },
        { "edits",           required_argument, 0, ARG_ATTR_EDITS },
          { "remove-dup-groups", required_argument, 0, ARG_REMOVE_DUP_GROUPS },
          { "remove-dup-group-spaces", no_argument, 0, ARG_REMOVE_DUP_GROUP_SPACES },
        { "verbose",         required_argument, 0, ARG_VERBOSE_LEVEL },
        { "hyphenation",     no_argument,       0, ARG_HYPHENATION },
        { "debug-text",      required_argument, 0, ARG_DEBUG_TEXT },
        { "debug-work",      required_argument, 0, ARG_DEBUG_WORK },
        { "url",             optional_argument, 0, ARG_URL_ZWSP },
        { "version",         no_argument,       0, ARG_VERSION },
        { NULL, 0, 0, 0 },
        // TODO :: FIXME :: TODO  Add '--help'
    };
    struct {
        union {
            int  c;
            char ch;  /* DBG display only */
        };
        int  option_index;
        int  this_option_optind;
        int  die;
    } w = {
        .option_index = 0,
        .die          = 0,  /* Set if an unrecoverable error occurred */
    };

    set_opt_defaults(pa_opts);


    /*
     **************************************************************************
     * https://stackoverflow.com/questions/1052746/getopt-does-not-parse-optional-arguments-to-parameters
     * TODO :: for --remove-dup-blocks to be an 'optional_argument' option.
     */
    while ( 1 ) {
        w.this_option_optind = optind ? optind : 1;

        w.c = getopt_long( argc, argv, short_options, long_options, &w.option_index );
        if ( -1 == w.c ) {
            break;
        }

        switch ( w.c ) {
        case ARG_REGRESSION:
            pa_opts->regression = 1;
            break;
        case ARG_NO_COMMENTS:
            pa_opts->no_comments = 1;
            break;
        case ARG_TEMPLATE_FILE:
            add_strptrary( &pa_opts->templates, optarg );
            break;
        case ARG_FONTS_DIR:
            /* FIXME :: validate the directories */
            add_strptrary( &pa_opts->font_dirs, optarg );
            break;
        case ARG_HEADER_TEMPLATE:
            if ( NULL == add_single_template( &pa_opts->header_template, optarg ) ) {
                int sv_errno = errno;
                fprintf(stderr, "ERROR :: can't load header template '%s'", optarg);
                fprintf(stderr, ", %d - '%s'\n", sv_errno, strerror(sv_errno));
                _exit(2);
            }
            if ( RC_FALSE == verify_template( pa_opts->header_template.data[0], "ddsssu", NULL ) ) {
                fprintf(stderr, "ERROR :: can't verify header template '%s'.\n", optarg);
                _exit(2);
            }
            break;
        case ARG_INPUT_TEXT:
            append_a_pathname( &pa_opts->in_chapters, optarg );
            break;
        case ARG_PNG_IMAGE_TYPE:  /* "jpg" is the default output image type */
            strcpy(pa_opts->details.image_type, "png");
            break;
        case ARG_INPUT_GLOB: {
            int  rc = add_glob_files( &pa_opts->in_chapters, optarg, PA_GLOB_SORT );
            if ( 0 == rc ) {
               w.die = 1;
               ERR_IGNORE( argv, optind, optarg, "No matching patterns were found\n" );
            }
            else if ( -1 == rc ) {
               w.die = 1;
               ERR_IGNORE( argv, optind, optarg, "An error was returned by 'glob()'\n" );
            }
            else if ( pa_opts->verbose_level >= VERBOSE_2 ) {
               fprintf(stderr, "Added %d input files from '%s' ...\n", rc, optarg);
            }
            } break;
        case ARG_INPUT_FILE_LIST: {
            int  rc = add_file_list( &pa_opts->in_chapters, optarg );
            if ( 0 == rc ) {
               ERR_IGNORE( argv, optind, optarg, "No pathnames in file were found\n" );
            }
            else if ( -1 == rc ) {
               ERR_IGNORE( argv, optind, optarg, "An I/O error occured'\n" );
            }
            else if ( pa_opts->verbose_level >= VERBOSE_2 ) {
               fprintf(stderr, "Added %d input files from '%s' ...\n", rc, optarg);
            }
            } break;
        case ARG_PNG_GLOB_NOSORT:
        case ARG_PNG_GLOB: {
            int  rc = add_glob_files( &pa_opts->in_png_list, optarg, (ARG_PNG_GLOB == w.c) );
            if ( 0 == rc ) {
               ERR_IGNORE( argv, optind, optarg, "No matching patterns were found\n" );
            }
            else if ( -1 == rc ) {
               ERR_IGNORE( argv, optind, optarg, "An error was returned by 'glob()'\n" );
            }
            } break;
        case ARG_LINE_SPACING: {
            char  *str = NULL;
            double val;
            if ( 1 == sscanf(optarg, "%lf%4mc", &val, &str) && val >= 0.0 && val <= 100.0 ) {
                pa_opts->line_spacing = val;
            } else ERR_IGNORE( argv, optind, optarg, "Value must be from 0.0 to 100.0\n" );
            (free)( str );
            } break;
        case ARG_VERBOSE_LEVEL: {
            char  str[ 4 ];
            int   val;
            /*
             *******************************************************************
             * Note, we don't bother w/checking MAX so the user can use
             *       --verbose=99 to ensure everything is logged.
             */
            if ( 1 == sscanf(optarg, "%d%3c", &val, str) && val >= VERBOSE_QUIET ) {
                pa_opts->verbose_level = val & VERBOSE_MASK;
            } else ERR_IGNORE( argv, optind, optarg, "Value should be from %d to %d.\n", VERBOSE_QUIET, VERBOSE_MAX );
            } break;
        case ARG_PAD_PARAGRAPH: {
            char  str[ 4 ];
            int   val;
            if ( 1 == sscanf(optarg, "%d%3c", &val, str) && val >= 4 && val <= 100 ) {
                pa_opts->pad_paragraph = val;
            } else ERR_IGNORE( argv, optind, optarg, "Value must be from 4 to 100.\n" );
            } break;
        case ARG_JPEG_QUALITY: {
            char  str[ 4 ];
            int   val;
            if ( 1 == sscanf(optarg, "%d%3c", &val, str) && val >= 0 && val <= 100 ) {
                pa_opts->jpeg_quality = val;
            } else ERR_IGNORE( argv, optind, optarg, "Value must be from 0 to 100 (where 100 is the best quality).\n" );
            } break;
        case ARG_MARGIN_BOTTOM: {
            char  str[ 4 ];
            int   val;
            if ( 1 == sscanf(optarg, "%d%3c", &val, str) && val >= 0 && val <= (pa_opts->png_height / 2) ) {
                pa_opts->margin_bottom = val;
            } else ERR_IGNORE( argv, optind, optarg, "Value must be from 0 to %d.\n", (pa_opts->png_height / 2) );
            } break;
        case ARG_TEXT_SIZE: {
            char  str[ 4 ];
            int   val;
            if ( 1 == sscanf(optarg, "%d%3c", &val, str) && val >= 8 && val <= 120 ) {
                pa_opts->text_size = val;
            } else ERR_IGNORE( argv, optind, optarg, "Value must be from 8 to 120.\n" );
            } break;
        case ARG_1A_ALPHA: {
            char  alpha_1a[ sizeof ("00") ];
            char  str[ 4 ];
            if ( 1 == sscanf(optarg, "%2[0-9A-Fa-f]%3c", alpha_1a, str) && 2 == strlen(alpha_1a) ) {
                for (size_t idx = 0; idx < (sizeof (alpha_1a) - 1); idx++ ) {
                    alpha_1a[ idx ] = toupper(alpha_1a[ idx ]);
                }
                strcpy(pa_opts->alpha_1a, alpha_1a);
            } else ERR_IGNORE( argv, optind, optarg, "String must be from '00' to 'FF'.\n" );
            } break;
        case ARG_1C_COLOUR: {
            char  colour_1c[ sizeof ("080808") ];
            char  str[ 4 ];
            if ( 1 == sscanf(optarg, "%6[0-9A-Fa-f]%3c", colour_1c, str) && 6 == strlen(colour_1c) ) {
                for (size_t idx = 0; idx < (sizeof (colour_1c) - 1); idx++ ) {
                    colour_1c[ idx ] = toupper(colour_1c[ idx ]);
                }
                strcpy(pa_opts->colour_1c, colour_1c);
            } else ERR_IGNORE( argv, optind, optarg, "String must be from '000000' to 'FFFFFF'.\n" );
            } break;
        case ARG_SED_SCRIPT_FILE:
            if ( RC_FALSE == access_a_pathname( &pa_opts->sed_script_files, optarg ) ) {
                fprintf(stderr, "ERROR :: can't access('%s').\n", optarg);
                _exit(2);
            }
            break;
        case ARG_IMAGE_PREFIX:
            free (pa_opts->details.image_prefix);
            pa_opts->details.image_prefix = strdup(optarg);
            break;
        case ARG_DEBUG_TEXT:
            if ( RC_TRUE == is_a_directory( optarg, W_OK ) ) {
                free (pa_opts->debug_text_dir);
                pa_opts->debug_text_dir = strdup(optarg);
            } else {
                w.die = 1;
                ERR_IGNORE( argv, optind, optarg, "Argument: is not a directory, does not exist, or write permission is denied.\n" );
            }
            break;
        case ARG_DEBUG_WORK:
            if ( RC_TRUE == is_a_directory( optarg, W_OK ) ) {
                free (pa_opts->debug_work_dir);
                pa_opts->debug_work_dir = strdup(optarg);
            } else {
                w.die = 1;
                ERR_IGNORE( argv, optind, optarg, "Argument: is not a directory, does not exist, or write permission is denied.\n" );
            }
            break;
        case ARG_DEST_DIR:
            if ( RC_TRUE == is_a_directory( optarg, W_OK ) ) {
                free (pa_opts->details.dest_dir);
                pa_opts->details.dest_dir = strdup(optarg);
            } else ERR_IGNORE( argv, optind, optarg, "Argument is not a directory, or write permission is denied.\n" );
            break;
        case ARG_CHOP_PREFIX:
            free (pa_opts->chop_prefix);  /* SPACEs are okay in this option */
            pa_opts->chop_prefix = strdup(optarg);
            break;
        case ARG_CHOP_CHARS: {
            char  str[ 4 ];
            int   val;
            if ( 1 == sscanf(optarg, "%d%3c", &val, str) && val >= 0 && val <= 12 ) {
                pa_opts->chop_chars = val;
            } else ERR_IGNORE( argv, optind, optarg, "Value must be from 0 to 12.\n" );
            } break;
        case ARG_ORIGINAL_DIR:
            if ( RC_TRUE == is_a_directory( optarg, 0 ) ) {
                free (pa_opts->original_dir);
                pa_opts->original_dir = strdup(optarg);
            } else ERR_IGNORE( argv, optind, optarg, "Argument is not a directory, or access is denied.\n" );
            break;
        case ARG_TEXT_FACE: {
            char const *p;
            if ( '\0' != *(p = zap_isspace( optarg )) ) {
                free (pa_opts->text_face);
                pa_opts->text_face = strdup(p);
            } else ERR_IGNORE( argv, optind, optarg, "Font name is an EMPTY string.\n" );
            } break;

        case ARG_XY_FORMAT: {
            pa_opts->xy_format = XY_PAGE_XofY;
            enum { OPT_XY_PAGE_XofY = 0, OPT_XY_DECIMAL, OPT_XY_ROMAN };
            static char *const token[] = {
                [ OPT_XY_PAGE_XofY ] = "XofY",
                [ OPT_XY_DECIMAL ]   = "decimal",
                [ OPT_XY_ROMAN ]     = "roman",
                NULL
            };
            int   errfnd = 0;  /* unused, for now */
            char *value;
            char *subopts = optarg;
            while ( ('\0' != *subopts) && 0 == errfnd ) {
                switch ( getsubopt(&subopts, token, &value) ) {
                case OPT_XY_PAGE_XofY:
                    pa_opts->xy_format = XY_PAGE_XofY;
                    break;
                case OPT_XY_DECIMAL:
                    pa_opts->xy_format = XY_DECIMAL;
                    break;
                case OPT_XY_ROMAN:
                    pa_opts->xy_format = XY_ROMAN;
                    break;
                default:
                    ERR_IGNORE( argv, optind, optarg, "Unknown sub-option for argument.\n" );
                }
            }
            } break;

        case ARG_ATTR_EDITS: {
            pa_opts->attr_edits = 0;
            enum { OPT_AE_ALL = 0, OPT_AE_TWO_SPACES, OPT_AE_DOUBLE_DASH, OPT_AE_URBAN_WORDS,
                   OPT_NO_ALL,     OPT_NO_TWO_SPACES, OPT_NO_DOUBLE_DASH, OPT_NO_URBAN_WORDS,
                   };
            static char *const token[] = {
                [ OPT_AE_ALL         ] = "all",
                [ OPT_AE_TWO_SPACES  ] = "two-spaces",
                [ OPT_AE_DOUBLE_DASH ] = "double-dash",
                [ OPT_AE_URBAN_WORDS ] = "urban-words",

                [ OPT_NO_ALL         ] = "none",
                [ OPT_NO_TWO_SPACES  ] = "!two-spaces",
                [ OPT_NO_DOUBLE_DASH ] = "!double-dash",
                [ OPT_NO_URBAN_WORDS ] = "!urban-words",
                NULL
            };
            static unsigned short ae_vals[] = {
                    AE_ALL,
                      AE_TWO_SPACES,  AE_DOUBLE_DASH,  AE_URBAN_WORDS,
                    0,
                     ~AE_TWO_SPACES, ~AE_DOUBLE_DASH, ~AE_URBAN_WORDS,
                };
            int   errfnd = 0;  /* unused, for now */
            char *value;
            char *subopts = optarg;
            while ( ('\0' != *subopts) && 0 == errfnd ) {
                int subopt = getsubopt(&subopts, token, &value);
                switch ( subopt ) {
                case OPT_AE_ALL:
                case OPT_AE_TWO_SPACES:
                case OPT_AE_DOUBLE_DASH:
                case OPT_AE_URBAN_WORDS:
                    pa_opts->attr_edits |= ae_vals[ subopt ];
                    break;
                case OPT_NO_ALL:
                case OPT_NO_TWO_SPACES:
                case OPT_NO_DOUBLE_DASH:
                case OPT_NO_URBAN_WORDS:
                    pa_opts->attr_edits &= ae_vals[ subopt ];
                    break;
                default:
                    ERR_IGNORE( argv, optind, optarg, "Unknown sub-option for argument.\n" );
                }
            }
            } break;

        case ARG_URL_ZWSP:
                /**************************************************************
                 * TODO :: The optional argument is a list of characters to
                 *         add a "zero-width" SPACE _after_.  Note too, that
                 *         libass doesn't fully support the ZWSP character,
                 *         so what we actually insert is '{\fs1} {\fs}'.
                 *         The default character is '/'.
                 */
            pa_opts->url_zwsp = 1;
            break;
        case ARG_REMOVE_DUP_GROUP_SPACES:
            pa_opts->remove_dup_group_spaces = 1;
            break;
        case ARG_REMOVE_DUP_GROUPS: {
            char  str[ 4 ];
            int   val;
            if ( NULL != optarg ) {
                if ( 1 == sscanf(optarg, "%d%3c", &val, str) && val > 0 && val <= 12 ) {
                    pa_opts->remove_dup_groups = val;
                } else ERR_IGNORE( argv, optind, optarg, "Value must be from 1 to 12 (inclusive).\n" );
            }
            else pa_opts->remove_dup_groups = 3;  // TODO :: "default" not implemented.
            } break;
        case ARG_VERBOSE_QUIET:
            pa_opts->verbose_level = VERBOSE_QUIET;
            break;
        case ARG_VERSION:
            fprintf(stderr, "%s%s\n", pa_opts->png_Software,
                   PA_BUILT_OPTIMIZED ? " (gcc optimizing compilation)" : " (unoptimized)"
                );
            _exit(0);
        default:
            fprintf(stderr, "ERROR - option '%s' NOT handled in switch.\n", argv[ optind - 1 ]);
            _exit(2);
        }

    }

    if ( 1 == w.die ) {
        fprintf(stderr, "Exiting due to unrecoverable command line error(s)\n");
        _exit( 1 );
    }
    post_opt_defaults( pa_opts );
    pa_verify_opts( pa_opts );


    return ( 0 );
}


/**
 *******************************************************************************
 */
static void set_opt_defaults(pa_opts_t *pa_opts)
{

    /*
     ***************************************************************************
     * These are *probably* not needed; thinking I'll get them from PNG (iow,
     * is there a value in using different values than indicated by the PNG)?
     */
    pa_opts->png_width = 1920;
    pa_opts->png_height = 1080;

    pa_opts->jpeg_quality = 95;  /* Set to write a high quality JPEG file */
    pa_opts->line_spacing = 0.0;

    pa_opts->verbose_level = VERBOSE_1;
    pa_opts->xy_format = XY_PAGE_XofY;

    pa_opts->text_size = 40;  /* text_size == 0 causes libass infinite loop */
    pa_opts->text_face = strdup("Arial");
    strcpy(pa_opts->alpha_1a, "00");       /* Default :: Opaque text */
    strcpy(pa_opts->colour_1c, "080808");  /* Default :: Almost black text */

    pa_opts->details.image_prefix = strdup("p");

    pa_opts->details.dest_dir = NULL;      /* USER MUST PROVIDE */
    strcpy(pa_opts->details.image_type, "jpg");

    pa_opts->margin_bottom = PA_UNSET_VALUE;
    pa_opts->pixel_fudge   = PA_UNSET_VALUE;

    pa_opts->chop_prefix = NULL;
    pa_opts->chop_chars = 0;
    pa_opts->original_dir = NULL;

    pa_opts->png_Software = get_Software();

    return ;
}


/**
 *******************************************************************************
 * Set post-option defaults that depend on other command line options.
 */
static void O3 post_opt_defaults(pa_opts_t *pa_opts)
{

    if ( PA_UNSET_VALUE == pa_opts->margin_bottom ) {
        double font_size = pa_opts->text_size;
        double margin_bottom = font_size * 0.2;
        pa_opts->margin_bottom = margin_bottom;
    }

    if ( pa_opts->verbose_level > VERBOSE_QUIET )
    if ( NULL != pa_opts->original_dir ) {
        fprintf(stderr, "Getting meta-data from matching images in '%s'.\n", pa_opts->original_dir);
    }

    if ( PA_UNSET_VALUE == pa_opts->pixel_fudge ) {
        pa_opts->pixel_fudge = (pa_opts->text_size * 100) / 1000;
        if ( pa_opts->pixel_fudge < ONE_PIXELs_FUDGE ) {
            pa_opts->pixel_fudge = ONE_PIXELs_FUDGE;
        }
    }

    return ;
}


/**
 *******************************************************************************
 */
static void cleanup_pa_opts( pa_opts_t **p_pa_opts )
{
    pa_opts_t *pa_opts = *p_pa_opts;

    cleanup_strptrary( &pa_opts->templates );
    cleanup_strptrary( &pa_opts->in_chapters );
    cleanup_strptrary( &pa_opts->font_dirs );
    cleanup_strptrary( &pa_opts->in_png_list );
    cleanup_strptrary( &pa_opts->header_template );
    cleanup_strptrary( &pa_opts->sed_script_files );

    cleanup_details  ( &pa_opts->details );

    (free)( (void *) pa_opts->chop_prefix );
    (free)( (void *) pa_opts->original_dir );
    (free)( (void *) pa_opts->text_face );

    (free)( (void *) pa_opts->debug_text_dir );
    (free)( (void *) pa_opts->debug_work_dir );

    free( *p_pa_opts );

    return ;
}


/**
 *******************************************************************************
 */
static void cleanup_details( details_t *details )
{

    free( details->png_Title );
    free( details->dest_dir );
    free( details->image_prefix );

    return ;
}


/**
 *******************************************************************************
 * Gawd - I can't think of an easier way to solve this.  I'm trying to render
 *        as much text as possible before splitting the string.  There doesn't
 *        seem to be a trivial way to do this...
 *
 * We want to incrementally build a 'Events:Text' string that will change each
 * time it is rendered by 'ass_render_frame()'.  For each successive render,
 * if the current render matches the previous render, then we know the previous
 * render is the maximum text that can be consumed for that template.
 *
 * For the previous render, we only need to save the position in 'work_text'
 * (there may be other things like character attributes, but that complexity
 * can be added at a later date).
 *
 * The 'work_text' blob has been pre-processed by 'make_work_text()' such
 * that its initial state is TEXT token, and we call 'skip_non_text_tokens()'
 * after each template iteration to ensure that the initial state is _always_
 * TEXT token for the remaining text.
 *
 * The libass documentation is unclear (even if I look at the source that's
 * no assurance that it wouldn't change between versions) about the lifetime
 * of an ASS_Image from ass_render_frame().
 *
 * TODO :: We might split in the middle of a font attribute (bold, italic, etc.)
 *         so we need a way to handle that (s/b pretty rare, but it can happen).
 */
static void O0 apply_template_complex( pa_image_t *pa_image, pa_opts_t *pa_opts, char const *const template, char *const work_text, text_segments_t *const text_segments )
{
#undef IS_FOLD_PASS_1
#define IS_FOLD_PASS_1  ( FOLD_PASS_1 == pa_opts->fold_pass )
#undef IS_FOLD_PASS_2
#define IS_FOLD_PASS_2  ( FOLD_PASS_2 == pa_opts->fold_pass )
    struct {
        char        *ass_text;
        size_t       ass_text_len;

        char const  *const template;
        char const  *work_text_2;
        unsigned int work_text_idx;
        unsigned int work_text_delta;
        size_t       good_work_text_idx;  /* Start of last token, not SPACE */

        ASS_Image   *img_curr;  /* returned from 'ass_render_frame()' */
        pa_ass_t     img_prev;  /* saved fields from last ASS_Image in array */
        int          clipped;   /* Set if the last ASS_Image was clipped. */

        int          width;
        int          height;
        int          text_size;

        size_t       advance_work_text_idx;  /* DBG helper */
        char        *ptr;                    /* DBG helper, gcc will optimize out */
        char        *ptr1;                   /* DBG helper, gcc will optimize out */
        short        dbg_pieces;
        ass_cmpr_e   rc;
    } w = {
        .img_curr      = NULL,
        .template      = template,

        .work_text_idx = 0,                /*+*/
        .width         = get_image_width( pa_image ),
        .height        = get_image_height( pa_image ),
        .text_size     = pa_opts->text_size,
        .clipped       = 0,

        .img_prev.pieces       = 0,
        .img_prev.piece_starts = NULL,
    };


    ASS_Library *ass_library = ass_library_init();
    add_font_search_dirs( ass_library, &pa_opts->font_dirs );
    ass_set_message_cb( ass_library, libass_msg_callback, pa_opts );

    ASS_Renderer *ass_renderer = ass_renderer_init( ass_library );

    ass_set_frame_size( ass_renderer, w.width, w.height );
    ass_set_fonts( ass_renderer, NULL, "sans-serif", 1, NULL, 1 );

    if ( pa_opts->line_spacing > 0.0 ) {
        ass_set_line_spacing( ass_renderer, pa_opts->line_spacing );  /* Works as expected :) */
    }

    /*
     ***************************************************************************
     * This is the FIRST PASS of folding / fitting the text onto one or more
     * PNG images.  We'll start at the "top" of the page, so we'll skip any
     * SPACE above the first printable string.
     */
    w.work_text_2 = work_text + text_segments->text_start_idx;

    if ( IS_FOLD_PASS_1 )
    while ( '\0' != *(w.work_text_2 + w.work_text_idx) ) {

        w.img_prev.token_start_idx = w.work_text_idx;

        //-g BREAK_STR((w.work_text_2 + w.work_text_idx), "If yo face");

        /*
         ***********************************************************************
         * See if we're at the start of a libass attribute.  We make a few
         * assumptions: attributes are not nestable and the characters '{}'
         * should NOT appear in an attribute, except the string "}" which
         * is used to close the attribute.
         */
        /**
         * We intentionally do NOT guarantee the case where an attribute is
         * applied over multiple words.  The complexity of managing a page
         * split in the middle of a phrase is just too much for a hobby.
         * However, there is a simple solution by simply applying the same
         * attribute for each word in the sed script.  For example -->
         *
         *  s/Earl Pendragon/{\b1}Earl Pendragon{\b0}/     <<== error-prone
         *  s/Earl Pendragon/{\b1}Earl{\b0} {\b1}Pendragon{\b0}/  <<== okay
         *
         * So now if Earl Pendragon is split across pages, both words will
         * carry the desired character attribute(s).  Another possible
         * solution is to replace the SPACE with a non-breaking SPACE so
         * that the phrase is treated as a single word.  For example -->
         *
         *  s/Earl Pendragon/{\b1}Earl\xa0Pendragon{\b0}/  <<== single word
         */
        next_char:
        w.advance_work_text_idx = w.work_text_idx;
        do {
            if ( w.advance_work_text_idx == skip_non_text_tokens(w.work_text_2, w.work_text_idx, 1, 0) ) {
                if ( 0 == strncmp((w.work_text_2 + w.work_text_idx), ASS_ATTRIBUTE_START, sizeof (ASS_ATTRIBUTE_START) - 1) ) {
                    w.work_text_idx += (sizeof (ASS_ATTRIBUTE_START) - 1);
                    w.ptr = strstr((w.work_text_2 + w.work_text_idx), ASS_ATTRIBUTE_END);
                    if ( NULL != w.ptr ) {
                        w.work_text_idx += w.ptr - (w.work_text_2 + w.work_text_idx);
                        w.work_text_idx += (sizeof (ASS_ATTRIBUTE_END) - 1);
                    }
                }
                else {
                    w.work_text_idx++;
                }
                goto next_char;
            }
        } while ( 0 );

        /*
         ***********************************************************************
         * Include any trailing white SPACE after the current token.
         */
        w.work_text_idx = skip_non_text_tokens(w.work_text_2, w.work_text_idx, 1, 0);

        w.ass_text_len = asprintf(&w.ass_text,
                                   w.template,
                                   w.width,
                                   w.height,
                                   pa_opts->text_face,
                                   w.text_size,
                                   pa_opts->colour_1c,
                                   "",  /* NOTE :: should persist for whole template */
                                   w.work_text_idx,
                                   w.work_text_2);

        w.ptr  = w.ass_text + strlen(w.template);       /* DBG */
        w.ptr1 = w.ass_text + strlen(w.ass_text) - 64;  /* DBG */

        ASS_Track *ass_track = ass_read_memory( ass_library, w.ass_text, w.ass_text_len, NULL );
        w.img_curr = ass_render_frame( ass_renderer, ass_track, 0LL, NULL );

        w.dbg_pieces = COUNT_ASS_Images(w.img_curr);    /* DBG */
        w.rc = cmpr_last_ASS_Image( &w.img_prev, w.img_curr, w.height - pa_opts->margin_bottom, pa_opts->pixel_fudge, w.work_text_2 );

        ass_free_track(ass_track);
        free ( w.ass_text );

        /*
         ***************************************************************************
         * Okay, this time for sure.  There's no bullet proof way to handle this
         * as I've done several buggy previous versions.  The basic problem is to
         * detect when an added token is really off the page, or wholly consists of
         * non-rendering characters.  The idea of watching the count of ASS_Images
         * is close, except for this case -- I can't tell the difference between
         * off image and not rendered for the last token added to the 'ass_track'.
         *
         * Trying to match up X and Y coordinates, width and height makes for a
         * complex solution that may still have some issues given how libass
         * renders its ASS_Images.
         *
         * So, the idea is this - if I see the ASS_TRY_SANDBOX_TOKEN result code,
         * I'll perform a sample render with just that last token in a "sandbox."
         * If it still produces no ASS_Image(s), then I'll _know_ that it's all
         * non-rendering characters and will continue trudging along.  If it does
         * render, then it's truly off the image and we're done with the template.
         *
         * I don't think this is fool-proof, but the results are much better.
         */
        if ( ASS_TRY_SANDBOX_TOKEN == w.rc ) {
            w.ass_text_len = asprintf(&w.ass_text,
                                       w.template,
                                       w.width,
                                       w.height,
                                       pa_opts->text_face,
                                       w.text_size,
                                       pa_opts->colour_1c,
                                       "",
                                       w.work_text_idx - w.good_work_text_idx,
                                       w.work_text_2 + w.good_work_text_idx);

            w.ptr  = w.ass_text + strlen(w.template);       /* DBG */
            w.ptr1 = w.ass_text + strlen(w.ass_text) - 64;  /* DBG */

            ass_track = ass_read_memory( ass_library, w.ass_text, w.ass_text_len, NULL );
            w.img_curr = ass_render_frame( ass_renderer, ass_track, 0LL, NULL );

            ass_free_track(ass_track);
            free ( w.ass_text );

            if ( NULL == w.img_curr ) {
                w.rc = ASS_IN_IMAGE;
            }
        }
        else if ( ASS_DROPPED == w.rc ) {
            /* FIXME :: Still need to do something ... TODO */
            fprintf(stderr, "OOPS -- ASS_DROPPED returned!!!!!!!!\n");
        }

        /**
         ***************************************************************************
         ***************************************************************************
         * See if we've consumed the maximum amount of text for this template,
         * and return the index for how far we advanced.  There are 3 cases:
         *
         * - the last segment did not render because it's completely off of the
         *   PNG's bitmap field -- we'll blend the whole ASS_Image list;
         * - the last segment in the ASS_Image list was clipped.  We'll blend
         *   all but the last ASS_Image in the list, and continue on next page;
         * - we exhausted the entire string (indicated by reaching the '\0'),
         *   we'll blend the whole ASS_Image list.
         * NOTE - The test order in this if(...) is important!  See 'git log'.
         */
        if (   (ASS_TRY_SANDBOX_TOKEN == w.rc)                             /*+*/
            || (ASS_LAST_IS_CLIPPED == w.rc
                 && (w.clipped = 1)
                 && (w.good_work_text_idx = w.img_prev.token_start_idx))   /*+*/
            || ('\0' == *(w.work_text_2 + w.work_text_idx)
                 && (w.good_work_text_idx = w.work_text_idx))              /*+*/
           ) {

            fprintf(stderr, "PASS #1 :: %lu bytes comsumed ...\n", w.good_work_text_idx);

            text_segments->work_text_delta = w.good_work_text_idx;
            text_segments->clipped = w.clipped;
            text_segments->pieces = w.img_prev.pieces;

            free(w.img_prev.piece_starts);
            break;
        }
        else if ( ASS_NO_IMAGE_ERROR == w.rc ) {
            break_me("Detected :: ASS_NO_IMAGE_ERROR ...");
        }

        w.good_work_text_idx = w.work_text_idx;

        //-g BREAK_STR((w.work_text_2 + w.work_text_idx), "[Magic] and [Skill] are blank");
    }

    else {
        /*
         ***************************************************************************
         * Take advantage of all of the work PASS 1 completed, and
         * simply render and blend the appropriate chunk of text!
         */
        w.work_text_delta = text_segments->work_text_delta;
        w.ass_text_len = asprintf(&w.ass_text,
                                   w.template,
                                   w.width,
                                   w.height,
                                   pa_opts->text_face,
                                   w.text_size,
                                   pa_opts->colour_1c,
                                   "",
                                   w.work_text_delta,
                                   w.work_text_2);

        w.ptr  = w.ass_text + strlen(w.template);       /* DBG */
        w.ptr1 = w.ass_text + strlen(w.ass_text) - 32;  /* DBG */

        ASS_Track *ass_track = ass_read_memory( ass_library, w.ass_text, w.ass_text_len, NULL );
        w.img_curr = ass_render_frame( ass_renderer, ass_track, 0LL, NULL );
        free(w.ass_text);

        int  pieces = blend_pa_image( pa_image, w.img_curr, 0 );
        ass_free_track(ass_track);

        fprintf(stderr, "PASS #2 :: %u bytes for %d PIECES%s.\n", text_segments->work_text_delta,
                        pieces, text_segments->clipped ? " (CLIPPED)" : "");
        fprintf(stderr, "%.*s\n", (int) text_segments->work_text_delta, w.work_text_2);  fflush( stderr );
    }

    ass_renderer_done( ass_renderer );
    ass_library_done( ass_library );

    return ;
}


/**
 *******************************************************************************
 * Render a simple template, usually for the header for the chapter name, page
 * number, and other meta-data.
 */
static size_t apply_template_simple( pa_image_t *pa_image, pa_opts_t *pa_opts )
{
    struct {
        char const  *page_X_of_Y;
        char        *filename;
        char        *png_Title;
        char        *ass_text;
        size_t       ass_text_len;

        int          width;
        int          height;
        unsigned int sequence;
    } w = {
        .width     = get_image_width( pa_image ),
        .height    = get_image_height( pa_image ),
        .sequence  = pa_opts->details.global_image_sequence_number,
    };
    char pathname[ strlen(pa_opts->details.chapter_filename) + 1 ];

    strcpy(pathname, pa_opts->details.chapter_filename);
    asprintf(&w.filename, "\"%s\"", chop_prefix( basename(pathname), pa_opts->chop_prefix, pa_opts->chop_chars ));

    asprintf(&w.png_Title, UTF8_LEFT_CORNER_BRACKET "%s" UTF8_RIGHT_CORNER_BRACKET, pa_opts->details.png_Title);
    remove_attributes( w.png_Title );

    w.page_X_of_Y = build_page_X_of_Y(pa_opts->details.chapter_image_number, pa_opts->details.chapter_images, pa_opts->xy_format);

    w.ass_text_len = asprintf(&w.ass_text,
                               pa_opts->header_template.data[0],
                               w.width,
                               w.height,
                               w.filename,
                               w.png_Title,
                               w.page_X_of_Y,
                               w.sequence
                             );

    ASS_Library *ass_library = ass_library_init();
    add_font_search_dirs( ass_library, &pa_opts->font_dirs );
    ass_set_message_cb( ass_library, libass_msg_callback, pa_opts );

    ASS_Renderer *ass_renderer = ass_renderer_init( ass_library );

    ass_set_frame_size( ass_renderer, w.width, w.height );
    ass_set_fonts( ass_renderer, NULL, "sans-serif", 1, NULL, 1 );

    /**
     ***************************************************************************
     * The filename and chapter title occupy the same space, so we use the time
     * to tell libass which we want to render for _this_ image.  Probably 100
     * other ways to do this; this works and seemed the simplest to implement.
     */
    long long now = (1 == pa_opts->details.chapter_image_number) ? 0 : 1000;
    ASS_Track *ass_track = ass_read_memory( ass_library, w.ass_text, w.ass_text_len, NULL );
    ASS_Image *img_curr  = ass_render_frame( ass_renderer, ass_track, now, NULL );

    int  pieces = blend_pa_image( pa_image, img_curr, 0 );
    ass_free_track(ass_track);

    fprintf(stderr, "HEADING :: %d PIECES.\n", pieces);
    fflush( stderr );

    ass_renderer_done( ass_renderer );
    ass_library_done( ass_library );

    (free)( w.ass_text );
    (free)( w.filename );
    (free)( w.png_Title );
    (free)( (void *) w.page_X_of_Y );

    return ( 0 );
}


/**
 *******************************************************************************
 */
static void libass_msg_callback(int level, const char *fmt, va_list args, void *vp)
{
     pa_opts_t *pa_opts = (pa_opts_t *) vp;

     if ( pa_opts->verbose_level >= VERBOSE_MAX )
     if ( FOLD_PASS_2 == pa_opts->fold_pass )
     if (   6 == level
         || 4 == level
         || 2 == level ) {
  //    fprintf(stderr, "LEVEL :: %d :: ", level);
        fprintf(stderr, "[ass] ");
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
     }
}


/**
 *******************************************************************************
 * Skip to the next TOKEN.
 * TODO :: Testing for a UTF8_ZERO_WIDTH_SPACE character here would mess up how
 *         the '--pad-paragraph' argument is implemented (since it must "stay"
 *         with the line) and maybe I should filter them out in pngass.c?
 */
static size_t O0 skip_non_text_tokens(char const *const in_text, size_t in_idx, int skip_spaces, int skip_tags)
{
    static char const *const IGNORE_STRS[] = {
       /* "\N"  */  ASS_HARD_BREAK,         \
       /* " \N" */  ASS_SPACE_HARD_BREAK,   \
                    BOM_SEQUENCE,           \
                    UTF8_IDEOGRAPHIC_SPACE, \
    };
#undef IGNORE_STRS_SZ
#define IGNORE_STRS_SZ (sizeof (IGNORE_STRS) / sizeof (IGNORE_STRS[0]))

    struct {
        char const *const in_text;
        char const       *ptr;
        char const       *p;
        char const       *ignore;
        size_t            in_idx;
    } w = {
        .in_text = in_text,
        .in_idx  = in_idx,
    };


    if ( NULL != w.in_text ) {
      label:
        w.ptr = (w.in_text + w.in_idx);

        if ( '\0' != *w.ptr ) {
            if ( skip_spaces && NULL != strchr(" \n", *w.ptr) ) {
                w.in_idx++;
                goto label;
            }
            for ( unsigned int ii = 0; ii < IGNORE_STRS_SZ; ii++ ) {
                w.ignore = IGNORE_STRS[ii];
                if ( STR_MATCH == strncmp(w.ptr, w.ignore, strlen(w.ignore)) ) {
                    w.in_idx += strlen(IGNORE_STRS[ii]);
                    goto label;
                }
            }

            /*
             ******************************************************************
             * If asked, skip libass attribute tags --> {\b1}, {\fnArial}, etc.
             * A tag can have embedded SPACEs, but (should never) have a '\n'.
             * TODO / BUG :: While this will work for the well-behaved cases,
             *               we should limit the strstr() to the current line,
             *               not the whole text as it is currently implemented.
             *               It's probably harmless in its current form ...
             */
            if ( skip_tags )
            if ( STR_MATCH == strncmp(w.ptr, ASS_ATTRIBUTE_START, sizeof (ASS_ATTRIBUTE_START) - 1) ) {
                w.p = strstr((w.ptr + sizeof (ASS_ATTRIBUTE_START) - 1), ASS_ATTRIBUTE_END);
                if ( NULL != w.p ) {
                    w.in_idx += (w.p - w.ptr);
                    w.in_idx += (sizeof (ASS_ATTRIBUTE_END) - 1);
                    goto label;
                }
            }
        }
    }

    return ( w.in_idx );
}


/**
 ******************************************************************************
 * Compare the last segment of the current ASS_Image with the last segment of
 * the previous ASS_Image.
 *
 * So, after a lot of iterations on how to do this, it boils down to a brute
 * force solution returning one of two possible conditions:
 *
 *  1. the last call to 'ass_render_frame()' produced no addtional ASS_Images,
 *     or
 *  2. one or more of the last ASS_Image(s) is clipped.
 *
 * It's brute force because I "capture" token start positions (a token is a
 * sequence of characters that doesn't include a break character - SPACE or
 * NL), there is no easy way to know where a (folded) line starts in the text.
 * In practice, if the text did NOT have any character attributes applied, it
 * could be easily deduced / calculated by watching the coordinates of the
 * ASS_Images change through the list.  Since we want to allow the flexability
 * of adding attributes in the text stream, this is NOT reliable as libass will
 * overlap images and will move previous ASS_Images (in the list) around so
 * that the result is visually correct when rendered in the order that they
 * appear in the list of ASS_Images.
 *
 * See EXAMPLEs/snapshot-02-PITA.png for a picture is worth a thousand words.
 *
 * The brute force part is having to look at every ASS_Image in the list from
 * the last to the first to see if it's clipped.
 ******************************************************************************
 */
static ass_cmpr_e O3 cmpr_last_ASS_Image( pa_ass_t *const prv, ASS_Image *i_cur, int height, short pixel_fudge, char const *const work_text_2 )
{
    struct {
        ASS_Image      *cur;
        int const       height;
        short const     pixel_fudge;
        short  pieces;
        short  ii;
        ass_cmpr_e             rc;

        char const *ptr; // DBG
    } w = {
        .height      = height,
        .pixel_fudge = pixel_fudge,
        .pieces      = 0,
        .cur         = i_cur,
        .rc          = ASS_IN_IMAGE,
    };


    if ( NULL != w.cur )
    do {
        /*
         **********************************************************************
         * Count of the number of pieces in the current ASS_Image list.
         */
        while ( w.pieces++, NULL != w.cur->next ) {
            w.cur = w.cur->next;
        }

        /*
         **********************************************************************
         * Let's prune the simplest case first - did we add any ASS_Images?
         *  YES -- continue to see if the new ASS_Image is slipped;
         *  NO  -- if this ASS_Image(w) == previous(w) the last token _set_ is
         *         completely off the image.  Otherwise, it's just a _wider_
         *         ASS_Image on the line at about the same 'dst_y' position
         *         (there might be jitter).
         * TODO :: review comments in this function
         */
        if ( w.pieces == prv->pieces ) {
            if ( w.cur->w == prv->w ) {
                w.rc = ASS_TRY_SANDBOX_TOKEN;
            }
            prv->w = w.cur->w;
            break;
        }
        prv->w = w.cur->w;

        /*
         **********************************************************************
         * We'll see this for a libass comment that wasn't escaped in the text.
         */
        if ( w.pieces < prv->pieces ) {
            w.rc = ASS_DROPPED;
            break;
        }

        /*
         **********************************************************************
         * Otherwise copy the ASS_Image list into an array.  Even for a fairly
         * complex page, this shouldn't be a large value.  Usually the number
         * of ASS_Images corresponds to the # of _rendered_ lines built by the
         * template, with each line being one ASS_Image.  So, 128 ASS_Images
         * is 1024 bytes of stack space.  A template with the font size 40 is
         * usually under 24 ASS_Images.
         */
        ASS_Image  *curs[ w.pieces ];
        w.cur = i_cur;
        for ( w.ii = 0; w.ii < w.pieces; w.ii++ ) {
            curs[ w.ii ] = w.cur;
            w.cur = w.cur->next;
        }

        /*
         **********************************************************************
         * This usually is a 1x loop, but ya never know w/libass...
         *
         * Since we work on "word" boundaries, it's possible to have a complex
         * word, for example --- '{\i1}H{\b1}ell{\i0}o{\b0}' is a valid word
         * which produces multiple (three in this case) ASS_Images.
         */
        prv->piece_starts = realloc(prv->piece_starts, w.pieces * sizeof (unsigned int));
        for ( w.ii = prv->pieces; w.ii < w.pieces; w.ii++ ) {
            prv->piece_starts[ w.ii ] = prv->token_start_idx;
        }

        /*
         **********************************************************************
         * Okay, start with the newest piece(s) to see if any were clipped.
         * If any piece of the last token clipped, we'll walk the list back.
         */
        for ( w.ii = prv->pieces; w.ii < w.pieces; w.ii++ ) {
            if ( (curs[ w.ii ]->dst_y + curs[ w.ii ]->h) >= w.height ) {
                break;
            }
        }

        /*
         **********************************************************************
         * If none of the new ASS_Images clipped, we're done.
         */
        if ( w.ii == w.pieces ) {
            prv->pieces = w.pieces;
            w.rc = ASS_IN_IMAGE;  /* (redundant) */
            break;
        }

        /*
         **********************************************************************
         * Okay, now walk back through the list looking for clipped ASS_Images.
         * This is where we have to use the "fudge" factor for comparison.
         * Also, there may be "gaps" so we have to walk the entire list (it's
         * easier than trying to figure out if we can stop the check along the
         * way).
         */
        w.ii = prv->pieces;
        while ( w.ii-- ) {
            if ( (curs[ w.ii ]->dst_y + curs[ w.ii ]->h) >= (w.height - w.pixel_fudge) ) {

                prv->token_start_idx = prv->piece_starts[ w.ii ];
                prv->pieces = 1 + w.ii;
                w.ptr = work_text_2 + prv->token_start_idx;   //-DBG DBG DBG
            }
        }
        w.rc = ASS_LAST_IS_CLIPPED;
    } while ( 0 );
    else w.rc = ASS_NO_IMAGE_ERROR;

    return ( w.rc );
}


/**
 *******************************************************************************
 * Set font search directories for libass.
 */
static size_t add_font_search_dirs( ASS_Library *ass_library, strptrary_t *font_dirs )
{
    size_t idx = 0;

    for ( ; idx < font_dirs->cnt; idx++ ) {
        ass_set_fonts_dir(ass_library, font_dirs->pathnames[ idx ] );
    }

    return ( idx );
}


/**
 *******************************************************************************
 * Verify a text template.
 *
 * Perform a rudimentary inspection of a text template.  We only verify that
 * it has the correct number and type of printf tags, but nothing beyond that.
 * It's really just designed to catch fat-fingered typos in the template file.
 *
 * Note, a typical template should NOT have a random / stray '%' character.
 */
static int O3 verify_template( char const *const template, char const *types, char const *const final_type )
{
    struct {
        rc_e        rc;
        char const *ptr;
    } w = {
        .rc  = RC_TRUE,
        .ptr = template,
    };


    while ( *types ) {
        w.ptr = strchr( w.ptr, '%' );
        if ( NULL == w.ptr ) {
            w.rc = RC_FALSE;
            break;
        }
        if ( *types != *(++w.ptr) ) {
            w.rc = RC_FALSE;
            break;
        }
        ++types;
    }

    /*
     ***************************************************************************
     * Check: is there a final format START in the string;
     *        does it match the 'final_type'; and
     *        there should be no additional format STARTs in the string.
     */
    if ( RC_TRUE == w.rc )  /* (guard) */
    if ( (NULL != final_type
            && (NULL == (w.ptr = strchr( w.ptr, '%' )) || STR_MATCH != strncmp(++w.ptr, final_type, strlen(final_type))))
         || NULL != strchr( w.ptr, '%' )
       ) {

        w.rc = RC_FALSE;
    }

    return ( w.rc );
}


/**
 *******************************************************************************
 * Get the chapter title, which is assumed to be the first real line of text.
 *
 * This is part of the automated process for rendering a chapter.
 *
 * Note that the title can contain libass attribute / layout tags (which is a
 * good thing for font selection, colour, bold, italics - things like that),
 * but layout tags can / will probably mess things up.
 *
 * The returned string is trimmed at the end.
 */
static char const * O3 get_chapter_title( char *const ptr, size_t max )
{
    char buf[ max ];

    /*
     ***************************************************************************
     * Setup the starting point ('p') for the reverse search below.
     */
    size_t ii = skip_non_text_tokens(ptr, 0, 1, 0);       /*+*/
    snprintf(buf, sizeof (buf), "%s", &ptr[ ii ]);  /* Lazy, but effective :) */

    char *p = strstr(buf, ASS_HARD_BREAK);  /* Get the / an end of the string */
    if ( NULL == p ) {
        p = &buf[ --max ];
    }

    /**
     ***************************************************************************
     * After copying the chapter's title, we'll search backwards from the
     * string's end and trim any whitespace.  And yes, there is a use-case
     * for this (I've seen this happen) ...
     *
     * I don't think there's a possibility of a collision between a 2-byte
     * UTF-8 char and a 3-byte UTF-8 char (i.e., the 2-byte char sequence is a
     * subset of the 3-byte char sequence), but the order in the 'spaces' array
     * (smallest to largest length) should handle those cases correctly, if any.
     */
    {
        static char const *const spaces[] = { SPACE_CHAR, UTF8_NON_BREAK_SPACE };
        #undef SPACES_SZ
        #define SPACES_SZ (sizeof (spaces) / sizeof (spaces[0]))

        label:
        for ( ii = 0; ii < SPACES_SZ; ii++ ) {
            char const *const space = spaces[ii];
            size_t len = strlen(space);

            char *q = (p - len);

            /*******************************************************************
             * Since we've ordered 'spaces[]' by length (small to large), this
             * terminates the for loop (i.e., don't need to 'continue;' here).
             */
            if ( q < buf ) break;
            if ( 0 == strncmp(q, space, len) ) {
                p = q;
                goto label;
            }
        }

        *p = '\0';                  /* Terminate the string now ... */
    }

    return ( strdup(buf) );         /* And finally, ultra-lazy for the win! */
}


/**
 *******************************************************************************
 * Build the output image's pathname from its details.
 */
static char * O3 build_name_from_details( details_t *const details )
{
    struct {
        char   *buf;
        char   *png_Title;
        size_t  buf_sz;
        size_t  page_number;
    } w = {
        .buf         = NULL,
        .buf_sz      = 0,
        .page_number = details->global_image_sequence_number,
        .png_Title   = strndup(details->png_Title, 80),
    };

    remove_attributes( w.png_Title );
    w.png_Title = make_legal_name( w.png_Title );

    int rc = asprintf(&w.buf, "%s" PA_PATH_SEP "%s%.4lu %s.%s",
                              details->dest_dir,
                              details->image_prefix,
                              w.page_number,
                              w.png_Title,
                              details->image_type );
    if ( rc < 0 ) {
        free( w.buf );   /* (also sets 'w.buf' to NULL.) */
    }

    (free)( (void *) w.png_Title );

    return ( w.buf );
}


/**
 *******************************************************************************
 * Remove any libass (character) attributes from 'str', modifying it in place.
 *
 * A libass character attribute looks like '{\stuff}'.  An example could be
 * {\b1}Liza{\b0} where '{\b1}' applies the bold attribute and '{\b0}' removes
 * the bold attribute.
 *
 * We're not going to go crazy with validating the tags since we'll assume
 * they've been added through some sort of automation or checked by the user.
 * Also, we _know_ we're going to make the string smaller, so memory management
 * is easy peasy.
 *
 * A use case is the volume's title, which is extracted from the first line
 * of the text, may contain libass character attributes.  We want to remove
 * these if this string is used to build a filename (which does happen).
 *
 * Any initial BOM sequence should have been removed by 'get_chapter_title()'.
 */
static char *O3 remove_attributes( char *const str )
{
    struct {
        char *const str;
        size_t      in_idx;
        size_t      out_idx;
        char        ch;
    } w = {
        .str = str,
        .in_idx = 0,
        .out_idx = 0,
    };


    if ( NULL != w.str )  /* (guard) */
    while ( (w.ch = *(w.str + w.out_idx) = *(w.str + w.in_idx++)) ) {
        if ( '{' == w.ch && '\\' == *(w.str + w.in_idx) ) {
            w.in_idx++;
            while ( (w.ch = *(w.str + w.in_idx) ) ) {
                w.in_idx++;   /* It's here to check for '\0' in string ... */
                if ( '}' == w.ch ) {
                    break;
                }
            }
        }
        else
        w.out_idx++;
    }

    return ( w.str );
}


/**
 *******************************************************************************
 * Change all occurrences of '/' to a 3-byte version that "looks the same."
 *
 * This is used for the chapter title, which may contain a '/' character, and
 * when used to build a filename, will wreck havoc trying to use that filename.
 *
 * Note, /bin/od -t x1 _file_ is your friend to get hex constants ...
 *       (Except for stdin, it doesn't flush its output buffer, so interactive
 *       usage can be a pain.)
 */
static char *O3 make_legal_name( char *const name )
{
    struct {
        char  *name;
        char  *p;
        size_t cur_len;
        size_t mov_len;
        size_t idx;
        size_t const delta;
    } w = {
        .name    = name,
        .cur_len = strlen( name ),
        .delta   = sizeof (UTF8_FRACTION_SLASH)  /* Should _never_ be negative */
                 - sizeof ("/"),
    };


    while ( NULL != (w.p = strchr( w.name, '/' )) ) {
        w.idx = w.p - w.name;
        w.cur_len += w.delta;
        w.name = realloc( w.name, w.cur_len + 1 );

        w.mov_len = strlen(w.name + w.idx);  /* This will count the '\0' */
        memmove( (w.name + w.idx + w.delta), (w.name + w.idx), w.mov_len );

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstringop-truncation"
        strncpy( (w.name + w.idx), UTF8_FRACTION_SLASH, sizeof (UTF8_FRACTION_SLASH) - 1 );
        #pragma GCC diagnostic pop
    }

    return ( w.name );
}


/**
 *******************************************************************************
 * A "Very Simple" function to chop the beginning of a string.
 *
 * For example, we want to remove "_picWork-" from -->
 *    _picWork-Death March kara Hajimaru Isekai Kyousoukyoku 03-05.0.txt
 */
static char const *chop_prefix(char const *name, char const *chop_prefix, size_t chop_chars)
{
    struct {
        size_t      idx;
        char const *p;
    } w = {
        .idx = 0,
    };

    if ( NULL != chop_prefix && NULL != (w.p = strstr(name, chop_prefix)) ) {
        if ( w.p == name ) {
            w.idx = strlen(chop_prefix);
        }
    }

    w.p = &name[ w.idx ];
    if ( chop_chars < strlen(w.p) ) {
        w.p += chop_chars;
    }

    return ( w.p );
}


/**
 *******************************************************************************
 */
static rc_e pa_verify_opts(UNUSED_ARG pa_opts_t *pa_opts)
{
    struct {
        rc_e  rc;
    } w = {
        .rc = RC_TRUE,
    };

    if ( pa_opts->pad_paragraph && pa_opts->pad_paragraph <= pa_opts->text_size ) {
        fprintf(stderr, "IGNORED :: --pad-paragraph=%d <= --text-size=%d.\n",
                        pa_opts->pad_paragraph, pa_opts->text_size);
        pa_opts->pad_paragraph = 0;
    }

    return ( w.rc );
}


/**
 *******************************************************************************
 * Get the RCS version of the program, printed on the console or in the image
 * comments tagged with 'Software'.
 */
static char O3 *get_Software()
{
    static char const *const version_pieces[] = {
        "$RCSfile: pngass.c,v $",
        "$Revision: 0.9 $",
        "$Date: 2018/11/17 21:28:50 $",
       &"$Date: 2018/11/17 21:28:50 $"[ sizeof ("#Date: 2018/09/02") - 1 ],
        NULL
    };
    static char buf[ 16 + sizeof ("pngass.c 1.2 2018/09/02 18:17:45") ] = { '\0' };
    struct {
        size_t       cnt;
        size_t       ii;
        char const  *ptr;

        unsigned int len;
        char        *p1;
        char        *p2;
    } w = {
        .ii    = 0,
    };


    if ( '\0' == buf[ 0 ] )  /* Only build the string ONCE */
    while ( (w.ptr = version_pieces[ w.ii++ ]) != NULL ) {
        w.p1 = strchr(w.ptr, ' ');
        w.p2 = strchr(++w.p1, (1 == w.ii) ? '.' : ' ');   /* HACK! */

        if ( w.ii > 1 ) {
            buf[ w.cnt ] = ' ';
            w.cnt++;
        }

        w.len = w.p2 - w.p1;
        strncpy(&buf[ w.cnt ], w.p1, w.len);
        w.cnt += w.len;
    }

    return ( buf );
}


/**
 *******************************************************************************
 *
 * TODO :: add a 'PA picture' with the pathname of the source picture.
 * TODO :: add a command line arg to provide interesting command line settings
 *         in the comments (e.g., font side, bottom border, etc.)
 */
static rc_e save_pa_image( pa_image_t *pa_image, pa_opts_t *pa_opts, clock_t my_clock )
{
    struct {
        details_t  *details;
        comments_t *comments;
        comments_t *png_comments;
        clock_t     diff;
        double      msec;
        time_t      now;
        char        tmp_str[ sizeof ("2011-10-08T07:07:09Z") ];
        size_t      idx;
        char       *str;   /* working pointer */
        rc_e        rc;
    } w = {
        .details  = &pa_opts->details,
        .diff     = clock() - my_clock,
        .now      = time(NULL),
        .rc       = RC_TRUE,
    };


    set_jpeg_quality( pa_image, pa_opts->jpeg_quality);
    set_no_comments( pa_image, pa_opts->no_comments );

    if ( 0 == pa_opts->no_comments ) {

        w.comments = calloc(1, sizeof (comments_t));

        add_comments( w.comments, "Software", get_Software() );

        if ( 0 == pa_opts->regression ) {
            strftime(w.tmp_str, sizeof (w.tmp_str), "%FT%TZ", gmtime(&w.now));
            add_comments( w.comments, "date:create", w.tmp_str );
            add_comments( w.comments, "PA Create", w.tmp_str );

            /*
             *******************************************************************
             * This is NOT meant to provide an accurate time (since trying to
             * keep track of the time for each pass on each PNG image isn't
             * mission critical).  It's just a _simple_ performance guage.
             */
            w.msec = (w.diff * 1000.0) / CLOCKS_PER_SEC;
            asprintf(&w.str, "%.2f milliseconds", w.msec);
            add_comments( w.comments, "PA Render Time", w.str );
            free (w.str);
        }

        w.str = realpath(w.details->chapter_filename, NULL);
        add_comments( w.comments, "PA Text Filename", w.str );
        free (w.str);

        if ( pa_opts->remove_dup_groups ) {
            asprintf(&w.str, "%u-%u", pa_opts->remove_dup_groups, w.details->dup_groups_found);
            add_comments( w.comments, "PA Duplicate Groups", w.str );
            free (w.str);
        }

        if ( pa_opts->remove_dup_group_spaces ) {
            add_comments( w.comments, "PA Group SPACEs", "Yes" );
        }

        asprintf(&w.str, "Font size=%d, Line Spacing=%.2f, Bottom margin=%d, Paragraph pad=%d.",
                         pa_opts->text_size, pa_opts->line_spacing,
                         pa_opts->margin_bottom, pa_opts->pad_paragraph);
        add_comments( w.comments, "PA Config", w.str );
        free (w.str);

        // TODO :: If 'jpg', then include a quality tag
        w.str = realpath(w.details->in_png_name, NULL);
        add_comments( w.comments, "PA Image", w.str );
        free (w.str);

        asprintf(&w.str, "%s", w.details->png_Title);
        remove_attributes( w.str );
        add_comments( w.comments, "PA Title", w.str );
        free (w.str);

        w.str = (char *) build_page_X_of_Y(w.details->chapter_image_number, w.details->chapter_images, XY_DECIMAL);
        add_comments( w.comments, "PA Page", w.str );
        free (w.str);

        add_comments( w.comments, "PA libass", xstr(LIBASS_VERSION));
        #include <jconfig.h>
        add_comments( w.comments, "PA libjpeg", xstr(JPEG_LIB_VERSION));

        /*
         ***********************************************************************
         * Append the "original" comments to the list of comments we've built.
         * HACK :: We'll skip comments that are the string 'null'.
         */
        w.png_comments = get_png_comments( pa_image );
        if ( NULL != w.png_comments )
        for ( w.idx = 0; w.idx < w.png_comments->cnt; w.idx++ ) {
            if ( strcmp("null", w.png_comments->kvs[ w.idx ].val) )
            add_comments( w.comments, w.png_comments->kvs[ w.idx ].key, w.png_comments->kvs[ w.idx ].val );
        }

        replace_png_comments( pa_image, w.comments );
    }

    w.str = build_name_from_details( w.details );
    write_image_file( w.str, pa_image, w.details->image_type );
    free (w.str);

    return ( w.rc );  // FIXME it always returns RC_TRUE
}

