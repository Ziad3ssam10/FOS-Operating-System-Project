#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_kspinlock(&AllShares.shareslock, "shares lock");
	init_kspinlock(&locky, "my locky");
	//init_sleeplock(&AllShares.sharessleeplock, "shares sleep lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//=========================
// [2] Find Share Object:
//=========================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* find_share(int32 ownerID, char* name)
{
#if USE_KHEAP
	//cprintf("hi find share\n");
	struct Share * ret = NULL;
	bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld)
	{
		acquire_kspinlock(&(AllShares.shareslock));
	}
	{
		struct Share * shr ;
		LIST_FOREACH(shr, &(AllShares.shares_list))
		{
			//cprintf("shared var name = %s compared with %s\n", name, shr->name);
			if(shr->ownerID == ownerID && strcmp(name, shr->name)==0)
			{
				//cprintf("%s found\n", name);
				ret = shr;
				break;
			}
		}
	}
	if (!wasHeld)
	{
		release_kspinlock(&(AllShares.shareslock));
	}
	//cprintf("bye find share --\n");
	return ret;
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [3] Get Size of Share Object:
//==============================
int size_of_shared_object(int32 ownerID, char* shareName)
{
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	//cprintf("this shit is what size of shared object is\n");
	struct Share* ptr_share = find_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}
//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=====================================
// [1] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* alloc_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #1 alloc_share
	//Your code is here
	/*bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld){
		acquire_kspinlock(&(AllShares.shareslock));
	}*/
	//cprintf("efta7 el alloc share\n");
	struct Share *sharedObj = (struct Share *) kmalloc(sizeof(struct Share));
	if(sharedObj == NULL){
		/*if (!wasHeld){
			release_kspinlock(&(AllShares.shareslock));
		}*/
		//cprintf("awel null\n");
		return NULL;
	}

	sharedObj -> framesStorage = (struct FrameInfo **) kmalloc(sizeof(struct Framinfo *)*(size/PAGE_SIZE));
	if(sharedObj -> framesStorage == NULL){
		//cprintf("abl el kfree\n");
		kfree(sharedObj);
		/*if (!wasHeld){
			release_kspinlock(&(AllShares.shareslock));
		}*/
		//cprintf("ba3d el kfree\n");
		return NULL;
	}
//	cprintf("memset\n");
	memset(sharedObj -> framesStorage, 0, sizeof(sharedObj -> framesStorage));
		/*
		// list link pointers
		LIST_ENTRY(Share) prev_next_info;*/
	/// how to mask?
	sharedObj -> ID = ((uint32)(sharedObj) & ( ~(1U << 31) ));
	sharedObj -> ownerID = ownerID;
    //	sharedObj -> name = shareName;   /// //////safer way?
	for(uint32 i = 0; i < 63; i++){
		sharedObj -> name[i] = shareName[i];
	}
	//cprintf("tam el esm bnagah\n");
	sharedObj -> size = size;
	sharedObj -> references = 1;
	sharedObj -> isWritable = isWritable;
	/*if (!wasHeld){
		release_kspinlock(&(AllShares.shareslock));
	}*/

	//cprintf("raga3 el shared\n");
	return sharedObj;
	//Comment the following line
	//panic("alloc_share() is not implemented yet...!!");
#else
	panic("fos_scheduler_PRIRR() is not implemented yet...!!");
#endif
}


//=========================
// [4] Create Share Object:
//=========================
int create_shared_object(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #3 create_shared_object
#if USE_KHEAP
	//Your code is here
	//Comment the following line
	//panic("create_shared_object() is not implemented yet...!!");
	//cprintf("create_shared_object b reglak elyemen\n");
	if (shareName == NULL || size == 0 || virtual_address == NULL) {
	//	cprintf("wtf eh da?\n");
		return E_NO_SHARE;
	}
	size = ROUNDUP(size, PAGE_SIZE);
	/*if (!wasHeld){
		acquire_kspinlock(&(AllShares.shareslock));
	}*/

//	cprintf("yatem ID\n");
	int ID = ((uint32)(virtual_address) & ( ~(1U << 31) ));
	struct Share * shr = find_share(ownerID, shareName);
	if(shr != NULL){
		/*if(!wasHeld){
			release_kspinlock(&(AllShares.shareslock));
		}*/
	//	cprintf("shr is not NULL a7eh\n");
		return E_SHARED_MEM_EXISTS;
	}
	struct Env* myenv = get_cpu_proc(); //The calling environment
	shr = alloc_share(ownerID, shareName, size, isWritable);
	if(shr == NULL){
		/*if (!wasHeld){
			release_kspinlock(&(AllShares.shareslock));
		}*/
	//	cprintf("shr is NULL ba3d el alloc_share a7eh\n");
		return E_NO_SHARE;
	}

	uint32 va = ROUNDDOWN((uint32) virtual_address, PAGE_SIZE);
	uint32 lim = va + size;
	uint32 perms = PERM_USER | PERM_UHPAGE; // yalla ya baba
	perms |= PERM_WRITEABLE;
	//cprintf("perm writable\n");
	for(uint32 i = va; i < lim; i += PAGE_SIZE){
		bool wasHeld = holding_kspinlock(&(locky));
		if(!wasHeld){
			acquire_kspinlock(&(locky));
		}
		int rt = alloc_page(myenv->env_page_directory, i, perms, 1);
		if(!wasHeld){
			release_kspinlock(&(locky));
		}
		struct FrameInfo *fi = NULL;
		uint32 *pt;
		if(!(rt < 0)){
			fi = get_frame_info(myenv -> env_page_directory, i, &pt);
		}
		if(rt < 0 || fi == NULL){
		//	cprintf("couldn't get frame\n");
			for(uint32 j = va; j < i; j += PAGE_SIZE){
				if(!wasHeld){
					acquire_kspinlock(&(locky));
				}
				unmap_frame(myenv -> env_page_directory, j);
				if(!wasHeld){
					release_kspinlock(&(locky));
				}
			}
			//cprintf("abl el kfree\n");
			kfree(shr->framesStorage);
			kfree(shr);
		//	cprintf("ba3d el kfree\n");
			/*if(!wasHeld){
				release_kspinlock(&(AllShares.shareslock));
			}*/
			return E_NO_SHARE;
		}
		shr->framesStorage[(i - va)/PAGE_SIZE] = fi;
	}
	bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld){
		acquire_kspinlock(&(AllShares.shareslock));
	}
	LIST_INSERT_HEAD(&(AllShares.shares_list), shr);
	if(!wasHeld){
		release_kspinlock(&(AllShares.shareslock));
	}
	return shr->ID;
	// This function should create the shared object at the given virtual address with the given size
	// and return the ShareObjectID
	// RETURN:
//	a) ID of the shared object (its VA after masking out its msb) if success
//	b) E_SHARED_MEM_EXISTS if the shared object already exists
//	c) E_NO_SHARE if failed to create a shared object
#else
	panic("fos_scheduler_PRIRR() is not implemented yet...!!");
#endif
}


//======================
// [5] Get Share Object:
//======================
int get_shared_object(int32 ownerID, char* shareName, void* virtual_address)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #5 get_shared_object
#if USE_KHEAP
	//Your code is here
	//Comment the following line
	//panic("get_shared_object() is not implemented yet...!!");
	//cprintf("get shared object no panic\n");
	struct Env* myenv = get_cpu_proc(); //The calling environment
	// get shared object from shares_list,
	// bool was held
	// get wanted ID
	// get its physical frames from frames_storage
	// share these frames starting from the given va
	// use the isWritable flage
	// update references?
	struct Share *shr;

	shr = find_share(ownerID, shareName);
	if(shr == NULL){
		/*if(!wasHeld){
			release_kspinlock(&(AllShares.shareslock));
		}*/
		//cprintf("there is no shared, mem no exist\n");
		return E_SHARED_MEM_NOT_EXISTS;
	}
	uint32 va = (uint32)virtual_address;
	uint32 lim = ROUNDUP(shr -> size, PAGE_SIZE) + va;
	uint32 perms = PERM_USER | ((shr -> isWritable)? PERM_WRITEABLE : 0);
	for(uint32 i = va; i < lim; i += PAGE_SIZE){
		struct FrameInfo *fi = shr -> framesStorage[(i-va)/PAGE_SIZE];
		bool wasHeld = holding_kspinlock(&(locky));
		if(!wasHeld){
			acquire_kspinlock(&(locky));
		}
		int ret = map_frame(myenv->env_page_directory, fi, i, perms);
		if(!wasHeld){
			release_kspinlock(&(locky));
		}
		if(ret != 0){
			panic("Couldn't map frame");
		}
	}
	bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld){
		acquire_kspinlock(&(AllShares.shareslock));
	}
	shr -> references++;
	if(!wasHeld){
		release_kspinlock(&(AllShares.shareslock));
	}
	//cprintf("get and return share id\n");
	return shr->ID;
	// 	This function should share the required object in the heap of the current environment
	//	starting from the given virtual_address with the specified permissions of the object: read_only/writable
	// 	and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists
#else
	panic("fos_scheduler_PRIRR() is not implemented yet...!!");
	#endif
}
//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//
//=========================
// [1] Delete Share Object:
//=========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - free_share
	//Your code is here
	//Comment the following line
	panic("free_share() is not implemented yet...!!");
}


//=========================
// [2] Free Share Object:
//=========================
int delete_shared_object(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - delete_shared_object
	//Your code is here
	//Comment the following line
	panic("delete_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should free (delete) the shared object from the User Heapof the current environment
	// If this is the last shared env, then the "frames_store" should be cleared and the shared object should be deleted
	// RETURN:
	//	a) 0 if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	// Steps:
	//	1) Get the shared object from the "shares" array (use get_share_object_ID())
	//	2) Unmap it from the current environment "myenv"
	//	3) If one or more table becomes empty, remove it
	//	4) Update references
	//	5) If this is the last share, delete the share object (use free_share())
	//	6) Flush the cache "tlbflush()"

}

