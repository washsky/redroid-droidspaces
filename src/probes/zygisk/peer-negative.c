#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
int main(void){
 for(int test=0;test<3;test++){
  char path[108];snprintf(path,sizeof(path),"/dev/arm13-peer-negative-%d",getpid());
  int listener=socket(AF_UNIX,SOCK_STREAM,0);if(listener<0)return 2;
  struct sockaddr_un addr={.sun_family=AF_UNIX};strcpy(addr.sun_path,path);
  unlink(path);if(bind(listener,(void*)&addr,sizeof(addr))||listen(listener,1))return 3;
  chmod(path,0666);
  pid_t child=fork();if(child<0)return 4;
  if(!child){
   close(listener);if(test==1&&setuid(2000))_exit(5);
   int fd=socket(AF_UNIX,SOCK_STREAM,0),request=test==2?8:10;
   if(connect(fd,(void*)&addr,sizeof(addr))||write(fd,&request,sizeof(request))!=sizeof(request))_exit(6);
   char response;if(read(fd,&response,1)!=1)_exit(7);close(fd);_exit(0);
  }
  int fd=accept(listener,NULL,NULL);if(fd<0)return 8;
  char context[128]={0};socklen_t size=sizeof(context);
  if(getsockopt(fd,SOL_SOCKET,SO_PEERSEC,context,&size)||strcmp(context,"u:r:droidspacesd:s0"))return 9;
  printf("NEGATIVE_%d=PASS context=%s\n",test,context);
  int request;if(recv(fd,&request,sizeof(request),MSG_WAITALL)!=sizeof(request))return 14;
  if(write(fd,"x",1)!=1)return 10;
  close(fd);close(listener);unlink(path);int status;
  if(waitpid(child,&status,0)!=child||!WIFEXITED(status)||WEXITSTATUS(status)){
   fprintf(stderr,"child test=%d status=%d errno=%d\n",test,status,errno);return 11;
  }
 }
 int fd=open("/proc/self/attr/current",O_WRONLY|O_CLOEXEC);if(fd<0)return 12;
 static const char transition[]="u:r:magisk:s0";
 errno=0;ssize_t result=write(fd,transition,sizeof(transition));int error=errno;close(fd);
 if(result!=-1||error!=EPERM)return 13;
 puts("CONTEXT_TRANSITION_DENIED=PASS");return 0;
}
