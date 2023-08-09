#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{
    
    int p2c[2]; // parent to child [0]:read [1]:write
    int c2p[2]; // child to parent [0]:read [1]:write

    /*create pipe*/
    if (pipe(p2c) == -1 || pipe(c2p) == -1) { //create fail
        fprintf(2, "Error: pipe error.\n");
    }
    
    /*create child process*/
    if (fork() == 0){
        char buffer[1];
        read(p2c[0], buffer, 1); //read p2c data into buffer
        close(p2c[0]); 
        fprintf(0, "%d: received ping\n", getpid()); //print child's id
        write(c2p[1], buffer, 1);//write the data into c2p, as a response
    	exit(0);
    }
    
    /*parent process*/
    else {
        char buffer[1];
        buffer[0] = 'a';
        write(p2c[1], buffer, 1); // write an 'a' into p2c
        close(p2c[1]); 
        read(c2p[0], buffer, 1); //read c2p (child's response)
        fprintf(0, "%d: received pong\n", getpid()); //print parent's id
    	exit(0);	
    }
    exit(0);

}

