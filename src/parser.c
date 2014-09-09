/* Copyright (c) 2014, Fortylines LLC
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

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "libvcd.h"

#define NAMESPACE_SEP        '/'

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
    size_t line_num;
    struct parser_t parser;
};


static int
append_to_prefix( char *scope_prefix, const char *src, size_t len )
{
    if( len > 0 ) {
        size_t s1_len = strlen(scope_prefix);
        if( s1_len + 2 + len >= FILENAME_MAX ) {
            fprintf(stderr,
                "error: symbol name is more than %d characters, too long!",
                FILENAME_MAX);
            return 1;
        }
        if( s1_len > 0 && src[0] != '[' ) {
            scope_prefix[s1_len++] = NAMESPACE_SEP;
        }
        memcpy(&scope_prefix[s1_len], src, len);
        scope_prefix[s1_len + len] = '\0';
    }
    return 0;
}

static void
remove_last_prefix( char *scope_prefix ) {
    char *end_ptr = strrchr(scope_prefix, NAMESPACE_SEP);
    if( end_ptr ) {
        *end_ptr = '\0';
    } else {
        scope_prefix[0] = '\0';
    }
}


static void
init_tokenizer( struct tokenizer_t *tokenizer,
    struct definitions_t *defs, struct simulation_t *sim )
{
    tokenizer->state = NULL;
    tokenizer->tok = err_vcd_token;
    tokenizer->line_num = 0;
    tokenizer->parser.state = NULL;
    tokenizer->parser.identifier_code[0] = '\0';
    tokenizer->parser.broken_token_len = 0;
    tokenizer->parser.defs = defs;
    tokenizer->parser.sim = sim;
}


static void
init_definitions( struct definitions_t *defs,
    signal_map *map, vcd_print_callback print, void *obj )
{
    defs->enter_scope = true;
    defs->enter_field = true;
    defs->scope_depth = 0;
    defs->map = map;
    defs->print = print;
    defs->obj = obj;
    memset(defs->scope_prefix, 0, FILENAME_MAX);
}


static void
init_simulation( struct simulation_t *sim, const signal_map *map,
    size_t start_time, size_t end_time, size_t resolution )
{
    sim->current_timestamp = 0;
    sim->map = map;
    sim->start_time = start_time;
    sim->end_time = end_time;
    sim->resolution = resolution;
}

static void
set_identifier_code( struct parser_t *parser,
    const char *buffer, size_t start, size_t last )
{
    size_t length = last - start;
    assert( length < sizeof(parser->identifier_code) - 1);
    strncpy(parser->identifier_code, &buffer[start], length);
    parser->identifier_code[length] = '\0';
}

static void
set_value_change( signal_buf *timeline,
    size_t timestamp, const char *buffer, size_t start, size_t last )
{
    timeline->initial_change_record_timestamp = timestamp;
    timeline->initial_change_record_length = last - start;
    assert( timeline->initial_change_record_length
        < sizeof(timeline->initial_change_record_value_change) - 1);
    strncpy(timeline->initial_change_record_value_change,
        &buffer[start], timeline->initial_change_record_length);
    timeline->initial_change_record_value_change[
        timeline->initial_change_record_length] = '\0';
}


static size_t
as_timestamp( const char *buffer, size_t start, size_t last )
{
    size_t i;
    size_t timestamp = 0;
    for( i = start + 1; i < last; ++i ) {
        timestamp *= 10;
        timestamp += buffer[i] - '0';
    }
    return timestamp;
}


static void
print_timestamp_and_value( signal_buf *timeline,
    size_t timestamp, const char *value_change, size_t length )
{
    // XXX Inefficient code here!
    size_t len = strlen(timeline->text);
    snprintf(&timeline->text[len], BUFFER_SIZE - len, "[%zd, \"", timestamp);
    len = strlen(timeline->text);
    if( !(length > BUFFER_SIZE - len) ) {
        memcpy(&timeline->text[len], value_change, length);
    }
    strncat(timeline->text, "\"]", BUFFER_SIZE);
}


static bool is_data_token( vcd_token tok ) {
    return tok == data_vcd_token
        | tok == sim_time_vcd_token
        | tok == keyword_vcd_token
        | tok == value_change_bit_vcd_token
        | tok == value_change_binary_vcd_token
        | tok == value_change_real_vcd_token;
}

static void
escape_identifier_code( const char *ident,
    vcd_print_callback print, void *obj )
{
    const char *p = ident;
    print(obj, "\"", 1);
    while( *p ) {
        /* IEEE Verilog 2001 (18.2.1):
           "identifier code is a code composed of the printable characters
           which are in the ASCII character set from ! to ~ (decimal 33
           to 126)" and escaped characters from http://json.org.  */
        if( *p == '"' | *p == '\\' | *p == '/' ) print(obj, "\\", 1);
        print(obj, p++, 1);
    }
    print(obj, "\"", 1);
}


