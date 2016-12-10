#include <stdint.h>

#include "memory_block.h"
#include "set_associative.h"

set_associative_cache* sac_init(main_memory* mm)
{
    int numblocks = SET_ASSOCIATIVE_NUM_SETS*SET_ASSOCIATIVE_NUM_WAYS;
    
    set_associative_cache* result = malloc(sizeof(set_associative_cache));
    result->mm = mm;
    result->blocks = malloc(numblocks*sizeof(memory_block*));
    result->tags = malloc(numblocks*sizeof(int));
    result->vals = malloc(numblocks*sizeof(int));
    result->dirts = malloc(numblocks*sizeof(int));
    result->ages = malloc(numblocks*sizeof(int));
    result->cs = cs_init();
    
    for (int i = 0; i < numblocks; i++)
    {
        result->blocks[i] = NULL;
        result->tags[i] = -1;
        result->vals[i] = 0;
        result->dirts[i] = 0;
        result->ages[i] = 0;
    }
        
    return result;
    return 0;
}

static int addr_to_set(void* addr)
{
    int set;
    int block;
    block = (int) addr >> MAIN_MEMORY_BLOCK_SIZE_LN;
    set = (int) block % SET_ASSOCIATIVE_NUM_SETS;
    
    return set;
    
}

static int addr_to_tag(void* addr)
{
    int block;
    int tag;
    block = (int) addr >> MAIN_MEMORY_BLOCK_SIZE_LN;
    tag = (int) block >> SET_ASSOCIATIVE_NUM_SETS_LN;
    
    return tag;
}

static void mark_as_used(set_associative_cache* sac, int set, int way)
{
    int start = set * SET_ASSOCIATIVE_NUM_WAYS;
    int finish = start + SET_ASSOCIATIVE_NUM_WAYS;
    int block = start + way;
    
    sac->ages[block] = 0;
    for (int i = start; i < finish; i++)
    {
        if (i != block)
            sac->ages[i]++;
    }
}

static int lru(set_associative_cache* sac, int set)
{
    int max = 0;
    int oldest = 0;
    int start = set * SET_ASSOCIATIVE_NUM_WAYS;
    int finish = start + SET_ASSOCIATIVE_NUM_WAYS;
    
    for (int i = start; i < finish; i++)
    {
        if (sac->ages[i] > max)
        {
            max = sac->ages[i];
            oldest = i;
        }
    }
    
    return oldest;
}


void sac_store_word(set_associative_cache* sac, void* addr, unsigned int val)
{
    int tag;
    int set;
    int start;
    int finish;
    int oldest;
    int got;
    int vacant;
    
    addr -= MAIN_MEMORY_START_ADDR;
    tag = addr_to_tag(addr);
    set = addr_to_set(addr);
    start = set * SET_ASSOCIATIVE_NUM_WAYS;
    finish = start + SET_ASSOCIATIVE_NUM_WAYS;
    oldest = lru(sac, set);
    got = 0;
    vacant = -1;
        
    // precompute start address of memory block
    size_t addr_offt = (size_t) addr % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;
    
    memory_block* mbhave;
    memory_block* mbneed;
    unsigned int* wd_addr;    
    
    // check if block is in cache
    for (int i = start; i < finish; i++)
    {
        // if cached is correct block select it for write
        if (sac->vals[i] == 1)
        {
            if (sac->tags[i] == tag)
            {
                mbneed = sac->blocks[i];
                sac->dirts[i] = 1;
                mark_as_used(sac, set, i);
                got = 1;
                break;
            }
        }
        // save vacant cache block for potential use
        else
            vacant = i;
    }
    
    // otherwise cache new block
    if (got == 0)
    {
        // if no vacancy evict old and cache new block
        if (vacant == -1)
        { 
            mbhave = sac->blocks[oldest];
        
            // writeback if block is dirty
            if (sac->dirts[oldest] == 1) 
                mm_write(sac->mm, mbhave->start_addr, mbhave);

            mbneed = mm_read(sac->mm, mb_start_addr);
            sac->blocks[oldest] = mbneed;
            sac->tags[oldest] = tag;
            sac->dirts[oldest] = 1;
            mark_as_used(sac, set, oldest);
            
            mb_free(mbhave);
        }
        // otherwise cache new block at vacancy
        else
        {
            mbneed = mm_read(sac->mm, mb_start_addr);
            sac->blocks[vacant] = mbneed;
            sac->vals[vacant] = 1;
            sac->tags[vacant] = tag;
            sac->dirts[vacant] = 1;
            mark_as_used(sac, set, vacant);
        }
        
         ++sac->cs.w_misses;
    }
    
    // write value to cached block
    wd_addr = mbneed->data + addr_offt;
    *wd_addr = val;
    
    ++sac->cs.w_queries;
}


unsigned int sac_load_word(set_associative_cache* sac, void* addr)
{
    int set;
    int tag;
    int start;
    int finish;
    int oldest;
    int got;
    int vacant;
    
    addr -= MAIN_MEMORY_START_ADDR;
    set = addr_to_set(addr);
    tag = addr_to_tag(addr);
    start = set * SET_ASSOCIATIVE_NUM_WAYS;
    finish = start + SET_ASSOCIATIVE_NUM_WAYS;
    oldest = lru(sac, set);
    got = 0;
    vacant = -1;
        
    // precompute start address of memory block
    size_t addr_offt = (size_t) addr % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;
    
    memory_block* mbhave;
    memory_block* mbneed;
    unsigned int* wd_addr;
    unsigned int result;
    
    
    // check if block is in cache
    for (int i = start; i < finish; i++)
    {
        // if cached is correct block select it for read
        if (sac->vals[i] == 1)
        {
            if (sac->tags[i] == tag)
            {
                mbneed = sac->blocks[i];
                mark_as_used(sac, set, i);
                got = 1;
                break;
            }
        }
        // save vacant cache block for potential use
        else
            vacant = i;
    }
    
    // otherwise cache new block
    if (got == 0)
    {
        // if no vacancy evict and cache new block
        if (vacant == -1)
        { 
            mbhave = sac->blocks[oldest];
        
            // writeback if block is dirty
            if (sac->dirts[oldest] == 1) 
                mm_write(sac->mm, mbhave->start_addr, mbhave);

            mbneed = mm_read(sac->mm, mb_start_addr);
            sac->blocks[oldest] = mbneed;
            sac->tags[oldest] = tag;
            sac->dirts[oldest] = 0;
            mark_as_used(sac, set, oldest);
            
            mb_free(mbhave);
        }
        // otherwise cache new block at vacancy
        else
        {
            mbneed = mm_read(sac->mm, mb_start_addr);
            sac->blocks[vacant] = mbneed;
            sac->vals[vacant] = 1;
            sac->tags[vacant] = tag;
            mark_as_used(sac, set, vacant);
        }
        
         ++sac->cs.r_misses;
    }
    
    // get result from cached block
    wd_addr = mbneed->data + addr_offt;
    result = *wd_addr;
    
    ++sac->cs.r_queries;
        
    // return result
    return result;
}

void sac_free(set_associative_cache* sac)
{
    int numblocks = SET_ASSOCIATIVE_NUM_SETS*SET_ASSOCIATIVE_NUM_WAYS;
    
    for (int i = 0; i < numblocks; i++)
        free(sac->blocks[i]);
    free(sac->blocks);
    free(sac->tags);
    free(sac->vals);
    free(sac->dirts);
    free(sac->ages);
    free(sac);
}