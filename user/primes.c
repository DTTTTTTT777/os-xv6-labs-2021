#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void new_proc(int p[2]) { 
    /*new_proc: 从管道中读取一个素数 prime，然后筛选掉能被这个素数整除的数字[埃氏筛]*/
    close(p[1]); //close the write side of p
    
    int prime,n;
    
    if (read(p[0], &prime, sizeof(prime)) != sizeof(prime)) {
        fprintf(2, "Error in read.\n");
        exit(1);
    }
    printf("prime %d\n", prime);
    // if read return not zero and it still need next process
    if (read(p[0], &n, 4) == 4){
    	//printf("beff n=%d\n", n);
    	/*create new pipe*/
        int newp[2];
        pipe(newp);
        /*parent process*/      
        if (fork() != 0) {
            close(newp[0]); //close read port
            if (n % prime) 
            	write(newp[1], &n, 4);
            while (read(p[0], &n, 4) == 4) {
                if (n % prime) 
                    write(newp[1], &n, 4);
            }
            close(p[0]);
            close(newp[1]);
            wait(0);
        }
        /*create new child process*/
        else {
            new_proc(newp);
        }
    }
}

int
main(int argc, char* argv[])
{
    /*create pipe*/
    int p[2];
    pipe(p); 
    
    /*child process*/
    if (fork() == 0) {
        new_proc(p);
    }
    /*parent process*/
    else {
        close(p[0]); //close read port of pipe
        for (int i = 2; i <= 35; ++i) { //write 2~35 into the pipe
            if (write(p[1], &i, sizeof(i)) != sizeof(i)) { //write i into the pipe
                fprintf(2, "failed write %d into the pipe\n", i);
                exit(1);
            }
        }
        close(p[1]); //close write port of pipe
        wait(0); //wait for child process to exit
        exit(0);
    }
    return 0;
}
