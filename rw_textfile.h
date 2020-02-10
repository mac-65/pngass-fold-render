#ifndef READ_TEXTFILE_H
#define READ_TEXTFILE_H

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

/**
 *******************************************************************************
 */

#define INITIAL_BUF_SIZE  ( 16 * 1024 )

#undef READ_STDIN
#define READ_STDIN ":stdin:"

char *read_textfile_sz(char const *const filename, size_t *len, size_t sz);
char *read_stream_sz(FILE *file, size_t *len, size_t buf_size);

#undef read_textfile
#define read_textfile(fn, len)                                  \
        read_textfile_sz((fn), (len), INITIAL_BUF_SIZE)

#undef read_stream
#define read_stream(fi, len)                                    \
        read_stream_sz((fi), (len), INITIAL_BUF_SIZE)

#endif  /* READ_TEXTFILE_H */

