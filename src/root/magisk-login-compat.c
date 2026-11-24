#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Droidspaces uses Linux's `su -l USER`. Magisk 31 accepts USER before
 * arguments passed to its shell. Always delegate authorization to Magisk. */
int main(int argc,char **argv) {
    if(argc>=3&&(!strcmp(argv[1],"-l")||!strcmp(argv[1],"-"))&&argv[2][0]!='-') {
        char **args=calloc((size_t)argc+1,sizeof(*args));
        if(!args){perror("su arguments");return 1;}
        args[0]="/sbin/su";args[1]=argv[2];args[2]="-l";
        for(int i=3;i<argc;i++)args[i]=argv[i];
        execv(args[0],args);
    } else {
        argv[0]="/sbin/su";
        execv(argv[0],argv);
    }
    int error=errno;perror("Magisk su");return error==ENOENT?127:126;
}
