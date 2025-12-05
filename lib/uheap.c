#include <inc/lib.h>

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//////////////////////////////// my globals ///////////////////////////////////////////////////
#define MY_UHEAP_PAGE_ALLOC_START (USER_HEAP_START + (32 << 20) + PAGE_SIZE)
#define TOTAL_NUMBER_OF_PAGES ((USER_HEAP_MAX - MY_UHEAP_PAGE_ALLOC_START) / PAGE_SIZE)
struct Block_Entry_Info {
    uint32 start;
    uint32 size;
};

struct Block_Entry_Info free_blks[TOTAL_NUMBER_OF_PAGES];
struct Block_Entry_Info used_blks[TOTAL_NUMBER_OF_PAGES];

uint32 availble_pages = TOTAL_NUMBER_OF_PAGES;
int free_blks_size = 0;
int used_size = 0;
//////////////////////////////////////////////////////////////////////////////////////

//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
//		uheapPlaceStrategy = UHP_PLACE_CUSTOMFIT;
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;

		__firstTimeFlag = 0;
	}
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

// DEBUGGING
/*void dbg_start_address(){

	for(int i = 0;i < used_size;++i)
	cprintf("used blk start is\n%x\nsize = %u\n",(used_blks[i].start),(used_blks[i].size/PAGE_SIZE));

cprintf("\n///////////////////////////////////////////////////////////////////////////////////////\n");
}
void dbg_unused_address(){

	for(int i = 0;i < free_blks_size;++i)
	cprintf("free blk start is\n%x\nsize = %u\n",free_blks[i].start ,(free_blks[i].size/PAGE_SIZE));

	cprintf("\n///////////////////////////////////////////////////////////////////////////////////////\n");
}
*/

//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================
#if USE_KHEAP
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
	//Your code is here
	//Comment the following line
	// panic("malloc() is not implemented yet...!!");

	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE){
//		dbg_start_address();
		return alloc_block(size);
	}

	// Page allocator for large blocks
		uint32 needed_size = ROUNDUP(size, PAGE_SIZE);
		uint32 addr = 0;
		int idx = -1;

		if(availble_pages < needed_size/PAGE_SIZE) return NULL;
		// Step 1: Check free_blks for available blocks
		// 1a) Search for exact fit
		if(1 == 1){
			// kslt ashell el if dy
			for (int i = 0; i < free_blks_size; i++) {
				if (free_blks[i].size == needed_size) {
					idx = i;
					// cprintf("\n================exact fit found at %x with size = %u pages\n",
							// free_blks[i].start, free_blks[i].size / PAGE_SIZE);
					break;
				}
			}
		}
		// 1b) If no exact fit, search for worst fit
		if (idx == -1 ) {
			uint32 mx_size = 0;
			for (int i = 0; i < free_blks_size; i++) {
				if (free_blks[i].size >= needed_size && free_blks[i].size > mx_size) {
					mx_size = free_blks[i].size;
					idx = i;
				}
			}
		}


		// Step 2a: Found block in free_blks (exact or worst)
		if (idx != -1) {
			addr = free_blks[idx].start;

			// make sure you have enough memory
			if(addr + needed_size > USER_HEAP_MAX){
			//	cprintf("\nInsufficient Memory LEFT\n");
				return NULL;
			}
			// cprintf("Im Gonna alloc at address %x\n size = %u",addr,needed_size/PAGE_SIZE);
			availble_pages -= needed_size/PAGE_SIZE;
			sys_allocate_user_mem(addr, needed_size);

			//push to used_blks
			used_blks[used_size].start = addr;
			used_blks[used_size].size = needed_size;
			used_size++;

			// update free_blks (remove the exact fit blk and shift left)
			if (free_blks[idx].size == needed_size) {
				for (int j = idx; j < free_blks_size - 1; j++)
					free_blks[j] = free_blks[j + 1];
				free_blks_size--;
			}
			else {
				// update worst-fit block
				free_blks[idx].start += needed_size;
				free_blks[idx].size -= needed_size;
			}

			// cprintf("\n========Allocated from free_blks========\n");
		}
		// Step 2b: No fit found - expand heap by moving break or (Insufficient Memory LEFT)
		else {

			addr = uheapPageAllocBreak;
			if(uheapPageAllocBreak + needed_size > USER_HEAP_MAX){
					//		cprintf("\nInsufficient Memory LEFT\naddr = %x , size = %u",addr,needed_size/PAGE_SIZE);
							return NULL;
			}
			// cprintf("Im Gonna alloc (at BREAK) at address %x\n size = %u",addr,needed_size/PAGE_SIZE);
			availble_pages -= needed_size/PAGE_SIZE;
			sys_allocate_user_mem(addr, needed_size);

			// Add to used_blks
			used_blks[used_size].start = addr;
			used_blks[used_size].size = needed_size;
			used_size++;

			// Move break upward (expand heap)
			uheapPageAllocBreak += needed_size;

			// cprintf("\n========Allocated by expanding heap (break moved)========\n");
		}

		// cprintf("\n========================malloc complete======================================\n");
		// dbg_start_address();
		// dbg_unused_address();

		return (void*)addr;
