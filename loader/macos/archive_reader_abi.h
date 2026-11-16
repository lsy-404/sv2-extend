/* Public libarchive 3.x reader declarations used with macOS libarchive.
 * Adapted from libarchive v3.7.7 archive.h and archive_entry.h.
 * Copyright (c) 2003-2010 Tim Kientzle
 * Copyright (c) 2016 Martin Matuska
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef EXTEND_ARCHIVE_READER_ABI_H
#define EXTEND_ARCHIVE_READER_ABI_H
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdint.h>
struct archive;
struct archive_entry;
typedef ssize_t la_ssize_t;
#define ARCHIVE_OK 0
#define ARCHIVE_EOF 1
#define AE_IFDIR S_IFDIR
#define AE_IFREG S_IFREG
struct archive *archive_read_new(void);
int archive_read_support_format_zip(struct archive *);
int archive_read_open_memory(struct archive *,const void *,size_t);
int archive_read_next_header(struct archive *,struct archive_entry **);
const char *archive_entry_pathname(struct archive_entry *);
const char *archive_entry_symlink(struct archive_entry *);
const char *archive_entry_hardlink(struct archive_entry *);
mode_t archive_entry_filetype(struct archive_entry *);
int64_t archive_entry_size(struct archive_entry *);
la_ssize_t archive_read_data(struct archive *,void *,size_t);
int archive_read_data_skip(struct archive *);
int archive_read_free(struct archive *);
#endif
