#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
__attribute__((visibility("default")))
ssize_t write(int fd,const void*buf,size_t n){
 static const char transition[]="u:r:magisk:s0";
 if(n==sizeof(transition)&&memcmp(buf,transition,sizeof(transition))==0){
  char proc[64],path[256];snprintf(proc,sizeof(proc),"/proc/self/fd/%d",fd);
  ssize_t size=readlink(proc,path,sizeof(path)-1);
  if(size>0){path[size]=0;size_t l=strlen(path);const char suffix[]="/attr/current";
   if(l>=sizeof(suffix)-1&&!strcmp(path+l-(sizeof(suffix)-1),suffix)){
    /* Deliberately deny this transition, never claim that it succeeded. */
    errno=EPERM;return -1;
   }
  }
 }
 return syscall(SYS_write,fd,buf,n);
}
