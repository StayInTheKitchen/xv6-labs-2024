/* 이 과제를 하면서 모르는 것들이 너무 많다
1. pipe의 내부동작 원리
2. read/write의 내부동작 원리 - block등등... 이런 지식이 없으니 주먹구구로 했다.
3. recursion을 구현하기 위해서는 함수를 이용해야된다. GOTO를 처음에 쓰려고 했는데, 그건 사실 함수로 구현하면 되는 문제임
*/

#include "kernel/types.h"
#include "user/user.h"

void recursion(int p[2]) __attribute__((noreturn));  // 이 함수는 절대 반환되지 않음

void
recursion(int prev_fds[2])
{
  int fds[2];

  close(prev_fds[1]);
  pipe(fds);

  // 이 부분을 parent에서 check하니까 문제가 발생했었음.
  int first;
  if(read(prev_fds[0], &first, sizeof(first)) == 0){  // read의 리턴이 0인거랑 음수인거랑 차이가 뭐지?
    // 여기서 close 안해서 resource 계속 leak 발생
    close(fds[0]);
    close(fds[1]);
    close(prev_fds[0]);
    exit(0);
  }

  if(fork() == 0){
    close(prev_fds[0]);
    close(fds[1]);
    recursion(fds);
  }
  else{
    int number;

    printf("prime %d\n", first);
    close(fds[0]);

    while(read(prev_fds[0], &number, sizeof(number))){  // fd close가 생각보다 빡세네, 헷갈린다
      if(number % first){
        write(fds[1], &number, sizeof(number));
      }
    }
    close(prev_fds[0]);
    close(fds[1]);
    wait((int*)0);
    exit(0);
  }
}

int
main(int argc, char *argv[])
{
  int fds[2];
  int i;

  pipe(fds);

  if(fork() == 0){
    recursion(fds);
  }
  else{
    close(fds[0]);
    for(i=2; i<=280; ++i){
      write(fds[1], &i, sizeof(i)); 
    }
    close(fds[1]);
    wait((int*)0);
  }
  
  exit(0);
}