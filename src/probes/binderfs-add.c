#define _GNU_SOURCE
#include <linux/android/binderfs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
int main(int argc,char **argv){
 if(argc<3)return 2;int fd=open(argv[1],O_RDWR|O_CLOEXEC);if(fd<0){perror("binder-control");return 3;}
 for(int i=2;i<argc;i++){
  if(strlen(argv[i])>BINDERFS_MAX_NAME||strchr(argv[i],'/'))return 4;
  struct binderfs_device d={0};strcpy(d.name,argv[i]);
  if(ioctl(fd,BINDER_CTL_ADD,&d)){perror(argv[i]);close(fd);return 5;}
  printf("CREATED %s major=%u minor=%u\n",d.name,d.major,d.minor);
 }
 close(fd);return 0;
}
