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
 * Reformat a text file to the width (in characters) as specified on the
 * command line.  Basically, a more sophisticated and specialized 'fmt' command.
 *
 * 76 character ==>
 * guess it can't be helped that it's shaking more than when it's on the river.
 *
 * The is similar to the 'fmt' command, except that it's smarter about lines
 * and paragraphs.  Consider (from the original) -->
 *
 *   "Ya' guys! Captain Arisa-sama is departing yarr'~" *
 *   "Yessire~?"
 *   "Aye aye~"
 *   "Nn."
 *
 * The 'fmt' command will join them all together like so (with '-w 50') -->
 *
 *   "Ya' guys! Captain Arisa-sama is departing
 *   yarr'~" "Yessire~?"  "Aye aye~" "Nn."
 *
 * which is NOT what we want.  A line which begins with '"' should be treated
 * as a single paragraph until the closing '"' even if it's on another line.
 *
 * Also, we want to treat some (lines) paragraphs distinctly as well -->
 *   <TLN: ...
 * should always start a new paragraph, for example.
 *
 * TODO :: Should I fix these (there seems to be a few) ==>
 *         "Gaan.""Na, nanodesuu."
 *         in 'Death March kara Hajimaru Isekai Kyousoukyoku 09-01.0.txt'
 *         by putting a break between the double quotes (I can probably do
 *         it with a simple rule for the 'do_pair_substitution()' function.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <locale.h>

#include "pa_misc.h"
#include "pa_attributes.h"

#ifndef realloc
#warning "realloc() is not a macro..."
#endif


static char  *do_pair_substitution(char *in_buf, unsigned int idx, unsigned short r_level,
                                   key_values_t const *const, int *const);
#undef do_pair_substitution
#define       do_pair_substitution( root,   idx,      kv,   p_cnt )  \
              do_pair_substitution((root), (idx), 0, (kv), (p_cnt))

static char  *apply_two_spaces    (char *, char const *const ends, char const *const *exceptions, int *const);
static char  *do_quote_urban_words(char *, key_values_t const *const, size_t idx, int *const);

static rc_e   srch_fchrs(char const *const key, char const *const *keys, char const *const val, size_t idx);
static rc_e   srch_bchrs(char const *const key, char const *const *keys, char const *const str, size_t idx);
static size_t strfchars(char const *const *keys, char const *const str, size_t idx);

#pragma GCC diagnostic ignored "-Wunused-function"  // TODO :: remove later ...


/*******************************************************************************
 *******************************************************************************
 * This function is meant to add enhancements to the input text such as pair
 * matching quotes, fancy apostrophes, two spaces after a sentence, etc.
 *
 * The enhancement selection is controlled by the 'attr_edits_e' argument.
 */
char * O0 apply_attr_edits(char *ptr_root, attr_edits_e attr_edits, attr_stats_t *attr_stats)
{
#undef ATTR_CNT
#define ATTR_CNT(idx_) ((NULL == attr_stats)                \
        ? &w.cnt : &attr_stats[ __builtin_ctz(idx_) ].cnt)
    struct {
        char         *ptr_root;
        char         *ptr_end;
        unsigned int  idx;
        int           cnt;
    } w = {
        .idx       = 0,
        .ptr_root  = ptr_root,
    };


    if ( '\0' != *(w.ptr_root + w.idx) ) {
      /*************************************************************************
       * Ensure there are two SPACEs after a sentence end.
       */
      if ( attr_edits & AE_TWO_SPACES )
        w.ptr_root = apply_two_spaces( w.ptr_root, SENTENCE_END_CHARS, TWO_SPACE_EXCEPTONS, ATTR_CNT(AE_TWO_SPACES) );

      /*************************************************************************
       * Words:  'bout = about, 'fter = after, 'ning = morning, etc.
       */
      if ( attr_edits & AE_URBAN_WORDS )
        w.ptr_root = do_quote_urban_words( w.ptr_root, &quote_and_urban_word, w.idx, ATTR_CNT(AE_URBAN_WORDS) );
    }


    while ( '\0' != *(w.ptr_root + w.idx) ) {
        /*
         ***********************************************************************
         * Wide SPACEs are a nuisance since libass won't split the line on them
         * (which are _different_ than a non-breaking SPACE).
         */
        if ( attr_edits & AE_WIDE_SPACE )
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &replace_wide_space, ATTR_CNT(AE_WIDE_SPACE) );

        /*
         ***********************************************************************
         * Replace '--' with a heavier dash character (aesthetic).
         */
        if ( attr_edits & AE_DOUBLE_DASH )
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &replace_double_dash, ATTR_CNT(AE_DOUBLE_DASH) );

        /*
         ***********************************************************************
         * Words:  'bout = about, 'fter = after, 'ning = morning, etc.
         */
        if ( attr_edits & AE_URBAN_WORDS )
          if (0) w.ptr_root = do_quote_urban_words( w.ptr_root, &quote_and_urban_word, w.idx, ATTR_CNT(AE_URBAN_WORDS) );

        /*
         ***********************************************************************
         * Words:  y'can  n'all  d'ya  s'long  d'ya  yarr'~  y'hear?  y'knooo~
         */
        if ( attr_edits & AE_YOU_KNOW )
          if (0) w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &y_know_apostrophe, ATTR_CNT(AE_YOU_KNOW) );

        /*
         ***********************************************************************
         * Replace ',' with a SPACE after the comma -> ', '.
         */
        if ( attr_edits & AE_SQUOTE_SPACING )
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &fix_squotes_spacing, ATTR_CNT(AE_SQUOTE_SPACING) );

        /*
         ***********************************************************************
         * Match up SINGLE quote pairs.
         */
        if ( attr_edits & AE_SQUOTE_PAIRS )
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &single_quote_pairs, ATTR_CNT(AE_SQUOTE_PAIRS) );

        /*
         ***********************************************************************
         * Words:  y'can  n'all  d'ya  s'long  d'ya  yarr'~  y'hear?  y'knooo~
         */
        if ( attr_edits & AE_YOU_KNOW )
          if (1) w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &y_know_apostrophe, ATTR_CNT(AE_YOU_KNOW) );

        /*
         ***********************************************************************
         * Match up DOUBLE quote pairs.
         */
        if ( attr_edits & AE_DQUOTE_PAIRS ) {
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &double_quote_pairs, ATTR_CNT(AE_DQUOTE_PAIRS) );
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &double_quote_pairs_2, ATTR_CNT(AE_DQUOTE_PAIRS) );
        }

        /*
         ***********************************************************************
         * Most contractions: don't, can't, they're, etc.
         */
        if ( attr_edits & AE_CONTRACTIONS )
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &most_contractions, ATTR_CNT(AE_CONTRACTIONS) );

        /*
         ***********************************************************************
         * Plural possessive nouns
         */
        if ( attr_edits & AE_PLURAL_POSSESSIVE )
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &plural_possessive_noun, ATTR_CNT(AE_PLURAL_POSSESSIVE) );

        /*
         ***********************************************************************
         * Words:  hafta'  ta'  eno'  tru'  eh'  yer'  mockin'  yappin'!
         */
        if ( attr_edits & AE_URBAN_OTHER )
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &hafta_yappin_apostrophe, ATTR_CNT(AE_URBAN_OTHER) );

        /*
         ***********************************************************************
         * Emoji: :( (this is incomplete at this point)
         */
        if ( attr_edits & AE_EMOJI ) {
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &emoji_happy, ATTR_CNT(AE_EMOJI) );
          w.ptr_root = do_pair_substitution( w.ptr_root, w.idx, &emoji_sad, ATTR_CNT(AE_EMOJI) );
        }


        w.ptr_end = strchr((w.ptr_root + w.idx), '\n');
        if ( NULL == w.ptr_end ) {
            break;
        }

        w.idx += w.ptr_end - (w.ptr_root + w.idx);
        w.idx++;  /* Advance past the '\n' */
    }


    return ( w.ptr_root );
}


