#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  int *ptr1, *ptr2, *ptr3, *ptr4;
  Printf("hello_world (%d): Hello world!\n", getpid());
  Printf("hello_world (%d): malloc-ing 10 bytes\n", getpid());
  ptr1 = malloc(10);
  //Printf("hello_world (%d): malloc returned address 0x%x\n", getpid(), ptr);
  Printf("hello_world (%d): malloc-ing 100 bytes\n", getpid());
  ptr2 = malloc(100);
  //Printf("hello_world (%d): malloc returned address 0x%x\n", getpid(), ptr);
  Printf("hello_world (%d): malloc-ing 1000 bytes\n", getpid());
  ptr3 = malloc(1000);
  //Printf("hello_world (%d): malloc returned address 0x%x\n", getpid(), ptr);
  Printf("hello_world (%d): malloc-ing 10000 bytes\n", getpid());
  ptr4 = malloc(10000);

  Printf("hello_world (%d): trying to write to allocated memory (should cause a page fault!)\n", getpid());
  ptr4[0] = 42; //test writing to allocated memory
  Printf("hello_world (%d): reading back value from allocated memory: %d\n", getpid(), ptr4[0]);

  Printf("hello_world (%d): freeing allocated memory at ptr1\n", getpid());
  mfree(ptr1);
  Printf("hello_world (%d): freeing allocated memory at ptr2\n", getpid());
  mfree(ptr2);
  Printf("hello_world (%d): freeing allocated memory at ptr3\n", getpid());
  mfree(ptr3);
  Printf("hello_world (%d): freeing allocated memory at ptr4\n", getpid());
  mfree(ptr4);
}
