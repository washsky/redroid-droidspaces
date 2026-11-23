#define _GNU_SOURCE
#include <sched.h>
#include <sys/mount.h>
#include <unistd.h>
#include <stdio.h>
int main(int c,char**v){if(c<4)return 2;if(unshare(CLONE_NEWNS)){perror("unshare");return 3;}if(mount(0,"/",0,MS_REC|MS_PRIVATE,0)){perror("private");return 4;}if(mount(v[1],"/vendor/lib64/libgralloccore.so",0,MS_BIND,0)){perror("bind");return 5;}execv(v[2],v+2);perror("exec");return 6;}
