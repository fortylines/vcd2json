/* Copyright (c) 2012-2014, Fortylines LLC
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libvcd.h"

static void
stdout_print( void* obj, const char *buffer, size_t len )
{
    fwrite(buffer, 1, len, stdout);
}

int main( int argc, char *argv[] )
{
    size_t resolution = 1;
    size_t start_time = 0;
    size_t end_time = 0;
    signal_map map;
    char input_path[FILENAME_MAX];

    init_signal_map(&map);

    int argi = 1;
    while( argi < argc ) {
        if( strncmp(argv[argi], "-h", 2) == 0
            || strncmp(argv[argi], "--help", 6) == 0 ) {
            printf("%s [options] vcdfile\n", argv[0]);
            printf("version %s\n", __VCD2JSON_VERSION__);
            printf("Copyright (c) 2012-2014, Fortylines LLC\n\n");
            printf("-n, --name str        "\
                "variable name to include in json output\n");
            printf("-s, --start int       "\
                "start of timeframe to include in json output\n");
            printf("-e, --end int         "\
                "end of timeframe to include in json output\n");
            printf("-r, --resolution int  "\
                "number of timestamps per pixel\n");
            return 0;
        }
        if( strncmp(argv[argi], "-n", 2) == 0
            || strncmp(argv[argi], "--name", 6) == 0 ) {
            ++argi;
            if( argi >= argc ) {
                fprintf(stderr,
                    "error: missing symbol argument after %s", argv[argi - 1]);
                return 1;
            }
            map.head = insert_signal(map.head, argv[argi++]);
        } else if( strncmp(argv[argi], "-s", 2) == 0
            || strncmp(argv[argi], "--start", 7) == 0 ) {
            ++argi;
            if( argi >= argc ) {
                fprintf(stderr,
                    "error: missing time argument after %s", argv[argi - 1]);
                return 1;
            }
            start_time = atoi(argv[argi++]);
        } else if( strncmp(argv[argi], "-e", 2) == 0
            || strncmp(argv[argi], "--end", 5) == 0 ) {
            ++argi;
            if( argi >= argc ) {
                fprintf(stderr,
                    "error: missing time argument after %s", argv[argi - 1]);
                return 1;
            }
            end_time = atoi(argv[argi++]);
        } else if( strncmp(argv[argi], "-r", 2) == 0
            || strncmp(argv[argi], "--resolution", 12) == 0 ) {
            ++argi;
            if( argi >= argc ) {
                fprintf(stderr,
                    "error: missing integer ratio argument after %s",
                    argv[argi - 1]);
                return 1;
            }
            resolution = atoi(argv[argi++]);
        } else {
            strncpy(input_path, argv[argi++], FILENAME_MAX);
            if( argi < argc ) {
                fprintf(stderr,
                  "warning: arguments after input filename are being ignored.");
            }
            break;
        }
    }

    FILE *from = fopen(input_path, "r");
    if( !from ) {
        fprintf(stderr, "error: unable to open %s\n", argv[1]);
        return 1;
    }

#ifdef LOGENABLE
    fprintf(stderr,
        "(timestamp, value) in [%ld, %ld[ with resolution %ld for\n",
        start_time, end_time, resolution);
    signal_buf *curr = map.head;
    while( curr ) {
        fprintf(stderr, "\t%s\n", curr->name);
        curr = curr->next;
    }
#endif

    filter_value_changes(from, &map, start_time, end_time, resolution,
        stdout_print, NULL);

    destroy_signal_map(&map);
    return 0;
}
