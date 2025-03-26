#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char input[512];
  char buf;
  int i = 0;

  while(read(0,&buf,1)){
    if(buf=='\n'){
      input[i] = '\0';
      i = 0;

      if(fork() == 0){
        char *argvv[2];
        argvv[0] = argv[1];
        argvv[1] = input;
        argvv[2] = 0;
        printf("%s\n", input);
        printf("%s\n", argv[1]);
        exec(argv[1], argvv);
      } else {
        wait((void*)0);
      }
    } else {
      input[i++] = buf;
    }
  }

  exit(0);
}