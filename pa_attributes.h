#ifndef PA_ATTRIBUTES_H  /* { */
#define PA_ATTRIBUTES_H

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

typedef enum {
        AE_TWO_SPACES        = 0x0001,
        AE_DOUBLE_DASH       = 0x0002,
        AE_URBAN_WORDS       = 0x0004,
        AE_SQUOTE_SPACING    = 0x0008,
        AE_SQUOTE_PAIRS      = 0x0010,
        AE_YOU_KNOW          = 0x0020,
        AE_DQUOTE_PAIRS      = 0x0040,
        AE_CONTRACTIONS      = 0x0080,
        AE_PLURAL_POSSESSIVE = 0x0100,
        AE_URBAN_OTHER       = 0x0200,
        AE_EMOJI             = 0x0400,
        AE_WIDE_SPACE        = 0x0800,

        AE_ALL = AE_TWO_SPACES
               | AE_DOUBLE_DASH
               | AE_URBAN_WORDS
               | AE_SQUOTE_SPACING
               | AE_SQUOTE_PAIRS
               | AE_YOU_KNOW
               | AE_DQUOTE_PAIRS
               | AE_CONTRACTIONS
               | AE_PLURAL_POSSESSIVE
               | AE_URBAN_OTHER
               | AE_EMOJI
               | AE_WIDE_SPACE
} attr_edits_e;

#undef ATTR_EDITS_BITS
#define ATTR_EDITS_BITS \
        AE_TWO_SPACES, AE_DOUBLE_DASH, AE_URBAN_WORDS, AE_SQUOTE_SPACING, \
        AE_SQUOTE_PAIRS, AE_YOU_KNOW, AE_DQUOTE_PAIRS, AE_CONTRACTIONS,   \
        AE_PLURAL_POSSESSIVE, AE_URBAN_OTHER, AE_EMOJI, AE_WIDE_SPACE,

typedef struct attr_stats_t {
    int         cnt;
    char const *desc;
} attr_stats_t;


enum { ST_OPEN = 0, ST_CLOSE };
typedef struct {
    struct {
        char *str;
        size_t len;

        /**
         **********************************************************************
         * These are the characters that are allowed BEFORE the OPEN key 'str'.
         * E.g., for a '"', the legal characters that can appear _before_ the
         * '"' are: SPACE, a signle quote, and a double quote (and some other
         * utf-8 variants which are defined in the 'open_utf8s' variable).
         */
        char const *const  open_chars;
        char const *const *open_utf8s;
        char const *const  open_exclc;  /* Can't appear _after_ the opening */

        char const *const  close_chars;
        char const *const *close_strs;
    } key;
    struct {
        char  *str;
        size_t len;
    } vals[ 2 ];
    char const *desc;
    enum { KY_ONCE = 0, KY_RECURSE } recurse;
} key_values_t;  /* key / value1 / value2 */


char         *apply_attr_edits(char *ptr_root, attr_edits_e, attr_stats_t *);
attr_stats_t *build_attr_stats(attr_edits_e);

    /*
     **************************************************************************
     * This header file is a DEBUG hack.  High-bit multibyte characters mess
     * up the text formatting in ddd (possibly gdb as well), so I stick things
     * containing those characters in the header file.
     */
#define WORD_END_STRINGS   (char const *const []) {   \
        " ",  ".",  "?",  "!",  ":",  "'",            \
        ",",  "\"", "]",  ">",  ")",                  \
        "。",  "）",  "』",  "」",                    \
    NULL }
#define TWO_SPACE_EXCEPTONS    (char const *const []) {           \
        "Mr.",  "Ms.",  "Mrs.",  "Dr.",                           \
          /* Should I add directions (' N.', ' E.', etc.)? */     \
    NULL }

            /* Note, 'END_CHARS' can NOT contain any multi-byte characters. */
#define END_CHARS               "]>"  "\".?!"
#define SENTENCE_END_CHARS      &END_CHARS[2]
#define SENTENCE_END_CHARS_ALL  &END_CHARS[0]  /* Unused, historical */

/*******************************************************************************
 * 16-42.txt
 *   < 『Her parent said "I’m never giving my daughter to soldiers and guards."』
 *   ---
 *   > 『Her parent said “I’m never giving my daughter to soldiers and guards.”』
 *
 * Near impossible to parse (best solution is a dictionary, I suppose) ===>>
 * 15-42.txt
 *   < “Guess a direct confrontation ‘s impossible ‘fter all, mite’ work if we had 10,000 lesser dragons more as the ingredients tho’.”
 *   ---
 *   > “Guess a direct confrontation ’s impossible ‘fter all, mite’ work if we had 10,000 lesser dragons more as the ingredients tho'.”
 *   yer'  ta'
 */
