/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>
#include <kern/mem/kheap.h>

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE) {
	assert(
			LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE;
}
void setPageReplacmentAlgorithmCLOCK() {
	_PageRepAlgoType = PG_REP_CLOCK;
}
void setPageReplacmentAlgorithmFIFO() {
	_PageRepAlgoType = PG_REP_FIFO;
}
void setPageReplacmentAlgorithmModifiedCLOCK() {
	_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;
}
/*2018*/void setPageReplacmentAlgorithmDynamicLocal() {
	_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;
}
/*2021*/void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps) {
	_PageRepAlgoType = PG_REP_NchanceCLOCK;
	page_WS_max_sweeps = PageWSMaxSweeps;
}
/*2024*/void setFASTNchanceCLOCK(bool fast) {
	FASTNchanceCLOCK = fast;
}
;
/*2025*/void setPageReplacmentAlgorithmOPTIMAL() {
	_PageRepAlgoType = PG_REP_OPTIMAL;
}
;

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE) {
	return _PageRepAlgoType == LRU_TYPE ? 1 : 0;
}
uint32 isPageReplacmentAlgorithmCLOCK() {
	if (_PageRepAlgoType == PG_REP_CLOCK)
		return 1;
	return 0;
}
uint32 isPageReplacmentAlgorithmFIFO() {
	if (_PageRepAlgoType == PG_REP_FIFO)
		return 1;
	return 0;
}
uint32 isPageReplacmentAlgorithmModifiedCLOCK() {
	if (_PageRepAlgoType == PG_REP_MODIFIEDCLOCK)
		return 1;
	return 0;
}
/*2018*/uint32 isPageReplacmentAlgorithmDynamicLocal() {
	if (_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL)
		return 1;
	return 0;
}
/*2021*/uint32 isPageReplacmentAlgorithmNchanceCLOCK() {
	if (_PageRepAlgoType == PG_REP_NchanceCLOCK)
		return 1;
	return 0;
}
/*2021*/uint32 isPageReplacmentAlgorithmOPTIMAL() {
	if (_PageRepAlgoType == PG_REP_OPTIMAL)
		return 1;
	return 0;
}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt) {
	_EnableModifiedBuffer = enableIt;
}
uint8 isModifiedBufferEnabled() {
	return _EnableModifiedBuffer;
}

void enableBuffering(uint32 enableIt) {
	_EnableBuffering = enableIt;
}
uint8 isBufferingEnabled() {
	return _EnableBuffering;
}

