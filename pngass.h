#ifndef PNGASS_H
#define PNGASS_H
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

#undef ASS_HARD_BREAK
#define ASS_HARD_BREAK        "\\N"
#undef ASS_SPACE_HARD_BREAK
#define ASS_SPACE_HARD_BREAK  " \\N"  /* A workaround for a libass "feature" */

#undef ASS_ATTRIBUTE_START
#define ASS_ATTRIBUTE_START   "{\\"
#undef ASS_ATTRIBUTE_END
#define ASS_ATTRIBUTE_END     "}"

#undef PA_UNSET_VALUE
#define PA_UNSET_VALUE        (-42)


#undef PA_SED_EXEC
#define PA_SED_EXEC "sed"
#ifdef PA_ARCH_DARWIN
   #undef PA_SED_EXEC
   #define PA_SED_EXEC "gsed"  /* "/usr/local/bin/gsed" */
#endif  /* PA_ARCH_DARWIN */

#endif   /* PNGASS_H */