#undef MY_KEY
#define MY_KEY "'"
static key_values_t const quote_and_urban_word = {
    .key = {
        .str  = MY_KEY,
        .len  = sizeof (MY_KEY) - 1,
        .open_chars  = " ",  /* maybe " and ' ... */
 //          .close_chars = " .,;\"\n"  "->"  "!?",
          /* SUBSTRINGS must go _after_ their main part, 'ere' before 'er'. */
        .close_strs = (char const *const []) {
            "be",      /* 15-42 */
            "bout",    /* 15-42 about */
            "ear",     /* 15-42 hear */
            "em",      /* 16-30 them */
            "ere",     /* 15-42 */
            "er",      /* 15-42 */
            "ey",      /* 15-42 */
            "fter",    /* 15-42 after */
            "kay",     /* 15-13, 15-17 */
            "k",
            "ning",    /* 06-01 morning */
            "nother",  /* 16-44 another */
            "rite",    /* 15-33 right */
            "s",
            "til",     /* 07-03 until */
            "tis",     /* 16-31 it is */
            "uns",     /* 15-03 */
            "un",      /* 07-03 */
            NULL
          },
        },
    .vals = { [ ST_OPEN  ] { "’",  sizeof ("’") - 1 }, },
    .desc = "AN 'URBAN-WORD <DICT>",
};

#undef MY_KEY
#define MY_KEY "\""
static key_values_t const double_quote_pairs_XXXXXXX = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = " ,'\"",
             .open_utf8s  = (char const *const []) { "”", "“", "‘", "『", "「", NULL },  /* 05-08 '"I didn't hug him."' */
             .open_exclc  = " "  ",.!?;:",
             .close_chars = " .,;'\"\n"  "->",
             .close_strs  = (char const *const []) { "』", "」", "’", NULL },  /* 16-42 */
           },
    .vals = { [ ST_OPEN  ] { "“",  sizeof ("“") - 1 },
              [ ST_CLOSE ] { "”",  sizeof ("”") - 1 } },
    .desc = "PRETTY PAIR DOUBLE-QUOTES",
    .recurse = KY_RECURSE
};

#undef MY_KEY
#define MY_KEY "\""
static key_values_t const double_quote_pairs_2 = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = " ,'\"",
             .open_utf8s  = (char const *const []) { "”", "‘", "『", "「", NULL },  /* 05-08 '"I didn't hug him."' */
             .close_chars = " .,;'\"\n"  "!?->~",  /* Maybe '?', too? */
           },
    .vals = { [ ST_OPEN  ] { "“",  sizeof ("“") - 1 },
              [ ST_CLOSE ] { "”",  sizeof ("”") - 1 } },
    .desc = "PRETTY PAIR DOUBLE-QUOTES 2",
    .recurse = KY_RECURSE
};
#undef MY_KEY

#undef MY_KEY
#define MY_KEY "\""
static key_values_t const double_quote_pairs = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = " ,'\"",
             .close_chars = " .,;'\"\n"  "!?->~",  /* Maybe '?', too? */
           },
    .vals = { [ ST_OPEN  ] { "“",  sizeof ("“") - 1 },
              [ ST_CLOSE ] { "”",  sizeof ("”") - 1 } },
    .desc = "PRETTY PAIR DOUBLE-QUOTES",
    .recurse = KY_RECURSE
};
#undef MY_KEY

#undef MY_KEY
#define MY_KEY "'"
static key_values_t const single_quote_pairs = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = " ,'\"",
             .close_chars = " .,;'\"\n"  "!?->~",  /* Maybe '?', too? */
           },
    .vals = { [ ST_OPEN  ] { "‘",  sizeof ("‘") - 1 },
              [ ST_CLOSE ] { "’",  sizeof ("’") - 1 } },
    .desc = "PRETTY PAIR SINGLE-QUOTES",
    .recurse = KY_RECURSE
};
#undef MY_KEY

