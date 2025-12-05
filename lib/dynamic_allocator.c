/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	if (ptrPageInfo < &pageBlockInfoArr[0] || ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE/PAGE_SIZE])
			panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va)
{
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0 || idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================

bool is_initialized = 0;
void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//Your code is here
		//DAlimits
	dynAllocStart = daStart;
	dynAllocEnd   = daEnd;
	//Array of Page Info
	uint32 num_of_pages=(daEnd-daStart)/PAGE_SIZE;
	for(uint32 i=0;i<num_of_pages;i++)
	{
		pageBlockInfoArr[i].block_size=0;
		pageBlockInfoArr[i].num_of_free_blocks=0;
	}
	//initializeFreePageList
	LIST_INIT(&freePagesList);
	for (uint32 i = 0; i < num_of_pages; i++)
		{
			LIST_INSERT_TAIL(&freePagesList, &pageBlockInfoArr[i]);
		}
	// initializeFreeblockList

	for (int i = 0; i < (LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1); i++)
	{
	    LIST_INIT(&freeBlockLists[i]);
	}

	//Comment the following line

	//panic("initialize_dynamic_allocator() Not implemented yet");

}

//===========================
// [2] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here
	//Comment the following line
	struct PageInfoElement *pageInfo = to_page_info((uint32)va);
	    return pageInfo->block_size;
}
	//panic("get_block_size() Not implemented yet");

//===========================
// 3) ALLOCATE BLOCK:
//===========================
// helper_fun bt3ml return ll index bt5d size 5ly balk lazm ta5d el index b3d nearest pow-of-2

uint32 get_list_index(uint32 Bsize)
{
	uint32 index=0;
	uint32 size=8;
	while(size<Bsize&&size<DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		size<<=1;
		index++;
	}
	return index;
}

// bt3ml return nearest_pow_of_2
uint32 nearest_pow_of_2(uint32 nsize)
{
    uint32 power = 8;
    while (power < nsize)power <<= 1;
    return power;
}

void *alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
	//Your code is here
	if(size==0)return NULL;

	uint32 ssize=nearest_pow_of_2(size);
	uint32 index=get_list_index(ssize);
	//1- lw_fi_free block exists
	if (!LIST_EMPTY(&freeBlockLists[index])) {

	    struct BlockElement *block = LIST_FIRST(&freeBlockLists[index]);

	    LIST_REMOVE(&freeBlockLists[index], block);

	    struct PageInfoElement *page = to_page_info((uint32)block);

	    page->num_of_free_blocks--;

	    return (void*)block;
	}
	// lw_fi_freepagelist
	else if(!LIST_EMPTY(&freePagesList))
	{
		struct PageInfoElement *page = LIST_FIRST(&freePagesList);

		LIST_REMOVE(&freePagesList, page);
		uint32 va_page = to_page_va(page);
		get_page((void*)va_page);
		memset((void*)va_page, 0, PAGE_SIZE);
		uint32 num_ofblocks=PAGE_SIZE/ssize;
		for (uint32 i = 0; i < num_ofblocks; i++) {
			struct BlockElement *block = (struct BlockElement *)(va_page + i * ssize);
			LIST_INSERT_TAIL(&freeBlockLists[index], block);
		}
		struct BlockElement *block = LIST_FIRST(&freeBlockLists[index]);
		LIST_REMOVE(&freeBlockLists[index], block);
		page->block_size=ssize;
		page->num_of_free_blocks=num_ofblocks-1;

		return (void*)block;

	}
	 else if (index + 1 <= (LOG2_MAX_SIZE - LOG2_MIN_SIZE)) {

	        return alloc_block(ssize * 2);
	    }

	else
	{
		panic("Dynamic Allocator: Out of memory!");
	}




	//Comment the following line
	//panic("alloc_block() Not implemented yet");

	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
}
//===========================
// [4] FREE BLOCK:
//===========================

void free_block(void *va)
{
    //==================================================================================
    //DON'T CHANGE THESE LINES==========================================================
    //==================================================================================
    assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
    //==================================================================================

     //==================================================================================
     //==================================================================================

     //TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
     //Your code is here
        struct PageInfoElement *pageInfo = to_page_info((uint32)va);
        uint32 blockSize = get_block_size(va);
        uint32 blocksPerPage = 0;

        if (blockSize != 0) {
            blocksPerPage = PAGE_SIZE / blockSize;
            int listIndex = get_list_index(blockSize);

            struct BlockElement *block = (struct BlockElement *)va;

            LIST_INSERT_HEAD(&freeBlockLists[listIndex], block);
            pageInfo->num_of_free_blocks++;

            uint32 pageStart = to_page_va(pageInfo);
            uint32 pageEnd = pageStart + PAGE_SIZE;

            if (pageInfo->num_of_free_blocks == blocksPerPage) {
                struct BlockElement *blk;

                LIST_FOREACH(blk, &freeBlockLists[listIndex]) {
                	uint32 b = (uint32)blk;
                    if (b >= pageStart && b < pageEnd) {
                        LIST_REMOVE(&freeBlockLists[listIndex], blk);
                    }
                }
                pageInfo->num_of_free_blocks = 0;
                pageInfo->block_size = 0;
                return_page((void *)pageStart);
                LIST_INSERT_TAIL(&freePagesList, pageInfo);
            }
        }




     //Comment the following line
     //panic("free_block() Not implemented yet");

}
//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
