//
//	memory.c
//
//	Routines for dealing with memory management.

// Jason was here.

//static char rcsid[] = "$Id: memory.c,v 1.1 2000/09/20 01:50:19 elm Exp elm $";

#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "memory.h"
#include "queue.h"

// num_pages = size_of_memory / size_of_one_page
// 2MB / 4KB = 512 physical pages
// 512 pages, 1 bit per page = 16 uint32 freemap entries
static uint32 freemap[16]; //TODO: find a way to generate from memory constants
static uint32 pagestart;
static int nfreepages;
static int freemapmax;

//----------------------------------------------------------------------
//
//	This silliness is required because the compiler believes that
//	it can invert a number by subtracting it from zero and subtracting
//	an additional 1.  This works unless you try to negate 0x80000000,
//	which causes an overflow when subtracted from 0.  Simply
//	trying to do an XOR with 0xffffffff results in the same code
//	being emitted.
//
//----------------------------------------------------------------------
static int negativeone = 0xFFFFFFFF;
static inline uint32 invert (uint32 n) {
  return (n ^ negativeone);
}

//----------------------------------------------------------------------
//
//	MemoryGetSize
//
//	Return the total size of memory in the simulator.  This is
//	available by reading a special location.
//
//----------------------------------------------------------------------
int MemoryGetSize() {
  return (*((int *)DLX_MEMSIZE_ADDRESS));
}


//----------------------------------------------------------------------
//
//	MemoryModuleInit
//
//	Initialize the memory module of the operating system.
//      Basically just need to setup the freemap for pages, and mark
//      the ones in use by the operating system as "VALID", and mark
//      all the rest as not in use.
//
//----------------------------------------------------------------------
void MemoryModuleInit() {
  int i;
  int lastPage;
  if (lastosaddress % 4096 == 0) {
    lastPage = lastosaddress / 4096;
  } else {
    lastPage = (lastosaddress / 4096) + 1; //This is truncated, so make sure the last nonfilled page is counted
  }
  dbprintf('z', "MemoryModuleInit (%d): marking pages in use in freemap through address %x, page %d\n", GetCurrentPid(), lastosaddress, lastPage); 
  
  for (i = 0; i < 16; i++) //TODO: derive 16
  {
    freemap[i] = 0x00000000;
  }

  for (i = 0; i < lastPage; i++)
  {
    // i / 32 is which freemap entry this falls on
    // i % 32 is which bit in the entry it falls on
    freemap[i / 32] |= (0x1 << (i % 32)); //USEABLE IF 0, IN USE IF 1
  }
}


//----------------------------------------------------------------------
//
// MemoryTranslateUserToSystem
//
//	Translate a user address (in the process referenced by pcb)
//	into an OS (physical) address.  Return the physical address.
//
//----------------------------------------------------------------------
uint32 MemoryTranslateUserToSystem (PCB *pcb, uint32 addr) {
  uint32 page, offset, pte, physpage, physaddr;
  page = addr >> 12; /* 4KB pages */
  offset = addr & MEM_ADDRESS_OFFSET_MASK;

  //basic bounds check
  if (page >= MEM_NUM_PAGE_TABLE_ENTRIES) {
    dbprintf('m', "MemoryTranslateUserToSystem: page %d out of range\n", page);
    return 0;
  }

  //4-byte alignment check
  /*
  if (addr & 0x3) {
    dbprintf('m', "MemoryTranslateUserToSystem: address 0x%x is not 4-byte aligned.\n", addr);
    //addr = addr & 0xFFFFFFFC;
    ProcessKill();
    return 0;
  }
    */

  pte = pcb->pagetable[page];
  if (!(pte & MEM_PTE_VALID)) {
    //not mapped
    dbprintf('m', "MemoryTranslateUserToSystem: vpage %d not mapped (pte=0x%x)\n", page, pte);
    return 0;
  }

  //physical page number is stored in the high bits of the PTE (we store page<<12)
  physpage = pte >> 12;
  physaddr = (physpage << 12) | offset;
  dbprintf('m', "MemoryTranslateUserToSystem: addr 0x%x, vpage %d valid (pte=0x%x), returning physaddr 0x%x.\n", addr, page, pte, physaddr);
  return physaddr;
}


