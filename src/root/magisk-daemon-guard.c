#define _GNU_SOURCE
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
int main(void){
 if(getuid()!=0)return 2;
 if(unshare(CLONE_NEWNS)||mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL)){perror("private mount namespace");return 3;}
 const char *paths[]={"/sys/fs/selinux","/sys/fs/cgroup","/dev/memcg/apps",NULL};
 for(int i=0;paths[i];i++){
  struct stat s;if(stat(paths[i],&s))continue;
  if(mount(paths[i],paths[i],NULL,MS_BIND,NULL)||
     mount(NULL,paths[i],NULL,MS_REMOUNT|MS_BIND|MS_RDONLY|MS_NOSUID|MS_NODEV|MS_NOEXEC,NULL)){
   perror(paths[i]);return 4;
  }
 }
 setenv("LD_PRELOAD","/system/lib64/libmagisk_container_context.so",1);
 char *args[]={"/sbin/magisk","--daemon",NULL};
 execv(args[0],args);perror("exec magisk");return 5;
}