#undef MY_KEY
#define MY_KEY "'"  /* Also, possessive nouns */
static key_values_t const most_contractions = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = NULL,
             .close_chars = "stvlrmd",
           },
    .vals = { [ ST_OPEN  ] { "’",  sizeof ("’") - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "MOST CONTRACTIONS"
};
#undef MY_KEY

#undef MY_KEY
#define MY_KEY "'"
static key_values_t const plural_possessive_noun = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = "s",
             .close_chars = " ,;",
           },
    .vals = { [ ST_OPEN  ] { "’",  sizeof ("’") - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "PLURAL POSSESSIVE NOUN"
};
#undef MY_KEY

#undef MY_KEY
#define MY_KEY "'"
static key_values_t const y_know_apostrophe = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = "dnyY"   "sr",  /* 15-42 y'can  n'all  d'ya  s'long  d'ya  yarr'~ */
             .close_chars = "achksy" "l~",  /* 10-16 y'hear?  y'knooo~  y'can */
           },
    .vals = { [ ST_OPEN  ] { "’",  sizeof ("’") - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "Y'KNOW ... APOSTROPHE <MISC>"
};
#undef MY_KEY

#undef MY_KEY
#define MY_KEY "'"  /* Tough problem 'cause of single-quote pairs... */
static key_values_t const hafta_yappin_apostrophe = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = "aenoruh",  /* 15-42  hafta'  ta'  eno'  tru'  eh' */
             .close_chars = " .,?!",    /* 16-44  yer'  mockin'  yappin'! */
             .close_strs = (char const *const []) { "』", "」", NULL },
           },
    .vals = { [ ST_OPEN  ] { "’",  sizeof ("’") - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "HAFTA' ... APOSTROPHE <MISC>"
};
#undef MY_KEY

#define MY_KEY "','"
static key_values_t const fix_squotes_spacing = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = NULL,
             .close_chars = NULL,
           },
    .vals = { [ ST_OPEN  ] { "', '",  sizeof ("', '") - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "SINGLE QUOTE SPACING"
};
#undef MY_KEY

#define MY_KEY "--"  /* ― */
static key_values_t const replace_double_dash = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = NULL,
             .close_chars = NULL,
           },
    .vals = { [ ST_OPEN  ] { "──",  sizeof ("──") - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "DOULE-DASH REPLACEMENT"
};
#undef MY_KEY

#define MY_KEY "　"
static key_values_t const replace_wide_space = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = NULL,
             .close_chars = NULL,
           },
    .vals = { [ ST_OPEN  ] { " ",  sizeof (" ") - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "WIDE-SPACE REPLACEMENT"
};
#undef MY_KEY

#if 0
/*******************************************************************************
 * These pairs are to handle triple double-quotes.  We don't want to pretty
 * these double-quote sets, so we'll hide them (encode) and unhide them later.
 *
 * This problem was solved; this is here for historical reference.
 */
#define MY_ENCODE_KEY "\"\"\""
#define MY_DECODE_KEY "\x1B\x07"
static key_values_t const fix_triple_dquote_encode = {
    .key = { .str  = MY_ENCODE_KEY,
             .len  = sizeof (MY_ENCODE_KEY) - 1,
             .open_chars  = NULL,
             .close_chars = NULL,
           },
    .vals = { [ ST_OPEN  ] { MY_DECODE_KEY,  sizeof (MY_DECODE_KEY) - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "TRIPLE DOUBLE-QUOTE ENCODE"
};

static key_values_t const fix_triple_dquote_decode = {
    .key = { .str  = MY_DECODE_KEY,
             .len  = sizeof (MY_DECODE_KEY) - 1,
             .open_chars  = NULL,
             .close_chars = NULL,
           },
    .vals = { [ ST_OPEN  ] { MY_ENCODE_KEY,  sizeof (MY_ENCODE_KEY) - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "TRIPLE DOUBLE-QUOTE DECODE"
};
#undef MY_ENCODE_KEY
#undef MY_DECODE_KEY
#endif


/*******************************************************************************
 * Emoji experiment...
 * 😞😟  <<== I wish I could consistantly get these to work...
 */
#undef MY_KEY
#undef MY_VAL
#define MY_KEY ":("
#define MY_VAL "☹"   /* DM 6-30 */
static key_values_t const emoji_sad = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = NULL,
             .close_chars = NULL,
           },
    .vals = { [ ST_OPEN  ] { MY_VAL,  sizeof (MY_VAL) - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "EMOJI EXPERIMENT"
};
#undef MY_KEY

#undef MY_KEY
#undef MY_VAL
#define MY_KEY ":)"
#define MY_VAL "☺"
static key_values_t const emoji_happy = {
    .key = { .str  = MY_KEY,
             .len  = sizeof (MY_KEY) - 1,
             .open_chars  = NULL,
             .close_chars = NULL,
           },
    .vals = { [ ST_OPEN  ] { MY_VAL,  sizeof (MY_VAL) - 1 },
              [ ST_CLOSE ] { NULL, } },
    .desc = "EMOJI EXPERIMENT"
};
#undef MY_KEY

#endif  /* } PA_ATTRIBUTES_H */

