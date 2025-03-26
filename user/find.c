#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

void
find(char *path, char *target)
{
  char tmp[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
  }

  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(tmp)){
    printf("find: path too long\n");
  }

  strcpy(tmp, path);
  p = tmp + strlen(path);
  *p = '/';
  p++;

  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    // check if end of directory entries
    if(de.inum == 0){
      break;
    }

    if(strcmp(".", de.name) == 0 || strcmp("..", de.name) == 0){
      continue;
    }
    
    memmove(p, de.name, DIRSIZ);
    *(p+DIRSIZ) = 0;

    if(stat(tmp, &st) < 0){
      fprintf(2, "find: cannot stat %s\n", tmp);
      continue;
    }

    switch(st.type){
      case T_DEVICE:
      case T_FILE:
        if(strcmp(de.name, target) == 0){
          printf("%s\n", tmp);
        }
        break;
      
      case T_DIR:
        find(tmp, target);
        break;
    }
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "usage: find [start path(optional)] [target name]...\n");
    exit(1);
  }

  if(argc == 2){
    find(".", argv[1]);
  } else{
    find(argv[1], argv[2]);
  }

  exit(0);
}