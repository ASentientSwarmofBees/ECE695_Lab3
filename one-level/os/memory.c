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
  int lastPage = lastosaddress / 4096;
  dbprintf('z', "MemoryModuleInit (%d): marking pages in use in freemap through address %x, page %d\n", GetCurrentPid(), lastosaddress, lastPage); 
  
  for (i = 0; i < 16; i++) //TODO: derive 16
  {
    freemap[i] = 0x00000000;
  }

  for (i = 0; i < lastPage; i++)
  {
    // i / 32 is which freemap entry this falls on
    // i % 32 is which bit in the entry it falls on
    freemap[i / 32] |= 0x1 << (i % 32); //USEABLE IF 0, IN USE IF 1
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

  
  dbprintf('m', "MTUTS error test -2\n");
  page = addr >> 12; /* 4KB pages */
  dbprintf('m', "MTUTS error test -1\n");
  offset = addr & MEM_ADDRESS_OFFSET_MASK;
  dbprintf('m', "MTUTS error test 0\n");

  dbprintf('m', "MTUTS error test 1\n");

  //basic bounds check
  if (page >= 512) { //TODO: derive 512 from constants
    dbprintf('m', "MemoryTranslateUserToSystem: page %d out of range\n", page);
    return 0;
  }

  dbprintf('m', "MTUTS error test 2\n");

  pte = pcb->pagetable[page];
  if (!(pte & MEM_PTE_VALID)) {
    //not mapped
    dbprintf('m', "MemoryTranslateUserToSystem: vpage %d not mapped (pte=0x%x)\n", page, pte);
    return 0;
  }

  dbprintf('m', "MTUTS error test 3\n");

  //physical page number is stored in the high bits of the PTE (we store page<<12)
  physpage = pte >> 12;

  dbprintf('m', "MTUTS error test 4\n");

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

  //dbprintf('m', "MemoryMoveBetweenSpaces (%d): Beginning. System: 0x%x, User: 0x%x, N: %d\n", GetCurrentPid(), system, user, n);
  dbprintf('m', "MMBSS: System: 0x%x\n", system);
  dbprintf('m', "MMBSS: User: 0x%x\n", user);
  dbprintf('m', "MMBSS: N: %d\n", n);
  
  dbprintf('m', "MMBS error test 1\n");

  while (n > 0) {
    // Translate current user page to system address.  If this fails, return
    // the number of bytes copied so far.

    dbprintf('m', "MMBS error test 2\n");
    curUser = (unsigned char *)MemoryTranslateUserToSystem (pcb, (uint32)user);

    dbprintf('m', "MemoryMoveBetweenSpaces (%d): Beginning. System: 0x%x, User: 0x%x CurUser: 0x%x, N: %d, bytesToCopy: %d, bytesCopied: %d\n", GetCurrentPid(), system, user, curUser, n, bytesToCopy, bytesCopied);

    // If we could not translate address, exit now
    if (curUser == (unsigned char *)0) break;

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
    bytesToCopy = MEM_PAGESIZE - ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK);
    
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
  int newpage;

  dbprintf('m', "MemoryPageFaultHandler (%d): fault_vaddr=0x%x user_sp=0x%x\n",
           GetCurrentPid(), fault_vaddr, user_sp);

  /* basic bounds check for pagetable index */
  if (fault_page >= 512) { //Todo: derive
    dbprintf('m', "MemoryPageFaultHandler (%d): fault_page %d out of range\n", GetCurrentPid(), fault_page);
    ProcessKill();
    return MEM_FAIL;
  }

  if (fault_page >= sp_page) {
    dbprintf('m', "MemoryPageFaultHandler (%d): growing stack, alloc page %d\n", GetCurrentPid(), fault_page);
    if (pcb->pagetable[fault_page] & MEM_PTE_VALID) {
      dbprintf('m', "MemoryPageFaultHandler (%d): PTE %d already valid\n", GetCurrentPid(), fault_page);
      return MEM_FAIL;
    }
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
  } else {
    dbprintf('m', "MemoryPageFaultHandler (%d): Segmentation Fault. fault_vaddr=0x%x user_sp=0x%x\n",
             GetCurrentPid(), fault_vaddr, user_sp);
    ProcessKill();
    return MEM_FAIL;
  }
}


//---------------------------------------------------------------------
// You may need to implement the following functions and access them from process.c
// Feel free to edit/remove them
//---------------------------------------------------------------------

// Returns the page number allocated or -1 if full
int MemoryAllocPage(void) {
  //USEABLE IF 0, IN USE IF 1
  int i;
  for (i = 0; i < 512; i++)
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

//TODO implement
void *malloc(int memsize) {

}

//TODO iimplement
int mfree(void *ptr) {
  return -1;
}