#else
	panic("fos_scheduler_PRIRR() is not implemented yet...!!");
#endif
}




//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{

    // cprintf("\n========================HIII from FREE======================================\n");
	//TODO: [PROJECT'25.IM#2] USER HEAP - #3 free
#if USE_KHEAP
	//Your code is here
	//Comment the following line
	// panic("free() is not implemented yet...!!");
    if ((uint32)virtual_address < USER_HEAP_START || (uint32)virtual_address >= USER_HEAP_MAX)
        panic("INVALID ADDRESS\n");

    if ((uint32)virtual_address < uheapPageAllocStart) {
        free_block(virtual_address);
    //    dbg_start_address();
        return;
    }

    // linear search for the block in used_blks
    for (uint32 i = 0; i < used_size; i++) {
        if (used_blks[i].start == (uint32)virtual_address) {
            // cprintf("freed: start is\n%x\nsize = %u\n",used_blks[i].start,used_blks[i].size);
            availble_pages += used_blks[i].size / PAGE_SIZE;
            sys_free_user_mem(used_blks[i].start, used_blks[i].size);
            // Move block from used_blks to free_blks with merging
            struct Block_Entry_Info freed_block = used_blks[i];

            // Remove from used_blks
            for (uint32 j = i; j < used_size - 1; j++) {
                used_blks[j] = used_blks[j + 1];
            }
            used_size--;

            uint32 insert_pos = 0;

            /*
			 find insert position :
			 TO KEEP FREE BLOCKS ARRAY SORTED (By Addresses)
		 	 IMPORTANT!!
			 */
            while (insert_pos < free_blks_size &&
                   free_blks[insert_pos].start < freed_block.start) {
                insert_pos++;
            }


            for (uint32 j = free_blks_size; j > insert_pos; j--) {
				// shift blks to (free_blks.erase(target_blk))
				free_blks[j] = free_blks[j - 1];
            }
            free_blks_size++;
            free_blks[insert_pos] = freed_block;

            // merge with previous block if free as well
            if (insert_pos > 0 && (free_blks[insert_pos - 1].start + free_blks[insert_pos - 1].size == free_blks[insert_pos].start)) {
                free_blks[insert_pos - 1].size += free_blks[insert_pos].size;
				//update freeblks (erase + merge)
                for (uint32 j = insert_pos; j < free_blks_size - 1; j++) {
                    free_blks[j] = free_blks[j + 1];
                }
                free_blks_size--;
                insert_pos--;
            }

            // merge with next block if free
            if (insert_pos < free_blks_size - 1 &&(free_blks[insert_pos].start + free_blks[insert_pos].size == free_blks[insert_pos + 1].start)) {
                free_blks[insert_pos].size += free_blks[insert_pos + 1].size;

                // update free blocks array
                for (uint32 j = insert_pos + 1; j < free_blks_size - 1; j++) {
                    free_blks[j] = free_blks[j + 1];
                }
                free_blks_size--;
            }

			// update break
			uint32 new_break = uheapPageAllocStart;
            for (uint32 k = 0; k < used_size; k++) {
                uint32 block_end = used_blks[k].start + used_blks[k].size;
                if (block_end > new_break) {
                    new_break = block_end;
                }
            }
            uheapPageAllocBreak = new_break;
//            dbg_start_address();
//            dbg_unused_address();
            return;
        }
    }

    cprintf("Block not ALLOCATED (WRONG FREE)\n");
#endif
}
//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	/// check every "///"
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//cprintf("this is shared malloc init?\n");
	if (size == 0 || sharedVarName == NULL) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
