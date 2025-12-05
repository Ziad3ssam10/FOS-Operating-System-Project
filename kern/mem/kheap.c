#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"

// MY GLOBALS ..............................................

struct kspinlock lock;
//uint32 phy_to_virt[ (KERNEL_HEAP_MAX / PAGE_SIZE) + 50]; // for phy2virt , should be updatedin kmalloc and kfree
uint32 virt_to_size[(KERNEL_HEAP_MAX / PAGE_SIZE) + 50];


//............................................................

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)
void kheap_init()
{
	init_kspinlock(&lock,"k_lock");
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart;
		memset(virt_to_size, 0, sizeof(virt_to_size));
		//memset(phy_to_virt, 0, sizeof(phy_to_virt));
	}
	//==================================================================================
	//==================================================================================
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);
	if (ret < 0)
		panic("get_page() in kern: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================

void* kmalloc(unsigned int size)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #1 kmalloc
	//Your code is here
	//void* addr;
	acquire_kspinlock(&lock);
	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE){
		//acquire_kspinlock(&lock);
		void* addr = alloc_block(size);
		if(addr == NULL){
			release_kspinlock(&lock);
			return NULL;
		}
		if((uint32) addr < KERNEL_HEAP_START || (uint32) addr >= (KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE)){
			cprintf("FBBI , itis out of bounds  ");
		}
		release_kspinlock(&lock);
		return addr;
	}
	if(size > KERNEL_HEAP_MAX - (kheapPageAllocStart))
	{
		release_kspinlock(&lock);
		return NULL;
	}
	uint32 num_of_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	uint32 st = kheapPageAllocStart;
	uint32 end = kheapPageAllocBreak;
	uint32 cur_st = 0;
	uint32 cur_pg_cnt = 0;
	uint32 worst_st = 0;
	uint32 worst_cnt = 0;
	uint32 exact_st = -1;

	for(uint32 cur = st; cur < end; cur += PAGE_SIZE){
		uint32 *pt;
		struct FrameInfo *fi = get_frame_info(ptr_page_directory, cur, &pt);
		if(fi == NULL){
			// empty frame
			if(cur_pg_cnt == 0){
				cur_st = cur;
			}
			cur_pg_cnt++;

		}
		else{
			if(cur_pg_cnt == num_of_pages){
				exact_st = cur_st;
				break;
			}
			// free region ended         /// should think about when the free region ends by kheap max or kheap break?, should consider kheap break instead of kheap max?
			if(cur_pg_cnt > worst_cnt){
				worst_cnt = cur_pg_cnt;
				worst_st = cur_st;
			}
			cur_pg_cnt = cur_st = 0;
		}
	}
	if(cur_pg_cnt == num_of_pages){
		exact_st = cur_st;
	}

	if(cur_pg_cnt > worst_cnt){
		worst_cnt = cur_pg_cnt;
		worst_st = cur_st;
	}
	cur_pg_cnt = 0;
	cur_st = 0;

	uint32 st_of_pg_alloc;
	if (worst_cnt < num_of_pages && exact_st == -1) {
		uint32 alloc_start = kheapPageAllocBreak;
		if (num_of_pages > ((KERNEL_HEAP_MAX - kheapPageAllocBreak)/PAGE_SIZE)) {
			release_kspinlock(&lock);
			return NULL;
		}
		kheapPageAllocBreak += (num_of_pages * PAGE_SIZE);
		st_of_pg_alloc = alloc_start;
	} else {
		st_of_pg_alloc = (exact_st == -1 ? worst_st : exact_st);
	}
	const uint32 end_of_pg_alloc = st_of_pg_alloc + num_of_pages * PAGE_SIZE;
	for(uint32 cur = st_of_pg_alloc; cur < end_of_pg_alloc; cur += PAGE_SIZE){
		void* ret = (void*)cur;
		get_page(ret);
	}
	virt_to_size[st_of_pg_alloc / PAGE_SIZE] = num_of_pages * PAGE_SIZE;
	release_kspinlock(&lock);
	void* ret = (void*)st_of_pg_alloc;
	return ret;

	//Comment the following line
	// kpanic_into_prompt("kmalloc() is not implemented yet...!!");
	//TODO: [PROJECT'25.BONUS#3] FAST PAGE ALLOCATOR
