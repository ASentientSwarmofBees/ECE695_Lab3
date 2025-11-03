#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  int child_pid;

  Printf("hello_world (%d): this is the main program.\n", (int) getpid());
  child_pid = fork();
  Printf("hello_world (%d): forked a child process with id %d.\n", (int) getpid(), child_pid);
  if (getpid() != 0)
  {
    Printf("hello_world (%d): this is the parent process. child_id = %d.\n", (int) getpid(), child_pid);
  }
  else
  {
    Printf("hello_world (%d): this is the child process.\n", (int) getpid());
  }
}