static void
print_identifier_code( struct definitions_t *defs, const char *ident )
{
    insert_short_key(defs->map, defs->scope_prefix, ident);
    escape_identifier_code(ident, defs->print, defs->obj);
    remove_last_prefix(defs->scope_prefix);
}


/** Meta information like "date", "version", "timescale", etc.

    $score, ie. variable definitions, are handled separately through
    print_enter_scope, print_exit_scope, print_enter_var, print_exit_var.
 */
static void
print_field_name( struct definitions_t *defs,
    const char *buffer, size_t start, size_t last )
{
    if( !defs->enter_scope ) {
        defs->print(defs->obj, ",\n", 2);
    }
    defs->enter_scope = false;
    defs->enter_field = true;
    defs->print(defs->obj, "\"", 1);
    /* discard leading '$' character. */
    defs->print(defs->obj, &buffer[start + 1], last - start - 1);
}


/** Value of meta information like "date", "version", "timescale", etc.
 */
static void
print_field_value( struct definitions_t *defs,
    const char *buffer, size_t start, size_t last )
{
    if( defs->enter_field ) {
        defs->print(defs->obj, "\": \"", 4);
    } else {
        defs->print(defs->obj, " ", 1);
    }
    defs->enter_field = false;
    defs->print(defs->obj, &buffer[start], last - start);
}


static void
print_exit_field_value(  struct definitions_t *defs )
{
    defs->print(defs->obj, "\"", 1);
}


static void
print_enter_scope( struct definitions_t *defs,
    const char *buffer, size_t start, size_t last )
{
    if( !defs->enter_scope ) {
        defs->print(defs->obj, ",\n", 2);
    }
    defs->enter_scope = true;
    if( defs->scope_depth == 0 ) {
        defs->print(defs->obj, "\"definitions\": {\n", 17);
        ++defs->scope_depth;
    }
    for( int i = 0; i < defs->scope_depth; ++i )
        defs->print(defs->obj, "\t", 1);

    append_to_prefix(defs->scope_prefix, &buffer[start], last - start);
    ++defs->scope_depth;

    defs->print(defs->obj, "\"", 1);
    defs->print(defs->obj, &buffer[start], last - start);
    defs->print(defs->obj, "\": {\n", 5);
}


static void
print_exit_scope( struct definitions_t *defs )
{
    --defs->scope_depth;
    remove_last_prefix(defs->scope_prefix);

    defs->print(defs->obj, "\n", 1);
    for( int i = 0; i < defs->scope_depth; ++i )
        defs->print(defs->obj, "\t", 1);
    defs->print(defs->obj, "}", 1);
}


static void
print_enter_var( struct definitions_t *defs,
    const char *buffer, size_t start, size_t last )
{
    if( !defs->enter_scope ) {
        defs->print(defs->obj, ",\n", 2);
    }
    defs->enter_scope = false;
    for( int i = 0; i < defs->scope_depth; ++i )
        defs->print(defs->obj, "\t", 1);
    defs->print(defs->obj, "\"", 1);
    defs->print(defs->obj, &buffer[start], last - start);
    append_to_prefix(defs->scope_prefix, &buffer[start], last - start);
}


