#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  Printf("hello_world (%d): Hello world!\n", getpid());
  Printf("hello_world (%d): malloc-ing 10 bytes\n", getpid());
  malloc(10);
  Printf("hello_world (%d): malloc-ing 100 bytes\n", getpid());
  malloc(100);
  Printf("hello_world (%d): malloc-ing 1000 bytes\n", getpid());
  malloc(1000);
}
