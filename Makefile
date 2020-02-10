
###############################################################################
# Copyright (C) 2018
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

###############################################################################
# A simple, basic Makefile
#
# This Makefile was updated from Fedora 21 to Fedora 27 with some minor fixes.
#
# HTML2TEXT might be a little dated, but should be okay for the 'demo' target.
# (Actually, Fedora includes a patched version, but for Darwin, I have to
# build a patched version to get the needed fixes and UTF-8 support.)
###############################################################################

ARCH=$(shell uname)
ifeq ($(ARCH),Darwin)
  CC=gcc-8 -g -std=gnu11 -DPA_ARCH_DARWIN
  CINCLS=
  MD5SUM=gmd5sum
  WGET=/usr/local/bin/wget
  TOUCH=gtouch
  CONVERT=convert
  UNZIP=unzip
  HTML2TEXT=bin/html2text
  SED=gsed
  AWK=awk
  HEAD=ghead
  ARCH_KNOWN=yes
endif
ifeq ($(ARCH),Linux)
  CC=gcc -g -std=gnu11 -DPA_GCC_BUILTINS
  CINCLS=-I.
  MD5SUM=md5sum
  WGET=wget
  TOUCH=/bin/touch
  CONVERT=convert
  UNZIP=unzip
  HTML2TEXT=bin/html2text
  SED=sed
  AWK=awk
  HEAD=head
  ARCH_KNOWN=yes
endif


###############################################################################
# Just some NON-XXX non-licensed fan-art for the image backgrounds ...
#
PRETTY_GIRLS_URL=https://images.wallpaperscraft.com/image
DECORATIVE_FONT_URL=http://fonts3.bj.bcebos.com
A_LIGHT_NOVEL_URL=http://www.sousetsuka.com/2015/01


###############################################################################
# NOTE :: gcc version 4.9.2 20150212 (Red Hat 4.9.2-6) (GCC)
#         Doesn't catch unused scoped functions like it should but gcc-8 on
#         Darwin does produce a warning -->
#           rw_imagefile.c:673:10: warning: 'cleanup_row_pointers' defined but not used [-Wunused-function]
#         gcc version 8.2.0 (Homebrew GCC 8.2.0)
#
CFLAGS=-O3 -Wall -Wextra -Wshadow -Wunused-function -DENABLE_JPEG_RW ${CINCLS}
LDFLAGS=-lpng -L/usr/local/lib -lass -ljpeg


PNGASS=pngass


###############################################################################
# If these images are NOT available, any 1920x1080 JPG will work for the demo
# (this Makefile will need to be edited, however).
#
DEMO_DIR=./DEMOs
DEMO_IN_JPGs=${DEMO_DIR}/girl_anime_grass_lying_art_107288_1920x1080.jpg                                                                            \
             ${DEMO_DIR}/neko_yanshoujie_room_girl_graphic_hand_headphones_easel_shape_books_food_camera_lamp_chair_decor_94921_1920x1080.jpg       \
             ${DEMO_DIR}/art_zero_girl_hatsune_miku_mood_smile_headphones_music_school_uniforms_textbooks_laptop_room_vocaloid_94314_1920x1080.jpg  \
             ${DEMO_DIR}/bouno_satoshi_in_tokyo_otaku_mode_bouno_satoshi_girl_anime_art_98879_1920x1080.jpg                                         \
             ${DEMO_DIR}/anime_girl_hair_headphones_sadness_fence_13305_1920x1080.jpg
DEMO_FILTERED=$(DEMO_IN_JPGs:.jpg=.png)

FONT_DIR=./FONTs
DEMO_IN_FONTs=${FONT_DIR}/NatVignetteTwo.zip
DEMO_FONT_ZIP=$(DEMO_IN_FONTs:.zip=.ttf)

TEXT_FILE=death-march-kara-hajimaru-isekai
TEXT_DIR=./TEXTs
DEMO_IN_TEXTs=${TEXT_DIR}/${TEXT_FILE}.html
DEMO_TEXT=$(DEMO_IN_TEXTs:.html=.txt)
DEMO_OUT_JPG=./JPGs


###############################################################################
###############################################################################
#
default : arch-check ${PNGASS}


demo : arch-check ${PNGASS} ${HTML2TEXT}      \
                 ${DEMO_DIR} ${DEMO_FILTERED} \
                 ${FONT_DIR} ${DEMO_FONT_ZIP} \
                 ${TEXT_DIR} ${DEMO_TEXT}     \
                 run-demo


arch-check :
ifneq ($(ARCH_KNOWN),yes)
	$(error Unknown target architecture :: "${ARCH}")
endif


###############################################################################
#
CC_TEST=${CC} ${CFLAGS} -DTESTING -I.

SRCS=pngass.c  rw_imagefile.c  rw_textfile.c  rw_arrays.c  pa_misc.c  pa_edits.c
OBJS=$(SRCS:.c=.o)


${PNGASS} : ${OBJS}
	${CC} ${LDFLAGS} ${OBJS} -o $@


