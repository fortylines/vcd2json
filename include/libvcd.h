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


/* ==== Used by the C/Python wrapper ==== */

typedef enum {
    err_vcd_token = 0,
    whitespace_vcd_token,
    keyword_vcd_token,
    /* declaration keywords */
    comment_vcd_token,
    date_vcd_token,
    enddefinitions_vcd_token,
    scope_vcd_token,
    timescale_vcd_token,
    upscope_vcd_token,
    var_vcd_token,
    version_vcd_token,  /* 10 */
    end_vcd_token,
    data_vcd_token,
    /* simulation keywords */
    dumpall_vcd_token,
    dumpoff_vcd_token,
    dumpon_vcd_token,
    dumpvars_vcd_token,
    sim_time_vcd_token,
    value_change_bit_vcd_token,   /* 18 */
    value_change_binary_vcd_token,
    value_change_real_vcd_token
} vcd_token;


struct definitions_t {
    signal_map *map;
    bool enter_scope;
    bool enter_field;
    vcd_print_callback print;
    void *obj;
    int scope_depth;
    char scope_prefix[FILENAME_MAX];
};


struct simulation_t {
    const signal_map *map;    /* identifier codes we are interested in. */
    size_t current_timestamp;
    size_t start_time;         /* [start_time, end_time[ period we are   */
    size_t end_time;           /* interested in. */
    size_t resolution;
};


struct parser_t {
    char identifier_code[BUFFER_SIZE];
    char broken_token[BUFFER_SIZE];
    size_t broken_token_len;
    size_t broken_token_mark;
    struct definitions_t *defs;
    struct simulation_t *sim;
    void *state;
};


struct tokenizer_t {
    void *state;
    vcd_token tok;
    vcd_token last_significant_tok;
    size_t line_num;
    struct parser_t parser;
};

typedef struct trace_filter_t {
    struct signal_map_t map;
    struct definitions_t defs;
    struct simulation_t sim;
    struct tokenizer_t tokenizer;
} trace_filter;

void
trace_filter_init( struct trace_filter_t *trace,
    size_t start_time, size_t end_time, size_t resolution,
    vcd_print_callback print, void *obj );

void
trace_filter_flush( struct trace_filter_t *trace );

size_t
trace_filter_write( struct trace_filter_t *trace,
    const char *buffer, size_t buffer_length );

#ifdef __cplusplus
}
#endif

#endif