void setModifiedBufferLength(uint32 length) {
	_ModifiedBufferLength = length;
}
uint32 getModifiedBufferLength() {
	return _ModifiedBufferLength;
}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [0] INIT HANDLER:
//==================
void fault_handler_init() {
	//setPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX);
	//setPageReplacmentAlgorithmOPTIMAL();
	setPageReplacmentAlgorithmCLOCK();
	//setPageReplacmentAlgorithmModifiedCLOCK();
	enableBuffering(0);
	enableModifiedBuffer(0);
	setModifiedBufferLength(1000);
}
//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault = 0;
extern uint32 sys_calculate_free_frames();

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf) {
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//cprintf("************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env) {
		num_repeated_fault++;
		if (num_repeated_fault == 3) {
			print_trapframe(tf);
			panic(
					"Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n",
					before_last_fault_va, before_last_eip, fault_va);
		}
	} else {
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32) tf->tf_eip;
	last_fault_va = fault_va;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap) {
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env
				&& fault_va
						>= (uint32) cur_env->kstack&& fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va
				>= (uint32) c->stack&& fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!",
					c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else {
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL) {
		cprintf("\nFaulted VA = %x\n", fault_va);
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ((faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT)
			!= PERM_PRESENT) {
		faulted_env->tableFaultsCounter++;
		table_fault_handler(faulted_env, fault_va);
	} else {
		if (userTrap) {
#if USE_KHEAP
			/*============================================================================================*/
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here
#define and &&
			uint32* cur_table = NULL;
			get_page_table(faulted_env->env_page_directory, fault_va,
					&cur_table);
			int cur_perms;
			if (cur_table == NULL) {
				panic("There is no pagee table for that va");
			} else {
				cur_perms = cur_table[PTX(fault_va)];
			}
			if ((cur_perms & PERM_UHPAGE) == 0
					and (fault_va >= USER_HEAP_START
							and fault_va < USER_HEAP_MAX)) {
				cprintf("page unmarked in user heap\n");
				env_exit();
			} else if (fault_va >= USER_LIMIT) {
				cprintf("itis out of user control it is the kernal\n");
				env_exit();
			} else if ((cur_perms & PERM_PRESENT) == 1
					and (cur_perms & PERM_WRITEABLE) == 0) {
				cprintf("it is not writable\n");
				env_exit();
			}
#endif
			/*============================================================================================*/
		}

		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory,
				fault_va);
		if (perms & PERM_PRESENT)
			panic(
					"Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n",
					fault_va);
		/*============================================================================================*/

		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter++;

//				cprintf("[%08s] user PAGE fault va %08x\n", faulted_env->prog_name, fault_va);
//				cprintf("\nPage working set BEFORE fault handler...\n");
//				env_page_ws_print(faulted_env);
		//int ffb = sys_calculate_free_frames();

		if (isBufferingEnabled()) {
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		} else {
			page_fault_handler(faulted_env, fault_va);
		}

		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(faulted_env);
		//		int ffa = sys_calculate_free_frames();
		//		cprintf("fault handling @%x: difference in free frames (after - before = %d)\n", fault_va, ffa - ffb);
	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}

//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va) {
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory,
				(uint32) fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//=========================
// [3] PAGE FAULT HANDLER:
//=========================
/* Calculate the number of page faults according th the OPTIMAL replacement strategy
 * Given:
 * 	1. Initial Working Set List (that the process started with)
 * 	2. Max Working Set Size
 * 	3. Page References List (contains the stream of referenced VAs till the process finished)
 *
 * 	IMPORTANT: This function SHOULD NOT change any of the given lists
 */
int ret = 0;
int get_optimal_num_faults(struct WS_List *initWorkingSet, int maxWSSize,
		struct PageRef_List *pageReferences) {
	//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #2 get_optimal_num_faults
	//Your code is here
	int ptr = 0;
	uint32* arr = (uint32*) kmalloc(maxWSSize * sizeof(uint32));
	struct WorkingSetElement *ptr1 = NULL;
	LIST_FOREACH(ptr1,initWorkingSet)
	{
		arr[ptr] = ROUNDDOWN(ptr1->virtual_address, PAGE_SIZE);
		ptr++;
	}
	int cnt = 0;
	struct PageRefElement *ptr2 = NULL;
	LIST_FOREACH(ptr2,pageReferences)
	{
		bool found = 0;
		for (int i = 0; i < ptr; i++) {
			if (arr[i] == ROUNDDOWN(ptr2->virtual_address, PAGE_SIZE)) {
				found = 1;
			}
		}
		if (found)
			continue;
		if (ptr < maxWSSize) {
			cnt++;
			arr[ptr] = ROUNDDOWN(ptr2->virtual_address, PAGE_SIZE);
			ptr++;
		} else {
			cnt++;
			int idx = 0, nxt = -1;
			for (int i = 0; i < ptr; i++) {
				int nnxt = -1, dist = 0;
				struct PageRefElement *cur = ptr2->prev_next_info.le_next;
				while (cur != NULL) {
					if (ROUNDDOWN(cur->virtual_address, PAGE_SIZE) == arr[i]) {
						nnxt = dist;
						break;
					}
					dist++;
					cur = cur->prev_next_info.le_next;
				}
				if (nnxt == -1) {
					idx = i;
					break;
				}
				if (nnxt > nxt) {
					nxt = nnxt;
					idx = i;
				}
			}
			arr[idx] = ROUNDDOWN(ptr2->virtual_address, PAGE_SIZE);
		}
	}
	kfree(arr);
	return cnt;
	//Comment the following line
	//panic("get_optimal_num_faults() is not implemented yet...!!");
}

bool first = 1;
struct WS_List copylist;
int mxsz;
struct WorkingSetElement* lastws;
void page_fault_handler(struct Env * faulted_env, uint32 fault_va) {
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL()) {

		//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #1 Optimal Reference Stream
		//Your code is here

		fault_va = ROUNDDOWN(fault_va, PAGE_SIZE);
		uint32*Y = NULL;
		if (first) {
			LIST_INIT(&copylist);
			struct WorkingSetElement *ptr = NULL;
			LIST_FOREACH(ptr , &(faulted_env->page_WS_list))
			{
				struct WorkingSetElement *nnew =
						env_page_ws_list_create_element(faulted_env,
								ptr->virtual_address);
				LIST_INSERT_TAIL(&(copylist), nnew);
			}
			mxsz = faulted_env->page_WS_max_size;
			lastws = faulted_env->page_last_WS_element;
			first = 0;
		}

		struct FrameInfo *X = get_frame_info(faulted_env->env_page_directory,
				fault_va, &Y);
		if (X == NULL) {
		    ret++;
			allocate_frame(&X);
			map_frame(faulted_env->env_page_directory, X, fault_va,
			PERM_PRESENT | PERM_USER | PERM_WRITEABLE);
			int ret = pf_read_env_page(faulted_env, (void *) fault_va);
			if (ret == E_PAGE_NOT_EXIST_IN_PF) {
				pt_set_page_permissions(faulted_env->env_page_directory,
						fault_va, PERM_PRESENT, 0);
			}
		}

		struct WorkingSetElement *ptr = NULL;
		bool found = 0;
		LIST_FOREACH(ptr , &(copylist))
		{
			if (ROUNDDOWN(ptr->virtual_address,PAGE_SIZE) == fault_va) {
				found = 1;
				break;
			}
		}

		if (!found) {
			ret++;
			uint32 wsSize = LIST_SIZE(&(copylist));
			if (wsSize < (faulted_env->page_WS_max_size)) {
				struct WorkingSetElement *wselem =
						env_page_ws_list_create_element(faulted_env, fault_va);
				LIST_INSERT_TAIL(&(copylist), wselem);
				pt_set_page_permissions(faulted_env->env_page_directory,
						fault_va, PERM_PRESENT, 0);
				struct PageRefElement *PRF = (struct PageRefElement*) kmalloc(
						sizeof(struct PageRefElement));
				PRF->virtual_address = fault_va;
				LIST_INSERT_TAIL(&(faulted_env->referenceStreamList), PRF);

			} else {
				LIST_FOREACH(ptr , &(copylist))
				{
					pt_set_page_permissions(faulted_env->env_page_directory,
							ptr->virtual_address, 0, PERM_PRESENT);
					LIST_REMOVE(&(copylist), ptr);
				}
				struct WorkingSetElement *wselem =
						env_page_ws_list_create_element(faulted_env, fault_va);
				LIST_INSERT_TAIL(&(copylist), wselem);
				pt_set_page_permissions(faulted_env->env_page_directory,
						fault_va, PERM_PRESENT, 0);
				struct PageRefElement* PRF = (struct PageRefElement*) kmalloc(
						sizeof(struct PageRefElement));
				PRF->virtual_address = fault_va;
				LIST_INSERT_TAIL(&(faulted_env->referenceStreamList), PRF);

			}

		}

		//Comment the following line
		//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");

	} else {
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
		if (wsSize < (faulted_env->page_WS_max_size)) {

			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #3 placement
			//Your code is here
			//Comment the following line
			//panic("page_fault_handler().PLACEMENT is not implemented yet...!!");
#define and &&
#define or ||
			struct FrameInfo *ptr_frame_info;
			allocate_frame(&ptr_frame_info);
			map_frame(faulted_env->env_page_directory, ptr_frame_info, fault_va,
			PERM_PRESENT | PERM_USER | PERM_WRITEABLE);
			int ret = pf_read_env_page(faulted_env, (void *) fault_va);

			if (ret == E_PAGE_NOT_EXIST_IN_PF) {
				if (!((fault_va >= USER_HEAP_START and fault_va < USER_HEAP_MAX)
						or (fault_va >= USTACKBOTTOM and fault_va < USTACKTOP))) {
					unmap_frame(faulted_env->env_page_directory, fault_va);
					cprintf("FBI , openup , illegal access");
					env_exit();
				}
			}

			struct WorkingSetElement *wselem = env_page_ws_list_create_element(
					faulted_env, fault_va);
             if(faulted_env->page_last_WS_element == NULL){

     			LIST_INSERT_TAIL(&(faulted_env->page_WS_list), wselem);
     			if (LIST_SIZE(&(faulted_env->page_WS_list))
     					== faulted_env->page_WS_max_size) {
     				faulted_env->page_last_WS_element = LIST_FIRST(
     						&(faulted_env->page_WS_list));
     			}

             }else{
            	 LIST_INSERT_BEFORE(&(faulted_env->page_WS_list),faulted_env->page_last_WS_element,wselem);
             }

		} else {
			if (isPageReplacmentAlgorithmCLOCK()) {
				//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #3 Clock Replacement
				//Your code is here
#define and &&
#define or ||
			//	cprintf("FBI , the woorking set before : \n");
		//		env_page_ws_print(faulted_env);
				struct WorkingSetElement *ptr =
						faulted_env->page_last_WS_element;
				struct WorkingSetElement *victim = NULL;
				int cnt = 0;
				while (1) {
					if (ptr == NULL)
						ptr = LIST_FIRST(&(faulted_env->page_WS_list));
					if (ptr == faulted_env->page_last_WS_element)
						cnt++;
					if (cnt == 2 and victim == NULL) {
						victim = ptr;
						break;
					}
					uint32 perms = pt_get_page_permissions(
							faulted_env->env_page_directory,
							ptr->virtual_address);
					if (perms & PERM_USED) {
						pt_set_page_permissions(faulted_env->env_page_directory,
								ptr->virtual_address, 0, PERM_USED);

					} else {
						victim = ptr;
						break;
					}
					ptr = ptr->prev_next_info.le_next;
				}
				uint32 remva = victim->virtual_address;
				uint32 perms = pt_get_page_permissions(
						faulted_env->env_page_directory, remva);
				if (perms & PERM_MODIFIED) {
					uint32 *temp = NULL;
					struct FrameInfo *Frameptr = get_frame_info(
							faulted_env->env_page_directory, remva, &temp);
					int ret = pf_update_env_page(faulted_env, remva, Frameptr);
					if (ret == E_NO_PAGE_FILE_SPACE) {
						panic("no space in page file\n");
					}
				}
				unmap_frame(faulted_env->env_page_directory, remva);
				struct FrameInfo *ptr_frame_info;
				allocate_frame(&ptr_frame_info);
				map_frame(faulted_env->env_page_directory, ptr_frame_info,
						fault_va,
						PERM_PRESENT | PERM_USER | PERM_WRITEABLE);
				ret = pf_read_env_page(faulted_env, (void *) fault_va);
				if (ret == E_PAGE_NOT_EXIST_IN_PF) {
					if (!((fault_va >= USER_HEAP_START
							and fault_va < USER_HEAP_MAX)
							or (fault_va >= USTACKBOTTOM
									and fault_va < USTACKTOP))) {
						unmap_frame(faulted_env->env_page_directory, fault_va);
						cprintf("FBI , openup , illegal access");
						env_exit();
					}
				}
				victim->virtual_address = fault_va;
				struct WorkingSetElement *nxt = victim->prev_next_info.le_next;
				if (nxt == NULL)
					nxt = LIST_FIRST(&(faulted_env->page_WS_list));
				if (LIST_SIZE(&(faulted_env->page_WS_list))
						< faulted_env->page_WS_max_size) {
					faulted_env->page_last_WS_element = NULL;
				} else
					faulted_env->page_last_WS_element = nxt;
			//	cprintf("FBI , the woorking set after : \n");
				//env_page_ws_print(faulted_env);

				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");

			} else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX)) {
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #2 LRU Aging Replacement
				//Your code is here
				//Comment the following line
				panic(
						"page_fault_handler().REPLACEMENT is not implemented yet...!!");
			} else if (isPageReplacmentAlgorithmModifiedCLOCK()) {
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #3 Modified Clock Replacement
				//Your code is here
				//Comment the following line
				panic(
						"page_fault_handler().REPLACEMENT is not implemented yet...!!");
			}
		}
	}
#endif
}

void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va) {
	panic("this function is not required...!!");
}