/**
 *******************************************************************************
 * Apply two spaces between each sentence in all of the paragraph(s).
 *
 * This is the simplest transformation to make.  Here are the rules:
 * - find an end-of-sentence character;
 * - ensure that the next char is a SPACE;
 * - ensure that the character after the SPACE is an uppercase letter;
 * - ensure that the character before the end-of-sentence character
 *   is a lowercase letter or '~'; and
 * - ensure that the previous string is NOT in the list of exceptions ('Mr.').
 */
static char * O0 apply_two_spaces(char *in_buf, char const *const ends, char const *const *exceptions, int *const p_cnt)
{
    struct {
       char   *ptr;
       size_t  idx;
       char    ch;  /* Could be 'int', but 'char' is easier in debugger. */
       size_t  len;
    } w = {
        .ptr = in_buf,
        .idx = 1,
        .len = strlen(in_buf),  /* Note, we add the '\0' at the realloc() */
    };


    while ( (w.ch = *(w.ptr + w.idx++)) ) {  /* NOTE, 'w.idx' points to SPACE */
            /*******************************************************************
             * Search for the four char sequence '[group][ends] [:upper:]'
             * where 'group' is a character in the set of :lower: or '~'.
             * This isn't an elegant solution, but it's robust (I think).
             */
        if (       strchr(ends, w.ch) != NULL
                && isblank(*(w.ptr + w.idx))
                && isupper(*(w.ptr + w.idx + 1))
                && (islower(*(w.ptr + w.idx - 2)) || strchr("~", *(w.ptr + w.idx - 2)))
                && RC_FALSE == srch_bchrs( NULL, exceptions, w.ptr, w.idx )
            ) {
                w.ptr = realloc(w.ptr, ++w.len + 1);  /* Don't forget '\0'. */
                *(w.ptr + w.idx) = ' ';  /* Ensure that the char is a SPACE. */
                memmove(w.ptr + w.idx + 1, w.ptr + w.idx, w.len - w.idx);

                /***************************************************************
                 * Point to character _after_ the start of the next sentence.
                 */
                w.idx += 3;
                (*p_cnt)++;
        }
    }

    return ( w.ptr );
}