//----------------------------------------------------------------------
//
//	MemoryMoveBetweenSpaces
//
//	Copy data between user and system spaces.  This is done page by
//	page by:
//	* Translating the user address into system space.
//	* Copying all of the data in that page
//	* Repeating until all of the data is copied.
//	A positive direction means the copy goes from system to user
//	space; negative direction means the copy goes from user to system
//	space.
//
//	This routine returns the number of bytes copied.  Note that this
//	may be less than the number requested if there were unmapped pages
//	in the user range.  If this happens, the copy stops at the
//	first unmapped address.
//
//----------------------------------------------------------------------
int MemoryMoveBetweenSpaces (PCB *pcb, unsigned char *system, unsigned char *user, int n, int dir) {
  unsigned char *curUser;         // Holds current physical address representing user-space virtual address
  int		bytesCopied = 0;  // Running counter
  int		bytesToCopy;      // Used to compute number of bytes left in page to be copied
  //uint32 temp1, temp2;            // Temporary variables for calculations

  dbprintf('m', "MemoryMoveBetweenSpaces (%d): Beginning. System: 0x%x, User: 0x%x, N: %d\n", GetCurrentPid(), (uint32)system, (uint32)user, n);

  while (n > 0) {
    // Translate current user page to system address.  If this fails, return
    // the number of bytes copied so far.

    curUser = (unsigned char *)MemoryTranslateUserToSystem (pcb, (uint32)user);

    dbprintf('m', "MemoryMoveBetweenSpaces (%d): While loop. System: 0x%x, User: 0x%x CurUser: 0x%x, N: %d, bytesToCopy: %d, bytesCopied: %d\n", GetCurrentPid(), (uint32)system, (uint32)user, (uint32)curUser, n, bytesToCopy, bytesCopied);

    // If we could not translate address, exit now
    if (curUser == (unsigned char *)0) {
      dbprintf('m', "MemoryMoveBetweenSpaces (%d): Exiting; could not translate address 0x%x.\n", GetCurrentPid(), (uint32)curUser);
      break;
    }

    dbprintf('m', "MemoryMoveBetweenSpaces (%d): Successfully translated address 0x%x\n", GetCurrentPid(), (uint32)curUser);

    // Calculate the number of bytes to copy this time.  If we have more bytes
    // to copy than there are left in the current page, we'll have to just copy to the
    // end of the page and then go through the loop again with the next page.
    // In other words, "bytesToCopy" is the minimum of the bytes left on this page 
    // and the total number of bytes left to copy ("n").

    // First, compute number of bytes left in this page.  This is just
    // the total size of a page minus the current offset part of the physical
    // address.  MEM_PAGESIZE should be the size (in bytes) of 1 page of memory.
    // MEM_ADDRESS_OFFSET_MASK should be the bit mask required to get just the
    // "offset" portion of an address.

    //temp1 = (uint32)MEM_PAGESIZE;
    //temp2 = (uint32)((uint32)curUser & MEM_ADDRESS_OFFSET_MASK);
    //bytesToCopy = (int)(temp1 - temp2);
    bytesToCopy = MEM_PAGESIZE - ((uint32)curUser & (MEM_ADDRESS_OFFSET_MASK));
    //bytesToCopy = (int)((uint32)MEM_PAGESIZE - (uint32)((uint32)curUser & (MEM_ADDRESS_OFFSET_MASK)));

    //dbprintf('m', "MemoryMoveBetweenSpaces (%d): Calculated bytes left in page as %d - 0x%x (or %d) = %d. Bytestocopy: %d\n", GetCurrentPid(), temp1, temp2, temp2, temp1 - temp2, bytesToCopy);
    //dbprintf('m', "MEM_PAGESIZE = 0x%x, or %d\n", MEM_PAGESIZE, MEM_PAGESIZE);
    //dbprintf('m', "curUser = 0x%x, or %d. *curUser = 0x%x, or %d.\n", curUser, curUser, *curUser, *curUser);
    //dbprintf('m', "MEM_ADDRESS_OFFSET_MASK = 0x%x\n", MEM_ADDRESS_OFFSET_MASK);
    //dbprintf('m', "curUser & mask = 0x%x, or %d\n", ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK), ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK));
    //dbprintf('m', "MEM_PAGESIZE - ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK) = 0x%x, or %d\n", MEM_PAGESIZE - ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK), MEM_PAGESIZE - ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK));
    //dbprintf('m', "bytesToCopy = 0x%x, or %d\n", bytesToCopy, bytesToCopy);
    dbprintf('m', "MemorymoveBetweenSpaces (%d): curUser: 0x%x, Mask: 0x%x\n", GetCurrentPid(), (uint32)curUser, MEM_ADDRESS_OFFSET_MASK);
    dbprintf('m', "MemoryMoveBetweenSpaces (%d): Successfully assigned bytesToCopy to %d - 0x%x (or %d) = 0x%x (or %d). BytesToCopy = %d, 0x%x\n", GetCurrentPid(), MEM_PAGESIZE, ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK), ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK), MEM_PAGESIZE - ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK), MEM_PAGESIZE - ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK), bytesToCopy, bytesToCopy);
    
    // Now find minimum of bytes in this page vs. total bytes left to copy
    if (bytesToCopy > n) {
      bytesToCopy = n;
    }

    // Perform the copy.
    if (dir >= 0) {
      bcopy (system, curUser, bytesToCopy);
    } else {
      bcopy (curUser, system, bytesToCopy);
    }

    // Keep track of bytes copied and adjust addresses appropriately.
    n -= bytesToCopy;           // Total number of bytes left to copy
    bytesCopied += bytesToCopy; // Total number of bytes copied thus far
    system += bytesToCopy;      // Current address in system space to copy next bytes from/into
    user += bytesToCopy;        // Current virtual address in user space to copy next bytes from/into
  }
  dbprintf('m', "MemoryMoveBetweenSpaces (%d): Finished. Total bytes copied: %d\n", GetCurrentPid(), bytesCopied);
  return (bytesCopied);
}

