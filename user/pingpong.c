#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int pid;
  int fds1[2];  // parent to child pipe
  int fds2[2];  // child to parent pipe

  char buf[1];

  pipe(fds1);
  pipe(fds2);

  pid = fork();
  if (pid == 0) {
    // child
    read(fds1[0], buf, sizeof(buf));
    printf("%d: received ping\n", getpid());
    close(fds1[0]);
    write(fds2[1], "A", 1);
    close(fds2[1]);
    exit(0);
  } else {
    // parent
    write(fds1[1], "A", 1);
    close(fds1[1]);
    read(fds2[0], buf, sizeof(buf));
    close(fds2[0]);
    printf("%d: received pong\n", getpid());
  }

  exit(0);
}