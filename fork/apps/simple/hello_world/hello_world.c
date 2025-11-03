#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  int child_pid;

  Printf("hello_world (%d): this is the main program.\n", (int) getpid());
  child_pid = fork();
  if (child_pid != 0)
  {
    Printf("hello_world (%d): this is the parent process. child_id = %d.\n", (int) getpid(), child_pid);
  }
}
