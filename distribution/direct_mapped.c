#include <stdint.h>
#include <stdio.h>

#include "memory_block.h"
#include "direct_mapped.h"

direct_mapped_cache* dmc_init(main_memory* mm)
{
    direct_mapped_cache* result = malloc(sizeof(direct_mapped_cache));
    result->mm = mm;
    result->blocks = malloc(DIRECT_MAPPED_NUM_SETS*sizeof(memory_block*));
    result->tags = malloc(DIRECT_MAPPED_NUM_SETS*sizeof(int));
    result->vals = malloc(DIRECT_MAPPED_NUM_SETS*sizeof(int));
    result->dirts = malloc(DIRECT_MAPPED_NUM_SETS*sizeof(int));
    result->cs = cs_init();
    
    for (int i = 0; i < DIRECT_MAPPED_NUM_SETS; i++)
    {
        result->blocks[i] = NULL;
        result->tags[i] = -1;
        result->vals[i] = 0;
        result->dirts[i] = 0;
    }
        
    return result;
};

static int addr_to_index(void* addr)
{
    int index;
    int block;
    block = (int) addr >> MAIN_MEMORY_BLOCK_SIZE_LN;
    index = (int) block % DIRECT_MAPPED_NUM_SETS;
    
    return index;
    
}

static int addr_to_tag(void* addr)
{
    int block;
    int tag;
    block = (int) addr >> MAIN_MEMORY_BLOCK_SIZE_LN;
    tag = (int) block >> DIRECT_MAPPED_NUM_SETS_LN;
    
    return tag;
}

void dmc_store_word(direct_mapped_cache* dmc, void* addr, unsigned int val)
{
    int index;
    int tag;
    
    addr -= MAIN_MEMORY_START_ADDR;
    index = addr_to_index(addr);
    tag = addr_to_tag(addr);
        
    // precompute start address of memory block
    size_t addr_offt = (size_t) addr % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;
    
    memory_block* mbhave;
    memory_block* mbneed;
    unsigned int* wd_addr;
    
    // check if cache block is valid
    if (dmc->vals[index] == 1)
    {
        mbhave = dmc->blocks[index];
        // if cached is correct select it for write
        if (dmc->tags[index] == tag)
            mbneed = mbhave;
        // otherwise evict cached block and cache new block
        else
        {
            // writeback if block is dirty
            if (dmc->dirts[index] == 1)
                mm_write(dmc->mm, mbhave->start_addr, mbhave);
            
            mbneed = mm_read(dmc->mm, mb_start_addr);
            dmc->blocks[index] = mbneed;
            dmc->tags[index] = tag;
            mb_free(mbhave);
            
            ++dmc->cs.w_misses;
        }
    }
    // if invalid cache new block
    else
    {
        mbneed = mm_read(dmc->mm, mb_start_addr);
        dmc->blocks[index] = mbneed;
        dmc->vals[index] = 1;
        dmc->tags[index] = tag;
        
        ++dmc->cs.w_misses;
    }
    
    // write value to cached block
    wd_addr = mbneed->data + addr_offt;
    *wd_addr = val;
    
    dmc->dirts[index] = 1;
    ++dmc->cs.w_queries;
}

unsigned int dmc_load_word(direct_mapped_cache* dmc, void* addr)
{   
    int index;
    int tag;
    
    addr -= MAIN_MEMORY_START_ADDR;
    index = addr_to_index(addr);
    tag = addr_to_tag(addr);
        
    // precompute start address of memory block
    size_t addr_offt = (size_t) addr % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;
    
    memory_block* mbhave;
    memory_block* mbneed;
    unsigned int* wd_addr;
    unsigned int result;
    
    // check if cache block is valid
    if (dmc->vals[index] == 1)
    {
        mbhave = dmc->blocks[index];
        // if cached is correct block select it for read
        if (dmc->tags[index] == tag)
            mbneed = mbhave;
        // otherwise unload cached block and cache new block
        else
        {
            // writeback if block is dirty
            if (dmc->dirts[index] == 1) 
                mm_write(dmc->mm, mbhave->start_addr, mbhave);
            
            mbneed = mm_read(dmc->mm, mb_start_addr);
            dmc->blocks[index] = mbneed;
            dmc->tags[index] = tag;
            dmc->dirts[index] = 0;
            
            mb_free(mbhave);
            
            ++dmc->cs.r_misses;
        }
    }
    // if invalid cache new block
    else
    {
        mbneed = mm_read(dmc->mm, mb_start_addr);
        dmc->blocks[index] = mbneed;
        dmc->vals[index] = 1;
        dmc->tags[index] = tag;
        
        ++dmc->cs.r_misses;
    }
    
    // get result from cached block
    wd_addr = mbneed->data + addr_offt;
    result = *wd_addr;
    
    ++dmc->cs.r_queries;
        
    // return result
    return result;
}

void dmc_free(direct_mapped_cache* dmc)
{
    for (int i = 0; i < DIRECT_MAPPED_NUM_SETS; i++)
        free(dmc->blocks[i]);
    free(dmc->blocks);
    free(dmc->tags);
    free(dmc->vals);
    free(dmc->dirts);
    free(dmc);
}