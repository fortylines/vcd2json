/* Copyright (c) 2012-2014, Fortylines LLC
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

#ifndef guardvcd2json
#define guardvcd2json

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef __VCD2JSON_VERSION__
#define __VCD2JSON_VERSION__ "dev"
#endif

#define BUFFER_SIZE           4096


#ifdef __cplusplus
extern "C" {
#endif

/** Buffer used to store an extracted signal trace.
 */
typedef struct signal_buf_t {
    struct signal_buf_t *next;
    const char *name;
    bool not_first_record;
    size_t initial_change_record_timestamp;
    size_t initial_change_record_length;
    char initial_change_record_value_change[BUFFER_SIZE];
    char text[BUFFER_SIZE];
} signal_buf;

/** Insert a new *name*d signal into an alphabetically-ordered linked list.
    This function returns a pointer to the head of the list.
 */
signal_buf *insert_signal( signal_buf *head, char *name );


typedef struct signal_map_entry_t {
    uint32_t key;
    signal_buf *timeline;
} signal_map_entry;


typedef struct signal_map_t {
    signal_buf *head;
    signal_map_entry entries[256];
} signal_map;


/** On return, the signal map will be ready to accept insertions
    of signal names and keys.
 */
void init_signal_map( signal_map *map );

/** On return, all allocated memory for the signal map will be freed.
 */
void destroy_signal_map( signal_map *map );

/** Insert the short key symbol used to represent a signal declared
    as *name* in the timeline.
 */
int insert_short_key( signal_map *map, const char *name, const char *key );


/** Find the timeline associated to a short key.
 */
signal_buf* find_timeline( const signal_map *map,
    const char *key, size_t key_len );


typedef void (*vcd_print_callback)( void* obj, const char *buffer, size_t len );


/**
   Prints the header information and definitions in a VCD file *from*
   as a json formatted string using the *print* callback. *obj* is
   a callback parameter passed through "as is" to *print* on every
   callback.
 */
void header_and_definitions( FILE *from, signal_map *map,
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
   { ".board.clock": [
            [0, "x"],
            [50, "1"],
            [100, "0"]
          ]
   }
 */
void value_changes( FILE *from, const signal_map *map,
    size_t start_time, size_t end_time, size_t resolution );


/**
   Prints header definitions and values over time in a VCD file *from*
   as a json formatted string using the *print* callback. *obj*
   is a callback parameter.
 */
void filter_value_changes( FILE *from, signal_map *map,
    size_t start_time, size_t end_time, size_t resolution,
    vcd_print_callback print, void *obj );

#ifdef __cplusplus
}
#endif

#endif
