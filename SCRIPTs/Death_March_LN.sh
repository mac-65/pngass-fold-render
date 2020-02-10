#! /bin/bash

###############################################################################
# Copyright (C) 2020
#
# This file is part of pngass.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Simple (?) script to illustrate how to grab a LN from the web.  It's messy,
# but functional, but if the site changes slightly, then somtimes a simple
# edit or an entire rewrite is necessary.  All part of the hobby, I suppose…
#

# . "${HOME}/C/sh_colors/sh_colors.sh" ; # defines the 'ATTR_*' variables.

# sed -ne ':a;N;$!ba;s/.*<title>\n\([^<]*\).*/\1/p' death-march-kara-hajimaru-isekai_16.html

# grep  -o 'href="http://[^/]*/20../../death[^"]*"' 'Death March kara Hajimaru Isekai Kyousoukyoku | Sousetsuka.html' \
#     | egrep --color=always '2015/02|$'

###############################################################################

HTML2TEXT='/bin/html2text' ;
RECODE='/bin/recode' ;
SED='/bin/sed' ;
EGREP='egrep' ;

TEXT_DIR='./TEXTs' ;
HTML_DIR='./Html' ;

###############################################################################
# DISCLAIMER :: I'm not a /bin/sed expert, just know some stuff about regexp. #
###############################################################################

###############################################################################
# These values are retrieved from Firefox.
#
TOP_LEVEL_BLOG_PAGE='blog-page_11.html' ;
TOP_LEVEL_BLOG_HTTP="http://www.sousetsuka.com/p/${TOP_LEVEL_BLOG_PAGE}" ;

###############################################################################
# Clean and get the top-level document which has all of the chapter titles.
#
if false ; then
    /bin/rm -f "${TOP_LEVEL_BLOG_PAGE}" ;
    wget -c "${TOP_LEVEL_BLOG_HTTP}" ;
fi

mkdir -p "${HTML_DIR}" ;
mkdir -p "${TEXT_DIR}" ;


###############################################################################
# Pull down ALL of the pages and rename them to their document's <title>.
# (I think there might be a better/cleaner way to do this - on my TODO list.)
#
# I think these are all of the variations:
#   <a href="http://www.sousetsuka.com/2015/05/6-3.html">Chapter 6-3</a><br />
#   <a href="http://www.sousetsuka.com/2015/05/death-march-kara-hajimaru-isekai_8.html">Chapter 6-4</a><br />
#   <a href="http://www.sousetsuka.com/2015/06/chapter-6-intermission-1.html">Chapter 6 Intermission 1</a><br />
#
if false ; then
CNT=0;
${EGREP} -o 'href="http://[^/]*/20../../death[^"]*"|href="http://[^/]*/20../../chapt[^"]*"|href="http://[^/]*/20../../[1-9][^"]*"' \
            "${TOP_LEVEL_BLOG_PAGE}" \
    | sed -e 's/href="//' -e 's/"$//' \
    | while read HTTP ; do
        RAW_DOCUMENT="`basename \"${HTTP}\"`" ;
        /bin/rm -f "${RAW_DOCUMENT}" ;
        echo -en "${ATTR_BOLD}wget${ATTR_OFF} ${RAW_DOCUMENT} ." ;

        wget -c "${HTTP}" >/dev/null 2>&1 ; RC=$? ;
        if [[ RC -ne 0 ]] ; then
           echo -e  ". ${ATTR_RED_BOLD}FAILED!${ATTR_OFF}" ;
        else
           echo -en ". ${ATTR_GREEN_BOLD}okay${ATTR_OFF} ." ;
        fi

        #######################################################################
        # Extract the <title> that is used for the destination filename, and:
        #  - cleanup author specific stuff (e.g. Sousetsuka),
        #  - cleanup any html (&#12304;) characters using 'recode',
        #  - and remove leading SPACEs from the title.
        #
        TITLE="`sed -ne ':a;N;$!ba;s/.*<title>\n\([^<]*\).*/\1/p' \"${RAW_DOCUMENT}\" \
              | sed  -e 's/ | Sousetsuka//'  \
                     -e 's/Chapter //'       \
                     -e 's/^[ ]*//'          \
              | ${RECODE} 'html..utf-8'      \
              `.html" ;
        echo -en "." ;

        (( CNT += 1 )) ;
        SEQUENCE="`printf \"%.4d\" ${CNT}`" ;
        echo -en ". '${ATTR_MAGENTA_BOLD}${SEQUENCE}${ATTR_CLR_BOLD}-${ATTR_YELLOW_BOLD}${TITLE}${ATTR_OFF}' ." ;
        /bin/mv "${RAW_DOCUMENT}" "${HTML_DIR}/${SEQUENCE}-${TITLE}" ;
        echo '.' ;

        if [[ CNT -eq 9999 ]] ; then
           exit ;  # ...debugging, stop after X pages, else make it '9999'.
        fi
    done ;
fi


