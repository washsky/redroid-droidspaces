#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

/* Independent AF_UNIX/SCM_RIGHTS regression; never attaches to Android init. */
int main(void) {
 int ready[2]; if(pipe(ready)) return 1;
 struct sockaddr_un a={.sun_family=AF_UNIX};
 snprintf(a.sun_path+1,sizeof(a.sun_path)-1,"arm13-fd-probe-%d",getpid());
 socklen_t len=offsetof(struct sockaddr_un,sun_path)+1+strlen(a.sun_path+1);
 pid_t child=fork(); if(child<0)return 2;
 if(!child){
  close(ready[0]);int s=socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC,0);
  struct timeval timeout={.tv_sec=3};setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
  if(s<0||bind(s,(void*)&a,len))_exit(3);
  if(write(ready[1],"1",1)!=1)_exit(4);
  char control[CMSG_SPACE(sizeof(int))]={0};struct msghdr m={.msg_control=control,.msg_controllen=sizeof(control)};
  ssize_t n=recvmsg(s,&m,0);struct cmsghdr *c=CMSG_FIRSTHDR(&m);int fd=-1;
  if(n>=0&&c&&c->cmsg_level==SOL_SOCKET&&c->cmsg_type==SCM_RIGHTS)memcpy(&fd,CMSG_DATA(c),sizeof(fd));
  char data[8]={0};ssize_t got=fd>=0?read(fd,data,7):-1;
  _exit(got==7&&!memcmp(data,"ARM13FD",7)?0:5);
 }
 close(ready[1]);char ok;if(read(ready[0],&ok,1)!=1)return 6;
 int s=socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC,0),fd=syscall(SYS_memfd_create,"arm13-fd-probe",1);
 if(s<0||fd<0||write(fd,"ARM13FD",7)!=7||lseek(fd,0,SEEK_SET)<0)return 7;
 char control[CMSG_SPACE(sizeof(int))]={0};struct msghdr m={.msg_name=&a,.msg_namelen=len,.msg_control=control,.msg_controllen=sizeof(control)};
 struct cmsghdr *c=CMSG_FIRSTHDR(&m);c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;c->cmsg_len=CMSG_LEN(sizeof(fd));memcpy(CMSG_DATA(c),&fd,sizeof(fd));
 ssize_t n=sendmsg(s,&m,0);int err=errno,status=0;waitpid(child,&status,0);
 printf("SCM_RIGHTS send=%zd errno=%d child_status=%d\n",n,n<0?err:0,status);
 close(s);close(fd);return n>=0&&WIFEXITED(status)&&WEXITSTATUS(status)==0?0:8;
}
