#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

int main(int argc, char* argv[]) {
  char buf[512]; //buffer
  char* p = buf; //pointer
  char* args[MAXARG]; // parameters
  int i, n;

  if (argc < 2) {
    fprintf(2,"Error: xargs requires 1 or more arguments\nRight Usage: xargs command ...\n");
    exit(1);
  }

  // copy argv into args
  for (i = 1; i < argc; i++)
    args[i - 1] = argv[i];
    
  //for (i = 0; i < argc; i++) 
  //  printf("#%s\n",argv[i]); //output #xargs #grep #hello

  // read
  while ((n = read(0, p, 1)) > 0) {
  
    if(*p != '\n')
      p++; // continue
    else {
      *p = 0; // replace '\n' with '\0'
      args[argc - 1] = buf; // copy buf into args
      args[argc] = 0; // end letter
	
      // Output the contents of args
      /*printf("Args array contents:\n");
      for (int j = 0; args[j] != 0; j++) {
       	printf("args[%d]: %s\n", j, args[j]);
      }*/
    
      // create child process
      if (fork() == 0) {
        exec(args[0], args);
        exit(0);
      }
      // parent process
      else {
        wait(0); // wait for child process to complete
      }
      p = buf; // redirect p
    }
    
  }

  //printf("%s\n",args);
  exit(0); 
}