#if USE_KHEAP
	//Your code is here
	/// if(dynallocmaxblock) -----------
	//cprintf("we're really getting some shared stuff huh?\n");
		size = ROUNDUP(size, PAGE_SIZE);
		uint32 addr = 0, idx = -1, numberOfPages = size / PAGE_SIZE;
		if(availble_pages < numberOfPages) return NULL;
		for(int i = 0; i < free_blks_size; i++){
			if(free_blks[i].size == size){
				idx = i;
				break;
				//cprintf("exacto fito\n");
			}
		}
		if(idx == -1){
			uint32 mx = 0;
			for(int i = 0; i < free_blks_size; i++){
				if(free_blks[i].size < mx || free_blks[i].size < size){
					continue;
				}
				mx = free_blks[i].size;
				idx = i;
			}
		}
		if(idx != -1){
			addr = free_blks[idx].start;
			if(addr + size > USER_HEAP_MAX){
				//cprintf("\nINSUFFICIENT MEMORY LEFT\n");
				return NULL;
			}
			availble_pages -= numberOfPages;
			//char* shareName, uint32 size, uint8 isWritable, void* virtual_address
			//cprintf("before create shared from smalloc\n");
			int ret = sys_create_shared_object(sharedVarName, size, isWritable, (void*)addr);
			if(ret < 0){
				return NULL;
			}
			used_blks[used_size].start = addr;
			used_blks[used_size].size = size;
			//cprintf("after create shared from smalloc\n");
			used_size++;
			if(free_blks[idx].size == size){
				for(int j = idx; j < free_blks_size - 1; j++)
					free_blks[j] = free_blks[j+1];
				free_blks_size--;
			}
			else{
				free_blks[idx].start += size;
				free_blks[idx].size -= size;
			}

			//cprintf("returning addr1\n");
			return (void*)addr;
		}
	addr = uheapPageAllocBreak;
	if(uheapPageAllocBreak + size > USER_HEAP_MAX){
		cprintf("\nInsufficient Memory LEFT\naddr = %x , size = %u",addr, numberOfPages);
		return NULL;
	}
	availble_pages -= numberOfPages;
	int ret = sys_create_shared_object(sharedVarName, size, isWritable, (void*)addr);
	if(ret < 0){
		return NULL;
	}
	//used_blks[used_size].start = addr;
	used_blks[used_size].size = size;
	used_blks[used_size].start = addr;
	used_size++;
	//cprintf("returning addr2\n");
	uheapPageAllocBreak += size;
	return (void*) addr;
	//Comment the following line
	//panic("smalloc() is not implemented yet...!!");
#else
	panic("fos_scheduler_PRIRR() is not implemented yet...!!");
#endif
}

//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName){
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//cprintf("sget heap init?\n");
	//==============================================================
	int size = sys_size_of_shared_object(ownerEnvID, sharedVarName);
	//cprintf("got size\n");
	if (size == 0 || sharedVarName == NULL) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
#if USE_KHEAP
	//cprintf("size not soo bad\n");
	//Your code is here
		size = ROUNDUP(size, PAGE_SIZE);
		uint32 addr = 0, idx = -1, numberOfPages = size / PAGE_SIZE;
		if(availble_pages < numberOfPages) return NULL;
		for(int i = 0; i < free_blks_size; i++){
			if(free_blks[i].size == size){
				idx = i;
				//cprintf("exacto fito\n");
				break;
			}
		}
		if(idx == -1){
			uint32 mx = 0;
			for(int i = 0; i < free_blks_size; i++){
				if(free_blks[i].size < mx || free_blks[i].size < size){
					continue;
				}
				mx = free_blks[i].size;
				idx = i;
			}
		}
		if(idx != -1){
			addr = free_blks[idx].start;
			if(addr + size > USER_HEAP_MAX){
				//cprintf("\nINSUFFICIENT MEMORY LEFT\n");
				return NULL;
			}
			availble_pages -= numberOfPages;
			//char* shareName, uint32 size, uint8 isWritable, void* virtual_address
			//cprintf("sys get shared1\n");
			int rt = sys_get_shared_object(ownerEnvID, sharedVarName, (void*)addr);
			if(rt < 0){
				availble_pages -= numberOfPages;
				//cprintf("bad get shared1\n");
				return NULL;
			}
			used_blks[used_size].start = addr;
			used_blks[used_size].size = size;
			used_size++;
			if(free_blks[idx].size == size){
				for(int j = idx; j < free_blks_size - 1; j++)
					free_blks[j] = free_blks[j+1];
				free_blks_size--;
				//cprintf("done pulling\n");
			}
			else{
				free_blks[idx].start += size;
				free_blks[idx].size -= size;
				//cprintf("satr 438 fel uheap asef\n");
			}
			return (void*)addr;
		}
	addr = uheapPageAllocBreak;
	if(uheapPageAllocBreak + size > USER_HEAP_MAX){
		//cprintf("\nInsufficient Memory LEFT\naddr = %x , size = %u",addr, numberOfPages);
		return NULL;
	}
	availble_pages -= numberOfPages;
	int rt = sys_get_shared_object(ownerEnvID, sharedVarName, (void*)addr);
	if(rt < 0){
		availble_pages -= numberOfPages;
		//cprintf("msh ader adek 451 uheap\n");
		return NULL;
	}
	used_blks[used_size].size = size;
	used_blks[used_size].start = addr;
	used_size++;
	uheapPageAllocBreak += size;
	//cprintf("oum ya masry return addr el sget betnadek\n");
	return (void*) addr;
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget

	//Your code is here
	//Comment the following line
	//panic("sget() is not implemented yet...!!");
#else
	panic("fos_scheduler_PRIRR() is not implemented yet...!!");
#endif
}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//== ================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}