/**
 *******************************************************************************
 * Recursively apply primitive (based on the rules provided in 'key_values_t')
 * substitutions to the string.
 *
 * We use a simple rule-based algorithm to match pairs.  Honestly, don't know
 * if there's a better / more accurte algorithm to do these things, but this
 * manages to do a pretty decent job anyway.
 */
#undef do_pair_substitution
static char * O0 do_pair_substitution(char *ptr_root, unsigned int idx, unsigned short r_level,
                                      key_values_t const *const kv, int *const p_cnt)
{
#undef NOT_END_OF_LINE
#define NOT_END_OF_LINE(idx_)  \
        ('\0' != *(w.ptr_root + (idx_)) && '\n' != *(w.ptr_root + (idx_)))

    struct {
       char   *ptr_root;         /* NOTE :: We treat this as a '* CONST'. */
       size_t  idx;

       size_t  next;
       size_t  move_len;
       size_t  new_size;         /* Only set if we need to do a memmove() */
       int     delta;            /* (Needs to be a signed type.) */
       int     open_ok;

       enum  { EX_NONE = 0, EX_SUBSTITUTE } ex_done;
    } w = {
        .ptr_root = ptr_root,
        .idx      = idx,
        .ex_done  = EX_NONE,
    };


    while ( NOT_END_OF_LINE(w.idx) ) {
        /*
         ***********************************************************************
         * Search for the 'key' string for the ST_OPEN state.
         */
        if ( strncmp((w.ptr_root + w.idx), kv->key.str, kv->key.len) ) {
            w.idx++;
            continue;
        }

        w.next = w.idx + kv->key.len;
        /*
         ***********************************************************************
         * We've matched the ST_OPEN key string.  Next, check the pre-character
         * requirement.  If that test fails and <idx> != 0, then we'll return.
         * Otherwise, we'll keep looking for a valid ST_OPEN state.
         */
        if (   RC_FALSE == srch_bchrs( kv->key.open_chars, kv->key.open_utf8s, w.ptr_root, w.idx )
             || (NULL != kv->key.open_exclc
                 && RC_TRUE == srch_fchrs( kv->key.open_exclc, NULL, w.ptr_root, w.next ))
           ) {
            if ( r_level ) {
                return ( w.ptr_root );
            }
            w.idx += kv->key.len;
            continue;
        }

        w.open_ok = 0;

        /*
         ***********************************************************************
         * If there's no ST_CLOSE string, then we'll overload the use of the
         * 'key.close_chars' string as a prerequisite to determine if we will
         * perform a substitution for this key.
         */
        if ( NULL == kv->vals[ ST_CLOSE ].str ) {
            if ( RC_FALSE == srch_fchrs( kv->key.close_chars, kv->key.close_strs, w.ptr_root, w.next ) ) {
                w.idx = w.next;
                continue;
            }
            w.open_ok = 1;
        }
        else while ( NOT_END_OF_LINE(w.next) ) {
            if ( STR_MATCH != strncmp((w.ptr_root + w.next), kv->key.str, kv->key.len) ) {
                  /*
                   *************************************************************
                   * Note, we can't advance by 'kv->key.len' because that
                   * may / probably put us beyond the end of the string.
                   */
                w.next++;
                continue;
            }

              /*
               *****************************************************************
               * See if this is the start of another pair...  After that, check
               * to see if our original match is still there (iows, it probably
               * was replaced by the call to do_pair_substitution()).
               */
            if ( kv->recurse ) {        //  TODO :: WORKS, cleanup amd make more "formal"
                w.ptr_root = (do_pair_substitution)(w.ptr_root, w.next, r_level + 1, kv, p_cnt);

                if ( STR_MATCH != strncmp((w.ptr_root + w.next), kv->key.str, kv->key.len) ) {
                    w.next += kv->vals[ ST_OPEN ].len;
                    continue;  /* The original match is no longer there ... */
                }
            }

            /*
             *******************************************************************
             * We have a match for the ST_CLOSE state, check the post-character
             * requirement.  If it FAILs, we advance past the 'key' and keep
             * searching for the ST_CLOSE until we exhaust the string.
             */
            if ( NOT_END_OF_LINE(w.next + kv->key.len) ) {
                if ( RC_FALSE == srch_fchrs( kv->key.close_chars, kv->key.close_strs, w.ptr_root, (w.next + kv->key.len) ) ) {
                    w.next += kv->key.len;
                    continue;
                }
            }

            w.delta = kv->vals[ ST_CLOSE ].len - kv->key.len;
            if ( w.delta > 0 ) {

                /***************************************************************
                 * (Note, the 'new_size' includes the string's '\0'.)
                 */
                w.new_size = 1 + strlen(w.ptr_root) + w.delta;
                w.ptr_root = realloc(w.ptr_root, w.new_size);
            }

            w.move_len = 1 + strlen(w.ptr_root + w.next + kv->key.len);
            if ( w.delta ) {
                memmove((w.ptr_root + w.next + kv->vals[ ST_CLOSE ].len), (w.ptr_root + w.next + kv->key.len), w.move_len);
            }
            strncpy((w.ptr_root + w.next), kv->vals[ ST_CLOSE ].str, kv->vals[ ST_CLOSE ].len);
            w.ex_done = EX_SUBSTITUTE;
            (*p_cnt)++;

            /*
             *******************************************************************
             * 'w.next' is used to calculate the start point for the next search.
             */
            w.next += kv->vals[ ST_CLOSE ].len;
            w.open_ok = 1;
            break;
        }

          /*
           *********************************************************************
           * If there is no pair match, or the prerequisites didn't pass,
           * then quit the search for this line.
           */
        if ( 0 == w.open_ok ) {
            break;
        }

        w.delta = 0;
        if ( kv->vals[ ST_OPEN ].str != NULL ) {
            w.delta = kv->vals[ ST_OPEN ].len - kv->key.len;

            w.move_len = 1 + strlen(w.ptr_root + w.idx + kv->key.len);
            if ( w.delta > 0 ) {

                /***************************************************************
                 * (Note, 'w.new_size' includes the string's '\0' terminator.)
                 */
                w.new_size = 1 + strlen(w.ptr_root) + w.delta;
                w.ptr_root = realloc(w.ptr_root, w.new_size);
            }

            if ( w.delta ) {
                memmove((w.ptr_root + w.idx + kv->vals[ ST_OPEN ].len), (w.ptr_root + w.idx + kv->key.len), w.move_len);
            }
            strncpy((w.ptr_root + w.idx), kv->vals[ ST_OPEN ].str, kv->vals[ ST_OPEN ].len);
            w.ex_done = EX_SUBSTITUTE;
            (*p_cnt)++;
        }

        /***********************************************************************
         * If we successfully completed 1 pair-replacement at _this_ recursion
         * level, then back up to the previous level and continue the search.
         */
        if ( w.ex_done && r_level ) {
            break;
        }

        w.idx = w.next + w.delta;
    }

    return ( w.ptr_root );
}


