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
# The Samsung TV has trouble w/3000+ files in a single NTFS directory,
# so I break things up into groups of 200 files / directory.
#

DBG='echo ' ;
DBG=' ' ;
SRC_DIR='JPGs-FULL_RUN' ;
SRC_DIR='JPGs-FULL_NEW' ;
DEST_BASE_DIR='DM' ;
DEST_PATH="/run/media/${USER}/TV_64G/Ms/${DEST_BASE_DIR}" ;
DEST_PATH="/run/media/${USER}/2TBBlue2/Ms/${DEST_BASE_DIR}" ;
PREFIX='p' ;
mkdir -p "${DEST_PATH}" ;

for yy in {00..98..2} ; do
    if [ ! -f "${SRC_DIR}/${PREFIX}${yy}01"*jpg ] ; then
        echo ; echo "FINISHED..." ;
        break ;
    fi

    echo -n "mkdir '${yy}'  " ;
    ${DBG} mkdir -p "${DEST_PATH}/${yy}" ;

    echo -n " cp ${PREFIX}${yy}00.jpg --> ${PREFIX}${yy}99.jpg  '${DEST_PATH}/${yy}'" ;
    ${DBG} /bin/cp -p "${SRC_DIR}/${PREFIX}${yy}"*jpg  "${DEST_PATH}/${yy}" ;
    echo ;

    (( zz = 10#${yy} + 1 )) ; zz=`printf '%.2d' ${zz}` ;

    if [ ! -f "${SRC_DIR}/${PREFIX}${zz}01"*jpg ] ; then echo "FINISHED..." ;
        break ;
    fi

    echo -n "             cp ${PREFIX}${zz}00.jpg --> ${PREFIX}${zz}99.jpg  '${DEST_PATH}/${yy}'" ;
    ${DBG} /bin/cp -p "${SRC_DIR}/${PREFIX}${zz}"*jpg  "${DEST_PATH}/${yy}" ;
    echo ;
done

