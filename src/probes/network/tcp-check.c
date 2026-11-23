#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=3)return 2;
 setbuf(stdout,NULL);printf("UID=%u TARGET=%s:%s ",getuid(),argv[1],argv[2]);
 int s=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0);
 if(s<0){printf("SOCKET_ERROR=%d:%s\n",errno,strerror(errno));return 3;}
 fcntl(s,F_SETFL,O_NONBLOCK);
 struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(atoi(argv[2]))};
 if(inet_pton(AF_INET,argv[1],&a.sin_addr)!=1)return 4;
 int r=connect(s,(void*)&a,sizeof(a)),e=errno;
 if(r&&e!=EINPROGRESS){printf("CONNECT_ERROR=%d:%s\n",e,strerror(e));return 5;}
 struct pollfd f={.fd=s,.events=POLLOUT};r=poll(&f,1,3000);
 if(r<=0){printf("CONNECTED=NO POLL=%d\n",r);return 6;}
 socklen_t n=sizeof(e);if(getsockopt(s,SOL_SOCKET,SO_ERROR,&e,&n)){perror("getsockopt");return 7;}
 printf("CONNECTED=%s ERR=%d\n",e?"NO":"YES",e);close(s);return e?8:0;
}
