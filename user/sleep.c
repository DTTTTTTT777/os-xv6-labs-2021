#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char* argv[]) //command parameters (nuber and value)
{
    if (argc != 2) {
        fprintf(2, "Usage: sleep times\n");
    }
    int time = atoi(*++argv); //string to int
    if (sleep(time) != 0) { //call sleep function
        fprintf(2, "Error in sleep sys_call!\n");
    }
    exit(0); //exit program
}
