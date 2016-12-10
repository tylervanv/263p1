#include "memory_block.h"
#include "fully_associative.h"

fully_associative_cache* fac_init(main_memory* mm)
{
    fully_associative_cache* result = malloc(sizeof(fully_associative_cache));
    result->mm = mm;
    result->blocks = malloc(FULLY_ASSOCIATIVE_NUM_WAYS*sizeof(memory_block*));
    result->tags = malloc(FULLY_ASSOCIATIVE_NUM_WAYS*sizeof(int));
    result->vals = malloc(FULLY_ASSOCIATIVE_NUM_WAYS*sizeof(int));
    result->dirts = malloc(FULLY_ASSOCIATIVE_NUM_WAYS*sizeof(int));
    result->ages = malloc(FULLY_ASSOCIATIVE_NUM_WAYS*sizeof(int));
    result->cs = cs_init();
    
    for (int i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        result->blocks[i] = NULL;
        result->tags[i] = -1;
        result->vals[i] = 0;
        result->dirts[i] = 0;
        result->ages[i] = 0;
    }
        
    return result;
}

static int addr_to_tag(void* addr)
{
    int tag;
    tag = (int) addr >> MAIN_MEMORY_BLOCK_SIZE_LN;
    
    return tag;
}

static void mark_as_used(fully_associative_cache* fac, int index)
{
    fac->ages[index] = 0;
    for (int i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        if (i != index)
            fac->ages[i]++;
    }
}

static int lru(fully_associative_cache* fac)
{
    int max = 0;
    int oldest = 0;
    for (int i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        if (fac->ages[i] > max)
        {
            max = fac->ages[i];
            oldest = i;
        }
    }
    
    return oldest;
    
}


void fac_store_word(fully_associative_cache* fac, void* addr, unsigned int val)
{
    int tag;
    int oldest;
    int got;
    int vacant;
    
    addr -= MAIN_MEMORY_START_ADDR;
    tag = addr_to_tag(addr);
    oldest = lru(fac);
    got = 0;
    vacant = -1;
        
    // precompute start address of memory block
    size_t addr_offt = (size_t) addr % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;
    
    memory_block* mbhave;
    memory_block* mbneed;
    unsigned int* wd_addr;    
    
    // check if block is in cache
    for (int i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        // if cached is correct block select it for write
        if (fac->vals[i] == 1)
        {
            if (fac->tags[i] == tag)
            {
                mbneed = fac->blocks[i];
                fac->dirts[i] = 1;
                mark_as_used(fac, i);
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
            mbhave = fac->blocks[oldest];
        
            // writeback if block is dirty
            if (fac->dirts[oldest] == 1) 
                mm_write(fac->mm, mbhave->start_addr, mbhave);

            mbneed = mm_read(fac->mm, mb_start_addr);
            fac->blocks[oldest] = mbneed;
            fac->tags[oldest] = tag;
            fac->dirts[oldest] = 1;
            mark_as_used(fac, oldest);
            
            mb_free(mbhave);
        }
        // otherwise cache new block at vacancy
        else
        {
            mbneed = mm_read(fac->mm, mb_start_addr);
            fac->blocks[vacant] = mbneed;
            fac->vals[vacant] = 1;
            fac->tags[vacant] = tag;
            fac->dirts[vacant] = 1;
            mark_as_used(fac, vacant);
        }
        
         ++fac->cs.w_misses;
    }
    
    // write value to cached block
    wd_addr = mbneed->data + addr_offt;
    *wd_addr = val;
    
    ++fac->cs.w_queries;
        
}


unsigned int fac_load_word(fully_associative_cache* fac, void* addr)
{
    int tag;
    int oldest;
    int got;
    int vacant;
    
    addr -= MAIN_MEMORY_START_ADDR;
    tag = addr_to_tag(addr);
    oldest = lru(fac);
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
    for (int i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        // if cached is correct block select it for read
        if (fac->vals[i] == 1)
        {
            if (fac->tags[i] == tag)
            {
                mbneed = fac->blocks[i];
                mark_as_used(fac, i);
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
            mbhave = fac->blocks[oldest];
        
            // writeback if block is dirty
            if (fac->dirts[oldest] == 1) 
                mm_write(fac->mm, mbhave->start_addr, mbhave);

            mbneed = mm_read(fac->mm, mb_start_addr);
            fac->blocks[oldest] = mbneed;
            fac->tags[oldest] = tag;
            fac->dirts[oldest] = 0;
            mark_as_used(fac, oldest);
            
            mb_free(mbhave);
        }
        // otherwise cache new block at vacancy
        else
        {
            mbneed = mm_read(fac->mm, mb_start_addr);
            fac->blocks[vacant] = mbneed;
            fac->vals[vacant] = 1;
            fac->tags[vacant] = tag;
            mark_as_used(fac, vacant);
        }
        
         ++fac->cs.r_misses;
    }
    
    // get result from cached block
    wd_addr = mbneed->data + addr_offt;
    result = *wd_addr;
    
    ++fac->cs.r_queries;
        
    // return result
    return result;
}

void fac_free(fully_associative_cache* fac)
{
    for (int i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
        free(fac->blocks[i]);
    free(fac->blocks);
    free(fac->tags);
    free(fac->vals);
    free(fac->dirts);
    free(fac->ages);
    free(fac);
}