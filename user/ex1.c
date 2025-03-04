#include "kernel/types.h"
#include "user/user.h"

int
main()
{
  char buf[64];

  write(1, "hello world\n", 11);
  while(1){
    int n = read(0, buf, sizeof(buf));
    if(n <= 0)
      break;
    write(1, buf, n);
  }

  exit(0);
}