#else
	panic("fos_scheduler_PRIRR() is not implemented yet...!!");
#endif
}

//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void move_break(){
	struct FrameInfo *fi = NULL;
	while(kheapPageAllocBreak - PAGE_SIZE >= kheapPageAllocStart && fi == NULL){
		uint32 *pt;
		fi = get_frame_info(ptr_page_directory, kheapPageAllocBreak - PAGE_SIZE, &pt);
		if(fi == NULL)
			kheapPageAllocBreak -= PAGE_SIZE;
	}
}

void kfree(void* virtual_address){
#if USE_KHEAP
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	//Your code is here
	acquire_kspinlock(&lock);
	//if(virtual_address == NULL || (uint32)virtual_address >= kheapPageAllocBreak || (uint32)virtual_address < KERNEL_HEAP_START){
		//panic("kfree(): invalid virtual address");
	//}
	uint32 va = ROUNDDOWN((uint32)virtual_address, PAGE_SIZE);
	if(va >= dynAllocStart && va < dynAllocEnd){
		//acquire_kspinlock(&lock);
		free_block(virtual_address);
		release_kspinlock(&lock);
		return;
	}
	if(va < kheapPageAllocStart){
		panic("invalid virtual address");
	}
	uint32 sz = virt_to_size[va / PAGE_SIZE];
	virt_to_size[va / PAGE_SIZE] = 0;
	const uint32 bound = va + sz;
	for(uint32 current = va; current < bound; current += PAGE_SIZE){
		return_page((void *)current);
	}
	move_break();
	release_kspinlock(&lock);
	//Comment the following line
	//panic("kfree() is not implemented yet...!!");
#endif
}

//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #3 kheap_virtual_address
	//Your code is here
	//acquire_kspinlock(&lock);         -----------------
	if(physical_address >= KERNEL_HEAP_MAX){
		panic("kheap_virtual_address(): invalid physical address given");
	}
	struct FrameInfo *fi = to_frame_info(physical_address);
	if(fi == NULL || fi->va == 0)
		return 0;
	else
		return (unsigned int) ((fi -> va) | ((physical_address) & ~PAGE_MASK)); // pagemask
	//Comment the following line
	//panic("kheap_virtual_address() is not implemented yet...!!");
#else
	panic("fos_scheduler_PRIRR() is not implemented yet...!!");
#endif
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================

unsigned int kheap_physical_address(unsigned int virtual_address){
#if USE_KHEAP
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #4 kheap_physical_address
	//Your code is here
	//acquire_kspinlock(&lock);
	uint32 *ptr = NULL;
	uint32 offset = PGOFF(virtual_address);
	get_page_table(ptr_page_directory, virtual_address, &ptr);
	if (ptr == NULL){
		//release_kspinlock(&lock);
		return 0;
	}
	uint32 pgTableEntry = ptr[PTX(virtual_address)];
	if (!(pgTableEntry & PERM_PRESENT))
	{
		//release_kspinlock(&lock);
		return 0;
	}
	uint32 f_num = pgTableEntry >> 12;
	uint32 pa = f_num * PAGE_SIZE + offset;
	//release_kspinlock(&lock);
	return pa;

	//Comment the following line
	// panic("kheap_physical_address() is not implemented yet...!!");
#else
	panic("fos_scheduler_PRIRR() is not implemented yet...!!");
#endif
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED*/
}


//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

extern __inline__ uint32 get_block_size(void *va);

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	panic("krealloc() is not implemented yet...!!");
}