//----------------------------------------------------------------------
//
//	These two routines copy data between user and system spaces.
//	They call a common routine to do the copying; the only difference
//	between the calls is the actual call to do the copying.  Everything
//	else is identical.
//
//----------------------------------------------------------------------
int MemoryCopySystemToUser (PCB *pcb, unsigned char *from,unsigned char *to, int n) {
  return (MemoryMoveBetweenSpaces (pcb, from, to, n, 1));
}

int MemoryCopyUserToSystem (PCB *pcb, unsigned char *from,unsigned char *to, int n) {
  return (MemoryMoveBetweenSpaces (pcb, to, from, n, -1));
}

//---------------------------------------------------------------------
// MemoryPageFaultHandler is called in traps.c whenever a page fault 
// (better known as a "seg fault" occurs.  If the address that was
// being accessed is on the stack, we need to allocate a new page 
// for the stack.  If it is not on the stack, then this is a legitimate
// seg fault and we should kill the process.  Returns MEM_SUCCESS
// on success, and kills the current process on failure.  Note that
// fault_address is the beginning of the page of the virtual address that 
// caused the page fault, i.e. it is the vaddr with the offset zero-ed
// out.
//
// Note: The existing code is incomplete and only for reference. 
// Feel free to edit.
//---------------------------------------------------------------------
int MemoryPageFaultHandler(PCB *pcb) {
  uint32 fault_vaddr = pcb->currentSavedFrame[PROCESS_STACK_FAULT];
  uint32 user_sp    = pcb->currentSavedFrame[PROCESS_STACK_USER_STACKPOINTER];
  /* compute page numbers (4KB pages -> shift 12) */
  uint32 fault_page = fault_vaddr >> 12;
  uint32 sp_page = (user_sp - 8) >> 12; /* per spec: legal if fault >= (sp - 8) */
  int newpage, heapBlockIndex;
  uint32 heapBaseVaddr, heapBlockOffset;
  int VALID_HEAP_ACCESS = 0;

  dbprintf('m', "MemoryPageFaultHandler (%d): fault_vaddr=0x%x user_sp=0x%x\n",
           GetCurrentPid(), fault_vaddr, user_sp);

  /* basic bounds check for pagetable index */
  if (fault_page >= MEM_NUM_PAGE_TABLE_ENTRIES) {
    dbprintf('m', "MemoryPageFaultHandler (%d): fault_page %d out of range\n", GetCurrentPid(), fault_page);
    ProcessKill();
    return MEM_FAIL;
  }

  if (pcb->pagetable[fault_page] & MEM_PTE_VALID) {
      dbprintf('m', "MemoryPageFaultHandler (%d): PTE %d already valid\n", GetCurrentPid(), fault_page);
      ProcessKill();
      return MEM_FAIL;
  }

  //PART 4: If vaddr corresponds to a valid heap buddy block, then allocate a new page on the PT for it.
  heapBaseVaddr = pcb->heapPTEPageNum << MEM_L1FIELD_FIRST_BITNUM;
  heapBlockOffset = fault_vaddr - heapBaseVaddr;
  heapBlockIndex = heapBlockOffset / 32;
  if (!(pcb->heapBuddyMap[heapBlockIndex] & MEM_HEAP_BUDDY_MAP_AVAIL)) {
    //This is a valid heap access; a new page needs to be allocated for it
    VALID_HEAP_ACCESS = 1;
    dbprintf('m', "MemoryPageFaultHandler (%d): valid heap access at block index %d (vaddr=0x%x)\n", GetCurrentPid(), heapBlockIndex, fault_vaddr);
  }

  if (fault_page < sp_page && VALID_HEAP_ACCESS == 0) {
    dbprintf('m', "MemoryPageFaultHandler (%d): Segmentation Fault. fault_vaddr=0x%x user_sp=0x%x\n", GetCurrentPid(), fault_vaddr, user_sp);
    ProcessKill();
    return MEM_FAIL;
  }

    dbprintf('m', "MemoryPageFaultHandler (%d): growing stack, alloc page %d\n", GetCurrentPid(), fault_page);
    
    newpage = MemoryAllocPage();
    if (newpage < 0) {
      dbprintf('m', "MemoryPageFaultHandler (%d): out of physical pages\n", GetCurrentPid());
      ProcessKill();
      return MEM_FAIL;
    }
    
    pcb->pagetable[fault_page] = (newpage << 12) | MEM_PTE_VALID;
    pcb->npages++;
    dbprintf('m', "MemoryPageFaultHandler (%d): allocated phys page %d -> PTE %d\n", GetCurrentPid(), newpage, fault_page);
    return MEM_SUCCESS;
}