/**
 *******************************************************************************
 */
static char *do_quote_urban_words(char *ptr_root, key_values_t const *const kv, size_t idx, int *const p_cnt)
{
#undef NOT_END_OF_LINE
#define NOT_END_OF_LINE(idx_)  ('\0' != *(w.ptr_root + (idx_)))
    struct {
       char   *ptr_root;        /* NOTE, we treat this as '* const buf'. */
       size_t  idx;
       size_t  nxt;
       size_t  move_len;
       size_t  new_size;        /* Only set if we need to do a memmove() */
       int     delta;           /* Needs to be a signed type. */

       size_t  len;
       rc_e    rc;
    } w = {
        .idx      = idx,
        .ptr_root = ptr_root,
    };


    while ( NOT_END_OF_LINE(w.idx) ) {
        /*
         ***********************************************************************
         * Search for the 'key' string for the ST_OPEN state.
         */
        if ( strncmp((w.ptr_root + w.idx), kv->key.str, kv->key.len) ) {
            w.idx++;
            continue;
        }

        /*
         ***********************************************************************
         * We've matched the ST_OPEN key string.  Next, check the pre-character
         * prerequisites.  If that test fails and <idx> != 0, then we'll return.
         * Otherwise, we'll keep looking for a valid ST_OPEN state.
         */
        if ( w.idx && kv->key.open_chars != NULL ) {
            if ( NULL == strchr( kv->key.open_chars, *(w.ptr_root + w.idx - 1)) ) {
                w.idx += kv->key.len;
                continue;
            }
        }

        if ( 0 == (w.len = strfchars( kv->key.close_strs, w.ptr_root, w.idx + kv->key.len )) ) {
            w.idx += kv->key.len;
            continue;
        }

        if ( 0 == (w.nxt = strfchars( WORD_END_STRINGS, w.ptr_root, w.idx + kv->key.len + w.len )) ) {
            w.idx += kv->key.len;
            continue;
        }

        w.delta = 0;
        if ( kv->vals[ ST_OPEN ].str != NULL ) {
            w.delta = kv->vals[ ST_OPEN ].len - kv->key.len;

            w.move_len = 1 + strlen(w.ptr_root + w.idx + kv->key.len);
            if ( w.delta > 0 ) {

                w.new_size = 1 + strlen(w.ptr_root) + w.delta;
                w.ptr_root = realloc(w.ptr_root, w.new_size);
            }

            if ( w.delta ) {
                memmove((w.ptr_root + w.idx + kv->vals[ ST_OPEN ].len), (w.ptr_root + w.idx + kv->key.len), w.move_len);
            }
            strncpy((w.ptr_root + w.idx), kv->vals[ ST_OPEN ].str, kv->vals[ ST_OPEN ].len);
            (*p_cnt)++;
        }

        w.idx += (kv->key.len + w.len + w.nxt + w.delta);
    }

    return ( w.ptr_root );
}


