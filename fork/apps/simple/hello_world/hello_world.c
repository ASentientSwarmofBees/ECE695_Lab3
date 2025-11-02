#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  int child_pid;

  Printf("hello_world (%d): this is the main program.\n", getpid());
  child_pid = fork();
  if (child_pid != 0)
  {
    printf("hello_world (%d): this is the parent process. child_id = %d.\n", getpid(), child_pid);
  }
  else
  {
    printf("hello_world (%d): this is the child process. child_id = %d.\n", getpid(), child_pid);
  }
}