static void
print_exit_var( struct definitions_t *defs,
    const char *buffer, size_t start, size_t last )
{
    /* [low:high] vector suffix */
    append_to_prefix(defs->scope_prefix, &buffer[start], last - start);
    defs->print(defs->obj, &buffer[start], last - start);
    defs->print(defs->obj, "\": ", 3);
}


static void
print_value_change( struct simulation_t *sim,
    const char *buffer, size_t start, size_t last, size_t mark )
{
    /* mark indicates the end of the value and there is a space
       delimiter between the value and symbol name for bit vector changes. */
    assert( last >= mark );
    size_t len = last - mark;
    if( buffer[mark] == ' ' ) {
        assert( len >= 1 );
        --len;
    }
    signal_buf *timeline = find_timeline(
        sim->map, &buffer[last - len], len);
    if( !timeline ) return;

    /* At this point we have a filtered variable.
       -----------------------------------> time
       ^              ^              ^
       change_record  enter_period   current_time
    */
    if( sim->start_time <= sim->current_timestamp
        & sim->current_timestamp < sim->end_time  ) {
        if( timeline->not_first_record ) {
            // XXX Inefficient
            strncat(timeline->text, ",\n", BUFFER_SIZE);
        } else {
            if( sim->start_time < sim->current_timestamp ) {
                print_timestamp_and_value(timeline,
                    timeline->initial_change_record_timestamp,
                    timeline->initial_change_record_value_change,
                    timeline->initial_change_record_length);
                /* We are going to print two records back-to-back here. */
                // XXX Inefficient
                strncat(timeline->text, ",\n", BUFFER_SIZE);
            }
        }
        /* No need to buffer here. */
        print_timestamp_and_value(timeline,
            sim->current_timestamp, &buffer[start], mark - start);
        timeline->not_first_record = true;

    } else {
        set_value_change(timeline,
            sim->current_timestamp, buffer, start, mark);
    }
}


#define advance(state) { trans = &&state; goto advancePointer; }