/*******************************************************************************
 * Is the utf-8 character at the current position in 'str' in 'key' or 'keys'?
 *
 * So -->
 *  - Return 'RC_TRUE' if 'key' is NULL and 'keys' is NULL (iow, the caller
 *    does NOT care if the character at 'str' + 'idx' matches to anything).
 *
 */
static rc_e srch_fchrs(char const *const key, char const *const *keys, char const *const str, size_t idx)
{
    struct {
        char const *const  key;
        char const *const *keys;
        char const *const  str;
        size_t             idx;

        rc_e               rc;
        char const        *ptr;
        size_t             len;
    } w = {
        .key  = key,
        .keys = keys,
        .str  = str,
        .idx  = idx,

        .rc   = RC_TRUE,
    };


    if ( w.key != NULL ) {
        if ( NULL == strchr(w.key, *(w.str + w.idx)) ) {
            w.rc = RC_FALSE;
        }
    }

    if ( w.keys != NULL )
    if ( RC_FALSE == w.rc || NULL == w.key ) {
        w.rc = RC_FALSE;
        while ( (w.ptr = *w.keys++) != NULL ) {
            w.len = strlen(w.ptr);
            if ( 0 == strncmp((w.str + w.idx), w.ptr, w.len) ) {
                w.rc = RC_TRUE;
                break;
            }
        }
    }

    return ( w.rc );
}


