#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  uint32 *ptr;
  Printf("hello_world (%d): Hello world!\n", getpid());
  Printf("hello_world (%d): malloc-ing 10 bytes\n", getpid());
  ptr = malloc(10);
  Printf("hello_world (%d): malloc returned address 0x%x\n", getpid(), (uint32)ptr);
  Printf("hello_world (%d): malloc-ing 100 bytes\n", getpid());
  ptr = malloc(100);
  Printf("hello_world (%d): malloc returned address 0x%x\n", getpid(), (uint32)ptr);
  Printf("hello_world (%d): malloc-ing 1000 bytes\n", getpid());
  ptr = malloc(1000);
  Printf("hello_world (%d): malloc returned address 0x%x\n", getpid(), (uint32)ptr);
}