/** push token into parser.
*/
static bool
push_token( struct parser_t *parser, vcd_token token, const char *buffer,
    size_t start, size_t last, size_t mark, bool broken, size_t line_num )
{
    void *trans = parser->state;

    /* Tokens which are broken over two input buffers must be
       reconstructed in a single linear block first. */
    if( broken ) {
        memcpy(&parser->broken_token[parser->broken_token_len],
            &buffer[start], last - start);
        parser->broken_token_len += last - start;
        parser->broken_token_mark = mark ? mark - start : 0;
        return false;

    } else if( parser->broken_token_len > 0 ) {
        memcpy(&parser->broken_token[parser->broken_token_len],
            &buffer[start], last - start);
        buffer = parser->broken_token;
        mark = mark ? mark - start : parser->broken_token_mark;
        last = parser->broken_token_len + (last - start);
        start = 0;
        parser->broken_token_len = 0;
    }

    if( trans != NULL ) goto *trans; else goto value_change_dump_definitions;

value_change_dump_definitions:
    switch( token ) {
    case comment_vcd_token:
    case date_vcd_token:
    case timescale_vcd_token:
    case version_vcd_token:
        if( parser->defs ) print_field_name(parser->defs, buffer, start, last);
        advance(keyword_field);
    case whitespace_vcd_token:
        advance(value_change_dump_definitions);
    case scope_vcd_token:
        advance(scope_scope_type);
    case upscope_vcd_token:
        if( parser->defs ) print_exit_scope(parser->defs);
        advance(end_keyword);
    case var_vcd_token:
        advance(var_var_type);
    case enddefinitions_vcd_token:
        if( parser->defs ) print_exit_scope(parser->defs);
        advance(end_keyword);
    case sim_time_vcd_token:
        if( parser->sim ) {
            parser->sim->current_timestamp = as_timestamp(buffer, start, last);
        }
        /* We encountered a simulation time at the top level,
           we are definitely done with the declaration commands. */
        advance(value_change_dump_definitions);

    case value_change_bit_vcd_token:
        if( parser->sim ) {
            print_value_change(parser->sim, buffer, start, last, mark);
        }
        advance(value_change_dump_definitions);
    case value_change_binary_vcd_token:
    case value_change_real_vcd_token:
        /* Remove the leading 'b', 'B', 'r' or 'R' */
        if( parser->sim ) {
            print_value_change(parser->sim, buffer, start + 1, last, mark);
        }
        advance(value_change_dump_definitions);
    case dumpvars_vcd_token:
    case dumpall_vcd_token:
    case dumpon_vcd_token:
        /* We are sure to find a reference for the variable in this section,
           let's fetch it as initial value for the time period. */
        advance(dumpall_variables);
    default:
        /* to shut-off gcc -Wswitch warning */
        break;
    }
    goto error;

keyword_field:
    if( token == whitespace_vcd_token ) advance(keyword_field);
    if( is_data_token(token) ) {
        if( parser->defs ) print_field_value(parser->defs, buffer, start, last);
        advance(keyword_field_value);
    }
    goto error;

keyword_field_value:
    if( token == whitespace_vcd_token ) advance(keyword_field_value);
    if( token == end_vcd_token ) {
        if( parser->defs ) print_exit_field_value(parser->defs);
        advance(value_change_dump_definitions);
    }
    if( is_data_token(token) ) {
        if( parser->defs ) print_field_value(parser->defs, buffer, start, last);
        advance(keyword_field_value);
    }
    goto error;

scope_scope_type:
    if( token == whitespace_vcd_token ) advance(scope_scope_type);
    if( token == data_vcd_token ) {
        advance(scope_scope_identifier);
    }
    goto error;

scope_scope_identifier:
    if( token == whitespace_vcd_token ) advance(scope_scope_identifier);
    if( token == data_vcd_token ) {
        if( parser->defs ) print_enter_scope(parser->defs, buffer, start, last);
        advance(end_keyword);
    }
    goto error;

var_var_type:
    if( token == whitespace_vcd_token ) advance(var_var_type);
    if( token == data_vcd_token ) {
        advance(var_var_size);
    }
    goto error;

var_var_size:
    if( token == whitespace_vcd_token ) advance(var_var_size);
    if( token == data_vcd_token ) {
        advance(var_var_identifier);
    }
    goto error;

var_var_identifier:
    if( token == whitespace_vcd_token ) advance(var_var_identifier);
    if( is_data_token(token) ) {
        set_identifier_code(parser, buffer, start, last);
        advance(var_var_reference);
    }
    goto error;

var_var_reference:
    if( token == whitespace_vcd_token ) advance(var_var_reference);
    if( token == data_vcd_token ) {
        if( parser->defs ) print_enter_var(parser->defs, buffer, start, last);
        advance(var_var_reference_slice);
    }
    goto error;

var_var_reference_slice:
    if( token == whitespace_vcd_token ) advance(var_var_reference_slice);
    if( token == end_vcd_token ) {
        if( parser->defs ) {
            print_exit_var(parser->defs, "", 0, 0);
            print_identifier_code(parser->defs, parser->identifier_code);
        }
        advance(value_change_dump_definitions);
    }
    if( token == data_vcd_token ) {
        if( parser->defs ) {
            print_exit_var(parser->defs, buffer, start, last);
            print_identifier_code(parser->defs, parser->identifier_code);
        }
        advance(end_keyword);
    }
    goto error;


end_keyword:
    if( token == whitespace_vcd_token ) advance(end_keyword);
    if( token == end_vcd_token ) advance(value_change_dump_definitions);
    goto error;

/* simulation commands */
dumpall_variables:
    if( token == whitespace_vcd_token ) advance(dumpall_variables);
    switch( token ) {
    case value_change_bit_vcd_token:
        if( parser->sim ) {
            print_value_change(parser->sim, buffer, start, last, mark);
        }
        advance(dumpall_variables);
    case value_change_binary_vcd_token:
    case value_change_real_vcd_token:
        /* Remove the leading 'b', 'B', 'r' or 'R' */
        if( parser->sim ) {
            print_value_change(parser->sim, buffer, start + 1, last, mark);
        }
        advance(dumpall_variables);
    case end_vcd_token:
        advance(value_change_dump_definitions);
    default:
        /* to shut-off gcc -Wswitch warning */
        break;
    }
    goto error;

advancePointer:
    parser->state = trans;
    return false;

 error:
    /* XXX */
    fprintf(stderr, "vcd:%ld: error: unexpected token %d\n", line_num, token);
#if 0
    printf("/* [%d:%ld,%ld:%d] ", token, start, last, broken);
    fwrite(&buffer[start], 1, last - start, stdout);
    printf("*/ ");
#endif
    assert( false );
}