/*******************************************************************************
 *
 */
static size_t strfchars(char const *const *keys, char const *const str, size_t idx)
{
    struct {
        char const *const *keys;
        char const *const  str;
        size_t             idx;

        int        rv;
        char const *ptr;
        size_t     len;
    } w = {
        .keys    = keys,
        .str     = str,
        .idx     = idx,

        .rv      = 0,
    };


    if ( w.keys != NULL )
    while ( (w.ptr = *w.keys++) != NULL ) {
        w.len = strlen(w.ptr);
        if ( 0 == strncmp((w.str + w.idx), w.ptr, w.len) ) {
            w.rv = w.len;
            break;
        }
    }

    return ( w.rv );
}


/**
 *******************************************************************************
 * Is the utf-8 character at the previous position in 'str' in 'key' or 'keys'?
 */
static rc_e O0 srch_bchrs(char const *const key, char const *const *keys, char const *const str, size_t idx)
{
    struct {
        char const *const  key;
        char const *const *keys;
        char const *const  str;
        size_t             idx;

        rc_e               rc;
        char const        *ptr;
        size_t             len;
    } w = {
        .key  = key,
        .keys = keys,
        .str  = str,
        .idx  = idx,

        .rc   = RC_TRUE,
    };


   /**
    ***************************************************************************
    * The start (or anchor) for a line is:
    *  - the beginning of the text block (i.e. 'w.idx' is ZERO), or
    *  - the previous character is a '\n'.
    * In either case, we'll return 'RC_TRUE' to indicate a successful match.
    */
    if ( w.idx ) {
        if ( NULL != w.key ) {
            if ( '\n' != *(w.str + w.idx - 1) )
            if ( NULL == strchr(w.key, *(w.str + w.idx - 1)) ) {
                w.rc = RC_FALSE;
            }
        }

        if ( NULL != w.keys )
        if ( RC_FALSE == w.rc || NULL == w.key ) {
            w.rc = RC_FALSE;
            while ( (w.ptr = *w.keys++) != NULL ) {
                w.len = strlen(w.ptr);
                if ( (w.idx >= w.len) && 0 == strncmp((w.str + w.idx - w.len), w.ptr, w.len) ) {
                    w.rc = RC_TRUE;
                    break;
                }
            }
        }
    }

    return ( w.rc );
}


/*
 ******************************************************************************
 * Build a free-able accounting array passed to 'apply_attr_edits()'.
 */
attr_stats_t *build_attr_stats(attr_edits_e attr_edits)
{
    static unsigned short const bits[] = { ATTR_EDITS_BITS };
    attr_stats_t *attr_stats = calloc(__builtin_popcount(AE_ALL), sizeof (attr_stats_t));

    attr_stats[ __builtin_ctz(AE_TWO_SPACES) ].desc        = "TWO SPACES AFTER SENTENCE";
    attr_stats[ __builtin_ctz(AE_DOUBLE_DASH) ].desc       = replace_double_dash.desc;
    attr_stats[ __builtin_ctz(AE_URBAN_WORDS) ].desc       = quote_and_urban_word.desc;
    attr_stats[ __builtin_ctz(AE_SQUOTE_SPACING) ].desc    = fix_squotes_spacing.desc;
    attr_stats[ __builtin_ctz(AE_SQUOTE_PAIRS) ].desc      = single_quote_pairs.desc;
    attr_stats[ __builtin_ctz(AE_YOU_KNOW) ].desc          = y_know_apostrophe.desc;
    attr_stats[ __builtin_ctz(AE_DQUOTE_PAIRS) ].desc      = double_quote_pairs.desc;
    attr_stats[ __builtin_ctz(AE_CONTRACTIONS) ].desc      = most_contractions.desc;
    attr_stats[ __builtin_ctz(AE_PLURAL_POSSESSIVE) ].desc = plural_possessive_noun.desc;
    attr_stats[ __builtin_ctz(AE_URBAN_OTHER) ].desc       = hafta_yappin_apostrophe.desc;
    attr_stats[ __builtin_ctz(AE_EMOJI) ].desc             = emoji_happy.desc;
    attr_stats[ __builtin_ctz(AE_WIDE_SPACE) ].desc        = replace_wide_space.desc;

    /*
     **************************************************************************
     * Anything the user is NOT interested in, we'll set it's count to -1.
     */
    for ( unsigned short ii = 0; ii < (sizeof (bits) / sizeof (bits[0])); ii++ ) {
        if ( bits[ ii ] & ~attr_edits ) {
            attr_stats[ __builtin_ctz(bits[ ii ]) ].cnt = -1;
        }
    }

    return ( attr_stats );
}


