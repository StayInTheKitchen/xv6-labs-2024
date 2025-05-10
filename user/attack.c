#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)

  char* end = sbrk(PGSIZE * 17);

  // brute force memory check
  // printf("start attack\n");

  // int bytes = 0;
  // int pg = 0;

  // while (bytes < PGSIZE * 30) {
  //   printf("write: %d\n", pg++);
  //   bytes += write(1, end + bytes, PGSIZE);
  //   printf("bytes: %d, bytes left: %d\n", bytes, bytes % PGSIZE);
  // }

  write(2, end + 16 * PGSIZE + 32, 8);
  exit(1);
}
