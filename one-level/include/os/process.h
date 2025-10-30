//
//	process.h
//
//	Definitions for process creation and manipulation.  These include
//	the process control block (PCB) structure as well as information
//	about the stack format for a saved process.
//

// Jason was here.

#ifndef	__process_h__
#define	__process_h__

#include "dlxos.h"
#include "queue.h"
#include "memory_constants.h"

#define PROCESS_FAIL 0
#define PROCESS_SUCCESS 1

#define	PROCESS_MAX_PROCS	32	// Maximum number of active processes

#define	PROCESS_INIT_ISR_SYS	0x140	// Initial status reg value for system processes
#define	PROCESS_INIT_ISR_USER	0x100	// Initial status reg value for user processes

#define	PROCESS_STATUS_FREE	0x1
#define	PROCESS_STATUS_RUNNABLE	0x2
#define	PROCESS_STATUS_WAITING	0x4
#define	PROCESS_STATUS_STARTING	0x8
#define	PROCESS_STATUS_ZOMBIE	0x10
#define	PROCESS_STATUS_MASK	0x3f
#define	PROCESS_TYPE_SYSTEM	0x100
#define	PROCESS_TYPE_USER	0x200

typedef	void (*VoidFunc)();

// Process control block
typedef struct PCB {
  uint32	*currentSavedFrame; // -> current saved frame.  MUST BE 1ST!
  uint32	*sysStackPtr;	// Current system stack pointer.  MUST BE 2ND!
  uint32	sysStackArea;	// System stack area for this process
  unsigned int	flags;
  char		name[80];	// Process name
  /* Put the size of the L1 page table here in pagetable[_] */
  uint32	pagetable[512]; // Statically allocated page table, TODO: should be derived from memory_constants.h?
  int		npages;		// Number of pages allocated to this process
  Link		*l;		// Used for keeping PCB in queues
} PCB;

extern PCB	*currentPCB;

// Offsets of various registers from the stack pointer in the register
// save frame.  Offsets are in WORDS (4 byte chunks)
#define	PROCESS_STACK_IREG	10	// Offset of r0 (grows upwards) (=10=r0)
// NOTE: r0 isn't actually stored!  This is for convenience - r1 is the
// first stored register, and is at location PROCESS_STACK_IREG+1
#define	PROCESS_STACK_FREG	(PROCESS_STACK_IREG+32)	// Offset of f0 (=42=r32)
#define	PROCESS_STACK_IAR	(PROCESS_STACK_FREG+32) // Offset of IAR (=74=r64)
#define	PROCESS_STACK_ISR	(PROCESS_STACK_IAR+1) //(=75=r65)
#define	PROCESS_STACK_CAUSE	(PROCESS_STACK_IAR+2) //(=76=r66)
#define	PROCESS_STACK_FAULT	(PROCESS_STACK_IAR+3) //(=77=r67)
//base address of the level 1 page table
#define	PROCESS_STACK_PTBASE	(PROCESS_STACK_IAR+4) //(=78=r68)
//maximum number of entries in the level 1 page table
#define	PROCESS_STACK_PTSIZE	(PROCESS_STACK_IAR+5) //(=79=r69)
//this register contains two different pieces of information, one in the upper 16 bits, and one in the lower 16 
//bits. The number in the lower 16 bits indicates the bit position of the least significant bit of the level 1 
//page number field in a virtual address. The number in the upper 16 bits indicates the bit position of the least 
//significant bit of the level 2 page table field in a virtual address. As a special case, if the two numbers are 
//the same, then the hardware assumes that there are no level 2 page tables and uses only one-level address 
//translation. In this lab, we will not deal with level 2 page tables
#define	PROCESS_STACK_PTBITS	(PROCESS_STACK_IAR+6) //(=80=r70)
//this is the initial stack pointer for the user process. This should be set to the highest 4-byte-aligned 
//address in the virtual memory space.
#define PROCESS_STACK_USER_STACKPOINTER  (PROCESS_STACK_IREG + 29) // r29 is user stack pointer (=39=r29)
#define	PROCESS_STACK_PREV_FRAME 10	// points to previous interrupt frame
#define	PROCESS_STACK_FRAME_SIZE 85	// interrupt frame is 85 words
#define PROCESS_MAX_NAME_LENGTH  100    // Maximum length of an executable's filename
#define SIZE_ARG_BUFF  	256             // Max number of characters in the
					// command-line arguments
#define MAX_ARGS	10		// Max number of command-line
					// arguments

// Number of jiffies in a single process quantum (i.e. how often ProcessSchedule is called)
#define PROCESS_QUANTUM_JIFFIES  CLOCK_PROCESS_JIFFIES
// Number of jiffies that have to pass before decaying all estcpu's
#define PROCESS_ESTCPU_DECAY_JIFFIES  ( PROCESS_QUANTUM_JIFFIES * 10 )

//---------------------------------------------------------
// Put any #define-d constants that you create here.  Be
// sure to prefix their names with "PROCESS_" so the
// grader knows that they are defined in this file.
//---------------------------------------------------------

//---------------------------------------------------------
// Existing function Prototypes
//---------------------------------------------------------

int ProcessFork (VoidFunc func, uint32 param, char *name, int isUser);
void ProcessSchedule ();
void ContextSwitch(void *, void *, int);
void ProcessSuspend (PCB *);
void ProcessSleep(); // A trap in dlxos.s, not a function
void ProcessWakeup (PCB *);
void ProcessSetResult (PCB *, uint32);
void ProcessUserSleep ();
void ProcessDestroy(PCB *pcb);
extern unsigned GetCurrentPid();
int GetPidFromAddress(PCB *pcb);
void ProcessKill();

//-------------------------------------------------------
// Put any functions prototypes that you define here.
//-------------------------------------------------------



#endif	/* __process_h__ */
