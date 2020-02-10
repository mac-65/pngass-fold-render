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
# The libass library wasn't intended to be a general purpose text-on-image
# library, but it provides some text niceties that are hard to overlook.
#
# The sed "copy every line into the pattern space" below MUST be first.
# See https://stackoverflow.com/questions/1251999/how-can-i-replace-a-newline-n-using-sed
#
# Replace the diamond character with some nice "ironwork."
:a;N;$!ba;s#\n\n\n◇\n\n\n#\n{\\1a\&H80\\1c\&H333647\&\\fnNatVignetteTwo\\fs220\blur0.8}\xa0\x9ch\x9c{@@1a@@@@1c@@@@face@@@@size@@}\n#
s/\n\n\n◇\n\n\n/\n{\\1a\&H80\\1c\&H333267\&\\fnNatVignetteTwo\\fs220\blur0.8}\xa0\x9cj\x9c{@@1a@@@@1c@@@@face@@@@size@@}\n/
s/\n\n\n◇\n\n\n/\n{\\1a\&H80\\1c\&H633647\&\\fnNatVignetteTwo\\fs200\blur0.8}\xa0\x9c2\x9c{@@1a@@@@1c@@@@face@@@@size@@}\n/g
s/\n\n◇\n\n/\n{\\1a\&H70\\1c\&H00A010\&\\fnNatVignetteTwo\\fscx250\\fs150}\xa0#{\\fscx100@@1a@@@@1c@@@@face@@@@size@@}\n/g
# Tighten up the white space at the start of the chapter
s#\n\n\n\n#\n\n#
s/Meteor Shower/{\\b1}Meteor Shower{\\b0}/g
s#<TLN: ([^>]*)>#<{\\fscx90\\fscy90\\1c\&HD00010\&}TLN:{@@1c@@}\xa0{\\fscx86\\fscy86\\i1}\1{\\i0\\fscx100\\fscy100}>#g