###############################################################################
# GET UPDATES
#
if false ; then
for HREF in \
    '<a href="http://www.sousetsuka.com/2018/10/death-march-kara-hajimaru-isekai.html">Chapter 16-66</a><br />'           \
    '<a href="http://www.sousetsuka.com/2018/10/death-march-kara-hajimaru-isekai_8.html">Chapter 16-67</a>&nbsp; </div>'  \
    ; do echo "${HREF}" \
    | ${EGREP} -o 'href="http://[^/]*/20../../death[^"]*"|href="http://[^/]*/20../../chapt[^"]*"|href="http://[^/]*/20../../[1-9][^"]*"' \
    | sed -e 's/href="//' -e 's/"$//' \
    | while read HTTP ; do
        RAW_DOCUMENT="`basename \"${HTTP}\"`" ;
        /bin/rm -f "${RAW_DOCUMENT}" ;
        echo -en "${ATTR_BOLD}wget${ATTR_OFF} ${RAW_DOCUMENT} ." ;

        wget -c "${HTTP}" >/dev/null 2>&1 ; RC=$? ;
        if [[ RC -ne 0 ]] ; then
           echo -e  ". ${ATTR_RED_BOLD}FAILED!${ATTR_OFF}" ;
        else
           echo -en ". ${ATTR_GREEN_BOLD}okay${ATTR_OFF} ." ;
        fi

        #######################################################################
        # Extract the <title> that is used for the destination filename, and:
        #  - cleanup author specific stuff (e.g. Sousetsuka),
        #  - cleanup any html (&#12304;) characters using 'recode',
        #  - and remove leading SPACEs from the title.
        #
        TITLE="`sed -ne ':a;N;$!ba;s/.*<title>\n\([^<]*\).*/\1/p' \"${RAW_DOCUMENT}\" \
              | sed  -e 's/ | Sousetsuka//'  \
                     -e 's/Chapter //'       \
                     -e 's/^[ ]*//'          \
              | ${RECODE} 'html..utf-8'      \
              `.html" ;
        echo -en "." ;

        (( CNT += 1 )) ;
        SEQUENCE="`printf \"%.4d\" ${CNT}`" ;
        echo -en ". '${ATTR_MAGENTA_BOLD}${SEQUENCE}${ATTR_CLR_BOLD}-${ATTR_YELLOW_BOLD}${TITLE}${ATTR_OFF}' ." ;
        /bin/mv "${RAW_DOCUMENT}" "${HTML_DIR}/${SEQUENCE}-${TITLE}" ;
        echo '.' ;

        if [[ CNT -eq 9999 ]] ; then
           exit ;  # ...debugging, stop after X pages, else make it '9999'.
        fi
    done ;
  done ;
fi


###############################################################################
# Use html2text to perform a basic conversion (we'll still need to clean this
# up a little).  (This process works for _this_ site, but other sites may use
# a different backend and require a different set of steps.)  These steps are:
#
# This is an imperfect solution, but the results are very usable.
#
# + Identify and mark (using '@@@@') the chapter's title.  The chapter's title
#   is marked with the <h3> heading and is converted to the string '**** ' by
#   'html2text'.  Sometimes, additional <h3> headings appear, so we have to
#   mark only the first <h3> heading which contains the chapter's title.
# - Convert all non-breaking SPACEs to SPACEs.
# - Convert _any_ BOM / UTF-8 signature to a SPACE.  These (?) appear as the
#   <feff> _signature_ in vim, but really is the byte sequence 0xEF 0xBB 0xBF.
#
# + Remove everything up to the (previously marked) chapter title.
#
# + Remove the <h3> heading markers.
# - Remove trailing SPACEs (this also has the effect of making lines that are
#   all SPACEs a simple '\n').
# - MANUALLY replace '&amp;' with '&'.  Dunno why html2text misses this...
# - Remove everything after the:
#     'Next_Chapter', 'Previous_Chapter', or 'Edited by:' markers.
# - Delete any lines containing 'Next update is'.  Sometimes there's a
#   footnote after the line, so it can't be included with the delete for the
#   Next / Previous chapter.
#
# + Compress long runs of EMPTY lines - 5 to 32 lines are compressed to 3
#   (these usually appear at the end of a chapter).  (This is visually 4 to 31
#   because the '\n' of the previous line is included in the count.)
###############################################################################
#
if true ; then
NEED_NL=0 ;
for HTML_IN in "${HTML_DIR}"/*html ; do
    TEXT_OUT="${TEXT_DIR}/`basename \"${HTML_IN}\" '.html'`.txt" ;

    #- if [ './Html/0027-Death March kara Hajimaru Isekai Kyusoukyoku 3-3.html' != "${HTML_IN}" ] ; then continue; fi

    ###########################################################################
    # If the '.txt' files isn't there, then build it.
    #
    if [ ! -f "${TEXT_OUT}" ] ; then
      if [ ${NEED_NL} -eq 1 ] ; then echo ; fi ;
      echo "${ATTR_YELLOW}${HTML_IN}${ATTR_OFF} --> '${ATTR_BLUE_BOLD}${TEXT_OUT}${ATTR_OFF}'" ;
      ${HTML2TEXT} -width 8192 -nobs "${HTML_IN}"                  \
          | ${SED}  -e ':a;N;$!ba;s/\n[*][*][*][*] /\n@@@@ /'      \
                    -e 's/\xC2\xA0/ /g'                            \
                    -e 's/\xEF\xBB\xBF/ /g'                        \
          | ${SED} -ne ':a;N;$!ba;s/.*\n\(@@@@ .*\)/\1/p;'         \
          | ${SED}  -e 's#^[*@][*@][*@][*@] [ ]*\(.*\)[*][*][*][*]#\1#'  \
                    -e 's/[ ]*$//'                                 \
                    -e 's/&amp;/\&/g'                              \
                    -e '/Next_Chapter\|Previous_Chapter\|[Ee]dited by[ ]*:/,+9999d;'  \
                    -e '/Next update is\|New chapters/,+d;'        \
          | ${SED}  -e ':a;N;$!ba;s/\n\{5,32\}/\n\n\n/g;'          \
          > "${TEXT_OUT}"
      NEED_NL=0 ;
    else
      NEED_NL=1 ; echo -n '.' ;
    fi
done
echo ;
fi

exit