size_t tokenize_header_and_definitions( struct tokenizer_t *tokenizer,
    const char *buffer, size_t buffer_length )
{
    size_t first = 0;
    size_t mark = 0, last = first;
    void *trans = tokenizer->state;
    const char *ptr = buffer;
    if( buffer_length == 0 ) return buffer_length;
    if( trans != NULL ) goto *trans; else goto token;

advancePointer:
    last = ptr - buffer;
    /* _ptr_ and _last_ point to the next character available for tokenization,
       which is one past the end of the last token and one less than the number
       of characters in the buffer (base at zero). */
    switch( last >= (buffer_length - 1) ? '\0' : *ptr ) {
    case '\0':
        if( last - first > 0 ) {
            push_token(&tokenizer->parser,
                tokenizer->tok, buffer, first, last, mark, true,
                tokenizer->line_num);
            first = last;
        }
        tokenizer->state = trans;
        return last + 1;
    case '\n':
        ++tokenizer->line_num;
    }
    ++ptr;
    goto *trans;

error:
    tokenizer->tok = err_vcd_token;
    while( *ptr != '$' ) advance(error);
    if( *ptr != 'e' )  advance(error);
    if( *ptr != 'n' )  advance(error);
    if( *ptr != 'd' )  advance(error);
    goto token;

token:
    last = ptr - buffer;
    if( last - first > 0 ) {
        if( push_token(&tokenizer->parser, tokenizer->tok,
                buffer, first, last, mark, false,
                tokenizer->line_num) ) {
            return last;
        }
        first = last;
    }
    if( isspace(*ptr) ) {
        tokenizer->tok = whitespace_vcd_token;
        advance(whitespace);
    }
    tokenizer->tok = data_vcd_token;
    switch( *ptr ) {
    case '$':
        advance(keyword);
    case '#':
        advance(simulation_time);
    case '0':
    case '1':
    case 'x':
    case 'X':
    case 'z':
    case 'Z':
        advance(scalar_value);
    case 'b':
    case 'B':
        advance(vector_binary_value);
    case 'r':
    case 'R':
        advance(vector_real_value);
    }
    advance(data);

keyword:
    switch( *ptr ) {
    case 'c':
        advance(c_omment);
    case 'd':
        advance(d_ate_or_d_ump);
    case 'e':
        advance(e_nd_or_e_nddefinitions);
    case 's':
        advance(s_cope);
    case 't':
        advance(t_imescale);
    case 'u':
        advance(u_pscope);
    case 'v':
        advance(v_ar_or_v_ersion);
    }
    if( !isspace(*ptr) ) {
        tokenizer->tok = keyword_vcd_token;
    }
    goto data;

/* comment keyword */
c_omment:
    if( *ptr == 'o' ) advance(co_mment);
    goto data;

co_mment:
    if( *ptr == 'm' ) advance(com_ment);
    goto data;

com_ment:
    if( *ptr == 'm' ) advance(comm_ent);
    goto data;

comm_ent:
    if( *ptr == 'e' ) advance(comme_nt);
    goto data;

comme_nt:
    if( *ptr == 'n' ) advance(commen_t);
    goto data;

commen_t:
    if( *ptr == 't' ) advance(comment);
    goto data;

comment:
    if( isspace(*ptr) ) {
        tokenizer->tok = comment_vcd_token;
        goto token;
    }
    goto data;

/* date or dump keywords */
d_ate_or_d_ump:
    if( *ptr == 'a' ) {
        advance(da_te);
    }
    if( *ptr == 'u' ) {
        advance(du_mp);
    }
    goto data;

/* date keyword */
da_te:
    if( *ptr == 't' ) advance(dat_e);
    goto data;

dat_e:
    if( *ptr == 'e' ) advance(date);
    goto data;

date:
    if( isspace(*ptr) ) {
        tokenizer->tok = date_vcd_token;
        goto token;
    }
    goto data;

/* dump* keyword */
du_mp:
    if( *ptr == 'm' ) advance(dum_p);
    goto data;

dum_p:
    if( *ptr == 'p' ) advance(dump_next);
    goto data;

dump_next:
    switch( *ptr ) {
    case 'a': advance(dumpa_ll);
    case 'o': advance(dumpo_n_ff);
    case 'v': advance(dumpv_ars);
    }
    goto data;

dumpa_ll:
    if( *ptr == 'l' ) advance(dumpal_l);
    goto data;

dumpal_l:
    if( *ptr == 'l' ) advance(dumpall);
    goto data;

dumpall:
    if( isspace(*ptr) ) {
        tokenizer->tok = dumpall_vcd_token;
        goto token;
    }
    goto data;

dumpo_n_ff:
    switch( *ptr ) {
    case 'n': advance(dumpon);
    case 'f': advance(dumpof_f);
    }
    goto data;

dumpon:
    if( isspace(*ptr) ) {
        tokenizer->tok = dumpon_vcd_token;
        goto token;
    }
    goto data;

dumpof_f:
    if( *ptr == 'f' ) advance(dumpoff);
    goto data;

dumpoff:
    if( isspace(*ptr) ) {
        tokenizer->tok = dumpoff_vcd_token;
        goto token;
    }
    goto data;

dumpv_ars:
    if( *ptr == 'a' ) advance(dumpva_rs);
    goto data;

dumpva_rs:
    if( *ptr == 'r' ) advance(dumpvar_s);
    goto data;

dumpvar_s:
    if( *ptr == 's' ) advance(dumpvars);
    goto data;

dumpvars:
    if( isspace(*ptr) ) {
        tokenizer->tok = dumpvars_vcd_token;
        goto token;
    }
    goto data;

/* end and enddefinitions keywords */
e_nd_or_e_nddefinitions:
    if( *ptr == 'n' ) advance(en_d_or_en_ddefinitions);
    goto data;

en_d_or_en_ddefinitions:
    if( *ptr == 'd' ) advance(end_or_end_definitions);
    goto data;

end_or_end_definitions:
    if( *ptr == 'd' ) advance(endd_efinitions);
    if( isspace(*ptr) ) {
        tokenizer->tok = end_vcd_token;
        goto token;
    }
    goto data;

endd_efinitions:
    if( *ptr == 'e' ) advance(endde_finitions);
    goto data;

endde_finitions:
    if( *ptr == 'f' ) advance(enddef_initions);
    goto data;

enddef_initions:
    if( *ptr == 'i' ) advance(enddefi_nitions);
    goto data;

enddefi_nitions:
    if( *ptr == 'n' ) advance(enddefin_itions);
    goto data;

enddefin_itions:
    if( *ptr == 'i' ) advance(enddefini_tions);
    goto data;

enddefini_tions:
    if( *ptr == 't' ) advance(enddefinit_ions);
    goto data;

enddefinit_ions:
    if( *ptr == 'i' ) advance(enddefiniti_ons);
    goto data;

enddefiniti_ons:
    if( *ptr == 'o' ) advance(enddefinitio_ns);
    goto data;

enddefinitio_ns:
    if( *ptr == 'n' ) advance(enddefinition_s);
    goto data;

enddefinition_s:
    if( *ptr == 's' ) advance(enddefinitions);
    goto data;

enddefinitions:
    if( isspace(*ptr) ) {
        tokenizer->tok = enddefinitions_vcd_token;
        goto token;
    }
    goto data;

/* scope keyword */
s_cope:
    if( *ptr == 'c' ) advance(sc_ope);
    goto data;

sc_ope:
    if( *ptr == 'o' ) advance(sco_pe);
    goto data;

sco_pe:
    if( *ptr == 'p' ) advance(scop_e);
    goto data;

scop_e:
    if( *ptr == 'e' ) advance(scope);
    goto data;

scope:
    if( isspace(*ptr) ) {
        tokenizer->tok = scope_vcd_token;
        goto token;
    }
    goto data;

/* upscope keyword, re-using scope keyword parsing */
u_pscope:
    if( *ptr == 'p' ) advance(up_scope);
    goto data;

up_scope:
    if( *ptr == 's' ) advance(ups_cope);
    goto data;

ups_cope:
    if( *ptr == 'c' ) advance(upsc_ope);
    goto data;

upsc_ope:
    if( *ptr == 'o' ) advance(upsco_pe);
    goto data;

upsco_pe:
    if( *ptr == 'p' ) advance(upscop_e);
    goto data;

upscop_e:
    if( *ptr == 'e' ) advance(upscope);
    goto data;

upscope:
    if( isspace(*ptr) ) {
        tokenizer->tok = upscope_vcd_token;
        goto token;
    }
    goto data;

/* timescale keyword */
t_imescale:
    if( *ptr == 'i' ) advance(ti_mescale);
    goto data;

ti_mescale:
    if( *ptr == 'm' ) advance(tim_escale);
    goto data;

tim_escale:
    if( *ptr == 'e' ) advance(time_scale);
    goto data;

time_scale:
    if( *ptr == 's' ) advance(times_cale);
    goto data;

times_cale:
    if( *ptr == 'c' ) advance(timesc_ale);
    goto data;

timesc_ale:
    if( *ptr == 'a' ) advance(timesca_le);
    goto data;

timesca_le:
    if( *ptr == 'l' ) advance(timescal_e);
    goto data;

timescal_e:
    if( *ptr == 'e' ) advance(timescale);
    goto data;

timescale:
    if( isspace(*ptr) ) {
        tokenizer->tok = timescale_vcd_token;
        goto token;
    }
    goto data;

/* var or version keywords */
v_ar_or_v_ersion:
    switch( *ptr ) {
    case 'a': advance(va_r);
    case 'e': advance(ve_rsion);
    }
    goto data;

va_r:
    if( *ptr == 'r' ) advance(var);
    goto data;

var:
    if( isspace(*ptr) ) {
        tokenizer->tok = var_vcd_token;
        goto token;
    }
    goto data;

ve_rsion:
    if( *ptr == 'r' ) advance(ver_sion);
    goto data;

ver_sion:
    if( *ptr == 's' ) advance(vers_ion);
    goto data;

vers_ion:
    if( *ptr == 'i' ) advance(versi_on);
    goto data;

versi_on:
    if( *ptr == 'o' ) advance(versio_n);
    goto data;

versio_n:
    if( *ptr == 'n' ) advance(version);
    goto data;

version:
    if( isspace(*ptr) ) {
        tokenizer->tok = version_vcd_token;
        goto token;
    }
    goto data;

whitespace:
    while( isspace(*ptr) ) advance(whitespace);
    goto token;

data:
    while( !isspace(*ptr) ) advance(data);
    goto token;

simulation_time:
    if( *ptr >= '0' & *ptr <= '9' ) advance(simulation_time_next);
    goto data;

simulation_time_next:
    if( *ptr >= '0' & *ptr <= '9' ) advance(simulation_time_next);
    if( isspace(*ptr) ) {
        tokenizer->tok = sim_time_vcd_token;
        goto token;
    }
    goto data;

scalar_value:
    /* We are bundling the name of the variable inside the token here. */
    if( !isspace(*ptr) ) { // XXX 33 to 126?
        mark = ptr - buffer;
        tokenizer->tok = value_change_bit_vcd_token;
        advance(scalar_value_identifier);
    }
    goto data;

scalar_value_identifier:
    while( !isspace(*ptr) ) advance(scalar_value_identifier);
    goto token;

vector_binary_value:
    switch( *ptr ) {
    case '0':
    case '1':
    case 'x':
    case 'X':
    case 'z':
    case 'Z':
        advance(vector_binary_value_next);
    }
    goto data;

vector_binary_value_next:
    /* We are bundling the name of the variable inside the token here. */
    if( isspace(*ptr) ) {
        mark = ptr - buffer;
        tokenizer->tok = value_change_binary_vcd_token;
        advance(vector_binary_value_identifier);
    }
    switch( *ptr ) {
    case '0':
    case '1':
    case 'x':
    case 'X':
    case 'z':
    case 'Z':
        advance(vector_binary_value_next);
    }
    goto data;

vector_binary_value_identifier:
    if( isspace(*ptr) ) advance(vector_binary_value_identifier);
    goto data;

vector_real_value:
    /* XXX no check of compliance with a float-formatted value here. */
    if( *ptr >= '0' & *ptr <= '9' ) advance(vector_real_value_next);
    goto data;

vector_real_value_next:
    /* We are bundling the name of the variable inside the token here. */
    if( isspace(*ptr) ) {
        mark = ptr - buffer;
        tokenizer->tok = value_change_real_vcd_token;
        advance(vector_real_value_identifier);
    }
    advance(vector_real_value_next);

vector_real_value_identifier:
    if( isspace(*ptr) ) advance(vector_real_value_identifier);
    goto data;

}


