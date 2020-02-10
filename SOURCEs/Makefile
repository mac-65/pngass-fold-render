
###############################################################################
# Copyright (C) 2018
#
# This Makefile file is part of pngass.
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
# This Makefile is builds the patched version of 'html2text' from source.
# Note, it does NOT try to install it, but builds it for local use by the demo.
#
# For Fedora builds, ensure we're using the "original" GCC compiler (in case
# a newer version of gcc was installed - seemed incompatible).
#
# For Darwin, ensure that libiconv is installed (brew install libiconv).
#
#( cd html2text-1.3.2a && ./configure && make LDFLAGS='-liconv' CXX=${CXX} )
###############################################################################
ARCH=$(shell uname)
CXX=g++
LDFLAGS=
ifeq ($(ARCH),Darwin)
  CXX=/usr/local/bin/g++-8
  LDFLAGS=-liconv
endif
ifeq ($(ARCH),Linux)
  CXX=/bin/g++
endif


default : clean
	tar xf html2text-1.3.2a.tar.gz
	for yy in *.patch ; do patch -p0 < "$${yy}" ; done
	( cd html2text-1.3.2a && ./configure && make LDFLAGS="${LDFLAGS}" CXX="${CXX}" )


clean :
	/bin/rm -rf html2text-1.3.2a

