/* Copyright (c) 2012-2013, Fortylines LLC
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of fortylines nor the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY Fortylines LLC ''AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL Fortylines LLC BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#ifndef guardlibvcd
#define guardlibvcd

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*vcd_print_callback)( void* obj, const char *buffer, size_t len );

/**
   Prints the header information and definitions in a VCD file *from*
   as a json formatted string using the *print* callback. *obj* is
   a callback parameter passed through "as is" to *print* on every
   callback.
 */
void header_and_definitions( FILE *from,
    vcd_print_callback print, void *obj );


/**
   Prints values over time in a VCD file *from* as a json formatted
   string using the *print* callback. *obj* is a callback parameter
   passed through "as is" to *print* on every callback.

   The output will contain a list (timestamp, value) pairs for *identifier_code*
   such that each timestamp is in [*start_time*, *end_time[.

   *resolution* indicates a timestamp per pixel ratio. This function will skip
   records in the VCD file that would display to the same pixel.

   ex:
   { "!": [ [0, "0"],
            [50, "1"],
            [100, "0"]
          ]
   }
 */
void value_changes( FILE *from, const char *identifier_code,
    size_t start_time, size_t end_time, size_t resolution,
    vcd_print_callback print, void *obj );

#ifdef __cplusplus
}
#endif

#endif
