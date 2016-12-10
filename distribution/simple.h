#ifndef SIMPLE_H
#define SIMPLE_H

#include "main_memory.h"
#include "cache_stats.h"

#define DIRECT_MAPPED_NUM_SETS 16
#define DIRECT_MAPPED_NUM_SETS_LN 4

typedef struct simple_cache
{
    main_memory* mm;
    cache_stats cs;
} simple_cache;

simple_cache* sc_init(main_memory* mm);

void addr_to_set(void* addr);

void sc_store_word(simple_cache* sc, void* addr, unsigned int val);

unsigned int sc_load_word(simple_cache* sc, void* addr);

void sc_free(simple_cache* sc);

#endif