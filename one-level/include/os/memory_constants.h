// Jason was here.

#ifndef	_memory_constants_h_
#define	_memory_constants_h_

//------------------------------------------------
// #define's that you are given:
//------------------------------------------------

// We can read this address in I/O space to figure out how much memory
// is available on the system.
#define	DLX_MEMSIZE_ADDRESS	0xffff0000

// Return values for success and failure of functions
#define MEM_SUCCESS 1
#define MEM_FAIL -1

//--------------------------------------------------------
// Put your constant definitions related to memory here.
// Be sure to prepend any constant names with "MEM_" so 
// that the grader knows they are defined in this file.
//--------------------------------------------------------

// Each page is 4KB (4096 bytes), which requires 12 bits to address inside of.
// The bottom 12 bits of a virtual address is its offset.
// The upper 20 bits of a virtual address will index the page table.

// bit position of the least significant bit of the level 1 page number field in a virtual address.
// Ex: 0000 0000 0000 0000 000_ | 0000 0000 0000
#define MEM_L1FIELD_FIRST_BITNUM 12
// the maximum allowable address in the virtual address space. Note that this is not the 4-byte-aligned address, 
//but rather the actual maximum address (it should end with 0xF).
#define MAX_VIRTUAL_ADDRESS 0xFFFFF
// 2MB in bytes = 2097152. /4 (for uint32) is 524288.
#define MEM_MAX_PHYS_MEM 524288
//If this bit is set in a PTE, it means that the page should be marked readonly. We will ONLY set this bit when 
//implementing fork is required, so be sure to set it to zero for now.
#define MEM_PTE_READONLY 0x4
//This is set by the hardware when a page has been modified. This is only used for caching, and we will not deal 
//with this bit in this lab. 
#define MEM_PTE_DIRTY 0x2
//If this bit is set to 1, then the PTE is considered to contain a valid physical page address.
#define MEM_PTE_VALID 0x1

#define MEM_PAGESIZE 0x1 << MEM_L1FIELD_FIRST_BITNUM
//should be the bit mask required to get just the
    // "offset" portion of an address.
#define MEM_ADDRESS_OFFSET_MASK 0xFFF

// 2MB of physical memory in 4KB pages, which is 512 pages.

/*
Bitwise tricks


    Finding the size of a page (MEM_PAGESIZE): 0x1 << MEM_L1FIELD_FIRST_BITNUM. Note that this is the actual size of the page: to get the maximum possible offset within a page you would need to subtract 1.
    Finding the level 1 pagetable size: (MEM_MAX_VIRTUAL_ADDRESS + 1) >> MEM_L1FIELD_FIRST_BITNUM. Note that this is the size of the level 1 page table: to get the maximum allowable index into the level 1 page table, you would need to subtract 1.
    Finding the page offset mask: (MEM_PAGESIZE - 1)
    Finding a mask to convert from a PTE to a page address: ~(MEM_PTE_READONLY | MEM_PTE_DIRTY | MEM_PTE_VALID)
*/


#endif	// _memory_constants_h_