/*
 ******************************************************************************
 */
#if 0
char *build_attr_stats(attr_stats_t *attr_stats)
{
    static unsigned short const bits[] = { ATTR_EDITS_BITS };


}
#endif


/*
 ******************************************************************************
 ******************************************************************************
 * Testing section ...
 */
#ifdef PA_ATTRIBUTES_MAIN  /* { */

int main(UNUSED_ARG int argc, UNUSED_ARG char *argv[])
{

#define TEST_STR_0 "This\nis a test\ntesting"
#define TEST_STR_1 \
        "\"\"\"BAD!\"\"'''OU!'''\"\n" \
        "\"\"\"GOOD\"\" '''OU!'''\"\n" \
        " \"\"\"XXXX\"\"\" '''OU!'''\"\n" \
\
        "'\"I didn't hug him.\"'" \
        "\n" \
        "\"'I didn't hug him.'\"" \
        "\n" \
\
        "\n" \
        "''OU!'' '''OU!'''\n" \
        "\"OU!\"\n"      \
        "a \"\"OU!\"\"\n"      \
        " \"\"\"OU!\"\"\" \n"\
        "'''OU!''' \n"      \
        "\"\"\"Yes\"\"\", "      \
        "\"\"\"Yes\"\"\" "      \
        "\"\"\"\"Yes\"\"\"\"\n"      \
        "\"\"\"Yes\"\"\", "      \
        "\"\"\"Yes\"\"\""      \
        "\"\"\"\"Yes\"\"\"\"\n"      \
        "'''OU!''' "      \
        "\"\"\"Kyupopo, kyupopo, kyupopo\"\", nanodesu!\"\n" \
        "\"This 'a', 'b'\" is \"\"line \"#2\", 'nice'\" quoted\"\n"        \
        "\"This\" 'is' a \"\"test\", 'a'\" \"good \"test\"\",\n"           \
        ":) :( 'bout = about, \"'fter\" = after, y'can  n'all  d'ya\n"     \
        "\"This 'a',''b''\" is \"\"line \"#2\", 'nice'\" quoted\"\n"       \
        "This 'a','b  is \"\"line \"#2\", 'nice'\" quoted\"\n"             \
        "a 'the' \"end.\" Jim's it's they're \"\"\"Yes\"\"\" "  "\n"    \
        "\"Before that, let me enjoy the teens' scent\"\"Auu.\""  /* 5.7 */

    attr_edits_e attr_edits =  0
                             | AE_SQUOTE_PAIRS
                             | AE_DQUOTE_PAIRS
/*                              | AE_ALL */
                              ;

    attr_stats_t *attr_stats = build_attr_stats(attr_edits);
    char *ptr_root = strdup(TEST_STR_1);


//  ptr_root = apply_attr_edits(ptr_root, AE_ALL | AE_SQUOTE_SPACING | AE_SQUOTE_PAIRS);
//  ptr_root = apply_attr_edits(ptr_root, AE_SQUOTE_PAIRS | AE_DQUOTE_PAIRS);
    ptr_root = apply_attr_edits(ptr_root, attr_edits, attr_stats);


    fprintf(stdout, "%s\n  ===>>\n%s\n", TEST_STR_1, ptr_root);

    free(ptr_root);
    free(attr_stats);

    return (0);
}

#endif  /* } */