void header_and_definitions( FILE *from, signal_map *map,
    vcd_print_callback print, void *obj )
{
    struct definitions_t defs;
    struct tokenizer_t tokenizer;
    size_t bytes_read = 1;
    char buffer[BUFFER_SIZE];

    init_definitions(&defs, map, print, obj);
    init_tokenizer(&tokenizer, &defs, NULL);

    defs.print(defs.obj, "{\n", 2);
    while( bytes_read > 0 ) {
        bytes_read = fread(buffer, 1, BUFFER_SIZE, from);
        if( tokenize_header_and_definitions(&tokenizer, buffer, bytes_read)
            != bytes_read ){
            break;
        }
    }
    defs.print(defs.obj, "\n}\n", 3);
}


void value_changes( FILE *from, const signal_map *map,
    size_t start_time, size_t end_time, size_t resolution )
{
    struct simulation_t sim;
    struct tokenizer_t tokenizer;
    size_t bytes_read = 1;
    char buffer[BUFFER_SIZE];

    init_simulation(&sim, map, start_time, end_time, resolution);
    init_tokenizer(&tokenizer, NULL, &sim);

    /* XXX seek to first reference as described in toc. */

    while( bytes_read > 0 ) {
        bytes_read = fread(buffer, 1, BUFFER_SIZE, from);
        if( tokenize_header_and_definitions(&tokenizer, buffer, bytes_read)
            != bytes_read ){
            break;
        }
    }
}


void filter_value_changes( FILE *from, signal_map *map,
    size_t start_time, size_t end_time, size_t resolution,
    vcd_print_callback print, void *obj )
{
    signal_buf *curr = NULL;
    struct definitions_t defs;
    struct simulation_t sim;
    struct tokenizer_t tokenizer;
    size_t bytes_read = 1;
    char buffer[BUFFER_SIZE];

    init_definitions(&defs, map, print, obj);
    init_simulation(&sim, map, start_time, end_time, resolution);
    init_tokenizer(&tokenizer, &defs, &sim);

    print(obj, "{\n", 2);

    while( bytes_read > 0 ) {
        bytes_read = fread(buffer, 1, BUFFER_SIZE, from);
        if( tokenize_header_and_definitions(&tokenizer, buffer, bytes_read)
            != bytes_read ){
            break;
        }
    }

    curr = map->head;
    while( curr ) {
        /* Always append comma. First one is to close header information. */
        print(obj, ",\n\"", 3);
        print(obj, curr->name, strlen(curr->name));
        print(obj, "\": [\n", 5);
        print(obj, curr->text, strlen(curr->text));
        print(obj, "\n]", 2);
        curr = curr->next;
    }
    print(obj, "}\n", 2);
}