//---------------------------------------------------------------------
// You may need to implement the following functions and access them from process.c
// Feel free to edit/remove them
//---------------------------------------------------------------------

// Returns the page number allocated or -1 if full
int MemoryAllocPage(void) {
  //USEABLE IF 0, IN USE IF 1
  int i;
  for (i = 0; i < MEM_NUM_PAGE_TABLE_ENTRIES; i++)
  {
    if (!(freemap[i / 32] & (0x1 << (i % 32))))
    {
      freemap[i / 32] |= (0x1 << (i % 32));
      dbprintf('z', "MemoryAllocPage (%d): page %d allocated\n", GetCurrentPid(), i); 
      return i;
    }
  }

  return -1;
}

uint32 MemorySetupPte (uint32 page) {
  return -1;
}

void MemoryFreePage(uint32 page) {
  uint32 mask = (0x1 << (page % 32)) ^ 0xFFFFFFFF;

  if (freemap[page / 32] & mask)
  {
    dbprintf('m', "MemoryFreePage (%d): Freeing page %d.\n", GetCurrentPid(), page);
    freemap[page / 32] ^= mask;
  }
  else
  {
    dbprintf('m', "MemoryFreePage (%d): Tried to free page %d, which is not in use.\n", GetCurrentPid(), page);
  }
}

