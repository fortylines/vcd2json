/* Copyright (c) 2015, Sebastien Mirolo
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
#include <string.h>
#include <stdlib.h>
#include "libvcd.h"

signal_buf *insert_signal( signal_buf *head, char *name ) {
    signal_buf *prev = NULL;
    signal_buf *curr = head;
    while( curr && strcmp(curr->name, name) >= 0 ) {
        prev = curr;
        curr = curr->next;
    }
    signal_buf *node = malloc(sizeof(signal_buf));
    memset(node, 0, sizeof(signal_buf));
    node->name = malloc(strlen(name) + 1);
    strcpy(node->name, name);
    node->next = curr;
    if( prev ) {
        prev->next = node;
    }
    return prev ? head : node;
}


void init_signal_map( signal_map *map ) {
    assert(map != NULL);
    memset(map, 0, sizeof(signal_map));
}


void destroy_signal_map( signal_map *map ) {
    signal_buf *curr = map->head;
    while( curr ) {
        signal_buf *prev = curr;
        curr = curr->next;
        free((void *)prev->name);
        free(prev);
    }
    memset(map, 0, sizeof(signal_map));
}


int insert_short_key( signal_map *map, const char *name, const char *key ) {
    int key_len = strlen(key);
    if( key_len > sizeof(uint32_t) ) {
        fprintf(stderr, "error: The key is %d characters long,"\
            " which is more than the %ld bytes.", key_len, sizeof(uint32_t));
        return 1;
    }
    /* Hash of the short key */
    uint8_t key_hash = *key++;
    uint32_t key_sym = key_hash;
    while( *key != '\0' ) {
        key_sym = key_sym << 8 | *key;
        key_hash = key_hash ^ *key;
        ++key;
    }
    /* Insert the short key into the hash table */
    int j = 0;
    while( j < 256 ) {
        if( map->entries[key_hash].key == 0
            || map->entries[key_hash].key == key_sym ) {
            /* Associate the timeline buffer */
            signal_buf *curr = map->head, *prev = NULL;
            while( curr && strcmp(curr->name, name) != 0 ) {
                prev = curr;
                curr = curr->next;
            }
            if( curr ) {
                map->entries[key_hash].timeline = curr;
                map->entries[key_hash].key = key_sym;
            }
            break;
        }
        key_hash = (key_hash + 1) % 256;
        ++j;
    }
    if( j == 256 ) {
        fprintf(stderr, "error: The hash table is full");
        return 1;
    }
    return 0;
}


signal_buf*
find_timeline( const signal_map *map, const char *key, size_t key_len ) {
#if 0
    assert(key_len < FILENAME_MAX);
    char key_tmp[FILENAME_MAX];
    strncpy(key_tmp, key, key_len);
    fprintf(stderr, "find %s in\n", key_tmp);
    for( int i = 0; i < 256; ++i ) {
        if( map->entries[i].key ) {
            fprintf(stderr, "%d: %d\n", i, map->entries[i].key);
        }
    }
#endif

    /* Hash of the short key */
    uint8_t key_hash = *key++;
    uint32_t key_sym = key_hash;
    --key_len;
    while( key_len ) {
        key_sym = key_sym << 8 | *key;
        key_hash = key_hash ^ *key;
        ++key;
        --key_len;
    }

    /* Find the short key into the hash table */
    int j = 0;
    while( j < 256 ) {
        if( map->entries[key_hash].key == 0 ) {
            /* key does not exists. */
            return NULL;
        } else if( map->entries[key_hash].key == key_sym ) {
            /* We found the key */
            assert( map->entries[key_hash].timeline );
            return map->entries[key_hash].timeline;
        }
        key_hash = (key_hash + 1) % 256;
        ++j;
    }
    fprintf(stderr, "warning: walked the whole table for %s.", key);
    return NULL;
}

