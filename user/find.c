#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

char*
fmtname(char* path)
{
    static char buf[DIRSIZ+1];
    int whiteSpaceSize = 0;
    int bufSize = 0;
    // 去除字符串后面的空格并且加上'\0'
    for(char* p = path + strlen(path) - 1; p >= path && *p == ' '; --p) {
        ++whiteSpaceSize;
    }
    bufSize = DIRSIZ - whiteSpaceSize;
    memmove(buf, path, bufSize);
    buf[bufSize] = '\0';
    return buf;
}

void
find(char* path, char* name)
{
    char buf[512], *p;
    int fd;
    
    struct dirent de; // dir descriptor
    struct stat st; // file descriptor

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) == -1) {
        fprintf(2, "find: cannot fstat %s\n", path);
        close(fd);
        return;
    }
    
    switch (st.type) {
        
        case T_FILE: //文件
            fprintf(2, "find: %s not a path value.\n", path);
            close(fd);
            break;
        case T_DIR: //目录
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
                printf("ls: path too long\n");
                break;
            }
            
            strcpy(buf, path); // create full path
            p = buf + strlen(buf);
            *p++ = '/';
            /*read dir infomation for file and dirs*/
            while (read(fd, &de, sizeof(de)) == sizeof de) {
                if (de.inum == 0)
                    continue;
                if (strcmp(".", fmtname(de.name)) == 0 || strcmp("..", fmtname(de.name)) == 0)
                    continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = '\0';
                // stat each of files
                if (stat(buf, &st) == -1) {
                    fprintf(2, "find: cannot stat '%s'\n", buf);
                    continue;
                }
                // printf("===file:'%s'\n", buf);
                if (st.type == T_DEVICE || st.type == T_FILE) {
                    if (strcmp(name, fmtname(de.name)) == 0) {
                        printf("%s\n", buf);
                    }
                }
                else if (st.type == T_DIR) {
                    find(buf, name);
                }
            }
        
    }
}

int
main(int argc, char* argv[])
{
    if (argc != 3) {
        fprintf(2, "Usage: find path file.\n");
        exit(0);
    }
    char* path = argv[1];
    char* name = argv[2];
    find(path, name);
    exit(0);
}