void *malloc(PCB *currentPCB, int memsize) {
  uint32 blockVaddr;
  uint32 blockPaddr;
  uint32 heapBaseVaddr = currentPCB->heapPTEPageNum << MEM_L1FIELD_FIRST_BITNUM;
  uint32 heapBasePaddr = ((uint32)currentPCB->pagetable[currentPCB->heapPTEPageNum]) & 0xFFFFF000;
  uint32 blockOffset;
  int order, blockIndex, splittingOrder, i;
  int allocationCompleted = 0;
  int blockToSplitFound = 0;

  dbprintf('m', "malloc: Requested memory size %d (0x%x) bytes.\n", memsize, memsize);

  //Fail if memsize is less than or equal to 0 or greater than heap size
  if (memsize <= 0 || memsize > (MEM_HEAP_NUM_BLOCKS*32)) {
    dbprintf('m', "malloc: Requested memory size %d is invalid.\n", memsize);
    return NULL;
  }
  //If requested space is not a multiple of 4 bytes, increase it until it is
  while (memsize % 4 != 0) {
    memsize++;
  }

  // Find a block of adequate size that isn't in use yet
  // Smallest block size: 4 bytes.
  /*
  Order 7: 4096 bytes
  Order 6: 2048 bytes
  Order 5: 1024 bytes
  Order 4: 512 bytes
  Order 3: 256 bytes
  Order 2: 128 bytes
  Order 1: 64 bytes
  Order 0: 32 bytes
  */
  for (order = 0; order <= MEM_HEAP_MAX_ORDER; order++) {
    if (memsize <= (1 << (order + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0))) { //2^5 = 32
      break;
    }
  }
  dbprintf('m', "malloc: Requested memory size %d bytes fits in order %d block of size %d bytes.\n", memsize, order, (1 << (order + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)));
  //order is now set to the smallest order that can fit memsize
  while(!allocationCompleted)
  {
    for (blockIndex = 0; blockIndex <= MEM_HEAP_NUM_BLOCKS; blockIndex += (1 << order)) {
      //Check if block is in use
      if (currentPCB->heapBuddyMap[blockIndex] == order)
      {
        // Block is = to just order, which means the avail bit is not being used
        // The block is free, so allocate it
        // Need to set all other blocks in this range to indicate they are also part of this allocation
        for(i = blockIndex; i < blockIndex + (1 << order); i++) {
          currentPCB->heapBuddyMap[i] = order | MEM_HEAP_BUDDY_MAP_AVAIL; //Set avail bit to 1
        }
        allocationCompleted = 1;
        printf("Allocated the block: order = %d, addr = %x, requested mem size = %d, block size = %d\n", order, blockIndex * 32, memsize, (1 << (order + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)));
        break;
      }
    }
    if(allocationCompleted) { break; }
    dbprintf('m', "malloc: No free block of order %d found, attempting to split larger block.\n", order);
    // Was not able to allocate at this order, need to find a block of next higher order to split in half
    splittingOrder = order;
    while(!blockToSplitFound) {
      splittingOrder++;
      if (splittingOrder > MEM_HEAP_MAX_ORDER) {
          // No blocks available to split
          dbprintf('m', "malloc: Could not find a free block of size %d bytes.\n", memsize);
          return NULL;
      }
      dbprintf('m', "malloc: Looking for block of order %d to split.\n", splittingOrder);
      for (blockIndex = 0; blockIndex <= MEM_HEAP_NUM_BLOCKS; blockIndex += (1 << splittingOrder)) {
        if (currentPCB->heapBuddyMap[blockIndex] == (splittingOrder)) {
          // Found a block to split
          dbprintf('m', "malloc: Found block of order %d at index %d to split.\n", splittingOrder, blockIndex);
          for(i = blockIndex; i < blockIndex + (1 << splittingOrder); i++) {
            currentPCB->heapBuddyMap[i] = splittingOrder-1;
          }
          blockToSplitFound = 1;
          printf("Created a right child node (order = %d, addr = %x, size = %d) of parent (order = %d, addr = %x, size = %d)\n", splittingOrder-1,  blockOffset-4 + (1 << (splittingOrder-1 + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)), (1 << (splittingOrder-1 + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)), splittingOrder, blockOffset-4, (1 << (splittingOrder + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)));
          printf("Created a left child node (order = %d, addr = %x, size = %d) of parent (order = %d, addr = %x, size = %d)\n", splittingOrder-1, blockOffset-4, (1 << (splittingOrder-1 + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)), splittingOrder, blockOffset-4, (1 << (splittingOrder + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)));
          break;
        }
      }
      //dbprintf('m', "malloc: block split search for loop done\n");
      if (blockToSplitFound && splittingOrder != order+1) {
          // Need to keep splitting until we reach desired order
          dbprintf('m', "malloc: Split block to order %d, need to keep splitting to reach order %d.\n", splittingOrder-1, order);
          blockToSplitFound = 0;
          splittingOrder = order;
      }
    }
  }
  if (allocationCompleted)
  {
    blockOffset = blockIndex * 32; //32 bytes per block
    //printf("just set blockOffset to %d (32 * %d)\n", blockOffset, blockIndex);
  }
  else
  {
    dbprintf('m', "malloc: Could not find a free block of size %d bytes.\n", memsize);
    return NULL;
  }
  
  blockVaddr = heapBaseVaddr + blockOffset;
  blockPaddr = heapBasePaddr + blockOffset;
  dbprintf('m', "malloc: Created a heap block of size %d bytes: virtual address 0x%x, physical address 0x%x.\n", memsize, blockVaddr, blockPaddr);
  printHeap(currentPCB);
  return (uint32*) blockVaddr;
}