pngass.o : pngass.h  rw_textfile.h  rw_imagefile.h  rw_arrays.h  pa_misc.h  pa_edits.h


rw_imagefile.o : rw_imagefile.h


rw_textfile.o : rw_textfile.h


rw_arrays.o : rw_arrays.h  rw_textfile.h  pa_misc.h


pa_misc.o : pa_misc.h


pa_edits.o : pa_edits.h  pa_misc.h


clean : demo-clean text-clean
	/bin/rm -f ${DEMO_OUT_JPG}/*jpg
	/bin/rm -f ${OBJS}
	/bin/rm -f ${PNGASS}
	-/bin/rmdir ${DEMO_DIR} 2>/dev/null
	-/bin/rm -rf ${PNGASS}.dSYM 2>/dev/null
	/bin/rm -f ${TEXT_DIR}/*.html
	-/bin/rmdir ${TEXT_DIR}


demo-clean :
	/bin/rm -f ${DEMO_FILTERED}
	/bin/rm -f ${HTML2TEXT}
	( cd SOURCEs && make clean )


text-clean :
	/bin/rm -f ${TEXT_DIR}/${TEXT_FILE}.txt


distclean : clean
	/bin/rm -f ${DEMO_IN_JPGs}
	/bin/rm -f ${DEMO_IN_TEXTs}


###############################################################################
# Simple testing target background images (needs an internet connection).
# Specifying the rules like _this_ iteratively applies the rule(s)
# for each image (i.e., a 'wget the file, convert the file' loop).
#
%.png : %.jpg
	${CONVERT} "$<" -fill grey -colorize '85%' "$@"


${DEMO_IN_JPGs} :
	( cd "${DEMO_DIR}" && ${WGET} -c "${PRETTY_GIRLS_URL}/$(shell basename $@)" )


###############################################################################
# Fonts...  No guarantee that this will always be available.  This font can't
# be included in the distribution because of its license uncertainty.
#
# Tidbit :: archive files' timestamps play havoc with Makefile rules.
#
%.ttf : %.zip
	( cd ${FONT_DIR} && ${UNZIP} "$(shell basename $<)" && ${TOUCH} "$(shell basename $< .zip).ttf" )


${DEMO_IN_FONTs} :
	( cd "${FONT_DIR}" && ${WGET} -c "${DECORATIVE_FONT_URL}/n/20161109/$(shell basename $@)" )


###############################################################################
# A sample text file from an online light novel.
#
# Note, it's complicated to take an arbitrary html page and accurately convert
# it to a nice well-behaved text file.  So many cheats are used in this demo
# to achieve the results I wanted.
#
# This is an iterative process -- I check a few samples, build a script, and
# run all of the pages through the script(s).  If there are exceptions, I fix
# the script to handle those additional cases.  Point being, every browser's
# save-as text page function is slightly different, and every LN site is more
# likely using a different backend engine to generate their html documents.
# So be prepared for some experimenting to get things "right."
#
# I set the width very large so that 'pngass' will do its own line folding.
#
%.txt : %.html
	${HTML2TEXT} -width 8192 -nobs "$<"         \
	  | ${SED} -ne "/Death March/,\$$p"         \
	  | ${SED}  -e 's/[*]*//g' -e 's/[ ]*$$//'  \
	  | ${AWK} "NR==1, /Next_Chapter/ {print}"  \
	  | ${HEAD} --lines=-1                      \
	> "$@"


${DEMO_IN_TEXTs} :
	( cd "${TEXT_DIR}" && ${WGET} -c "${A_LIGHT_NOVEL_URL}/$(shell basename $@)" && ${TOUCH} $(shell basename $@) )


###############################################################################
# This is a handy tool that disappeared from the Fedora distros 😞.
#
${HTML2TEXT} :
	mkdir -p bin
	( cd SOURCEs && make )
	ln SOURCEs/html2text-1.3.2a/html2text "$@"


###############################################################################
# This is the section that is the result of all of these Makefile rules!
#
run-demo : JPGs pngass
	/bin/rm -f ${DEMO_OUT_JPG}/*jpg
	./pngass -t TEMPLATEs/left_template.ass \
		--template="TEMPLATEs/right_template.ass" \
		--header-template='TEMPLATEs/header.ass' \
		--fonts-dir="${FONT_DIR}" \
		--dest-dir="${DEMO_OUT_JPG}" \
		-B 12 \
		--line-spacing=5.0 \
		--jpeg-quality=95 \
		--text-size=38 \
		--pad-paragraph=56 \
		--text-face='Arial' \
		--1c-colour='080808' \
		--input-file='./TEXTs/death-march-kara-hajimaru-isekai.txt' \
		--png-glob='DEMOs/*.png' \
		--xy-format=roman \
		--script-file='SCRIPTs/script1.sed'


###############################################################################
#
${DEMO_DIR} ${FONT_DIR} ${TEXT_DIR} ${DEMO_OUT_JPG} :
	mkdir -p "$@"