int mfree(PCB *currentPCB, void *ptr) {
  int memsize;
  uint32 vaddr;
  uint32 paddr;
  int order, i;
  uint32 heapBaseVaddr = currentPCB->heapPTEPageNum << MEM_L1FIELD_FIRST_BITNUM;
  uint32 heapBasePaddr = ((uint32)currentPCB->pagetable[currentPCB->heapPTEPageNum]) & 0xFFFFF000;
  int blockOffset, blockIndex, changeMade;

  dbprintf('m', "Entering mfree.\n");
  //printHeap(currentPCB);

  //dbprintf('m', "mfree: Freeing heap block at virtual address 0x%x.\n", (uint32)ptr);
  vaddr = (uint32)ptr;
  paddr = MemoryTranslateUserToSystem(currentPCB, vaddr);
  //dbprintf('m', "mfree: Freeing heap block at vaddr 0x%x, paddr 0x%x.\n", vaddr, paddr);

  blockOffset = vaddr - heapBaseVaddr;
  blockIndex = blockOffset / 32; //32 bytes per block
  //dbprintf('m', "mfree: Block offset is %d bytes, block index is %d.\n", blockOffset, blockIndex);

  //check to make sure that block is in use
  //dbprintf('m', "mfree: Buddy map entry at index %d is 0x%x.\n", blockIndex, currentPCB->heapBuddyMap[blockIndex]);
  if (!(currentPCB->heapBuddyMap[blockIndex] & MEM_HEAP_BUDDY_MAP_AVAIL)) {
    dbprintf('m', "mfree: Error - block at index %d is not currently allocated.\n", blockIndex);
    return -1;
  }

  order = currentPCB->heapBuddyMap[blockIndex] & ~MEM_HEAP_BUDDY_MAP_AVAIL;
  //dbprintf('m', "mfree: Block at index %d is of order %d.\n", blockIndex, order);

  dbprintf('m', "mfree: Freeing heap block at vaddr 0x%x, paddr 0x%x, order %d. %d bytes at offset %d.\n", vaddr, paddr, order, (1 << (order + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)), blockOffset);
  //order = currentPCB->heapBuddyMap[((uint32)ptr - (currentPCB->heapPTEPageNum << MEM_L1FIELD_FIRST_BITNUM)) / 32] & ~MEM_HEAP_BUDDY_MAP_AVAIL;

  //Mark all blocks in this allocation as free
  for(i = blockIndex; i < blockIndex + (1 << order); i++) {
    currentPCB->heapBuddyMap[i] = order; //Set avail bit to 0
  }
  printf("Freed the block: order = %d, addr = %x, size = %d\n", order, blockOffset, (1 << (order + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)));

  //Now comes the hard part. Need to free neighboring blocks if they are also free, and continue recursively
  changeMade = 1;
  while(changeMade != 0) {
    changeMade = 0;
    if (blockIndex % (1 << (order + 1)) == 0) {
      //This is the first block in the pair
      if (currentPCB->heapBuddyMap[blockIndex + (1 << order)] == order) {
        //The buddy block is also free, so merge
        dbprintf('m', "mfree: Merging block at index %d with buddy at index %d to form order %d block.\n", blockIndex, blockIndex + (1 << order), order + 1);
        printf("Coalesced buddy nodes (order = %d, addr = %x, size = %d) & (order = %d, addr = %x, size = %d)\n", order, blockIndex * 32, (1 << (order + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)), order, (blockIndex + (1 << order)) * 32, (1 << (order + MEM_HEAP_BIT_SHIFT_FOR_ORDER_0)));
        printf("into the parent node (order = %d, addr = %x, size = %d)\n", order + 1, blockIndex * 32, (1 << (order + 6)));
        for(i = blockIndex; i < blockIndex + (1 << (order + 1)); i++) {
          currentPCB->heapBuddyMap[i] = order + 1; //Set avail bit to 0
        }
        dbprintf('m', "mfree: Merged block is now at index %d.\n", blockIndex);
        //blockIndex remains the same
        changeMade = 1;
      }
      //Else, buddy block is not free, so done
    }
    else {
      //This is the second block in the pair
      if (currentPCB->heapBuddyMap[blockIndex - (1 << order)] == order) {
        //The buddy block is also free, so merge
        dbprintf('m', "mfree: Merging block at index %d with buddy at index %d to form order %d block.\n", blockIndex - (1 << order), blockIndex, order + 1);
        for(i = blockIndex - (1 << order); i < blockIndex + (1 << order); i++) {
          currentPCB->heapBuddyMap[i] = order + 1; //Set avail bit to 0
        }
        //Update blockIndex to the start of the new merged block
        blockIndex = blockIndex - (1 << order);
        dbprintf('m', "mfree: Merged block is now at index %d.\n", blockIndex);
        changeMade = 1;
      }
    }
    if (changeMade)
    {
      //keep searching for more merges at next order
      order++;
      dbprintf('m', "mfree: Continuing to look for merges at order %d.\n", order);
    }
  }
  dbprintf('m', "mfree: Finished merging heap blocks.\n");
  printHeap(currentPCB);
  
  //dbprintf('m', "Freeing heap block of size %d bytes: virtual address 0x%x, physical address 0x%x.\n", memsize, vaddr, paddr);
  return -1;
}

void printHeap(PCB *currentPCB) {
  int i;

  dbprintf("y", "Heap Buddy Map for process %d:\n", GetCurrentPid());
  for (i = 0; i < MEM_HEAP_NUM_BLOCKS; i++) {
    if (currentPCB->heapBuddyMap[i] & MEM_HEAP_BUDDY_MAP_AVAIL) {
      dbprintf('m', ".");
    }
    else { dbprintf('m', " "); }
    dbprintf('m', "%d ", currentPCB->heapBuddyMap[i] & ~MEM_HEAP_BUDDY_MAP_AVAIL);
    if ((i+1) % 16 == 0) {
      dbprintf('m', "\n");
    }
  }
  dbprintf('m', "\n");
}