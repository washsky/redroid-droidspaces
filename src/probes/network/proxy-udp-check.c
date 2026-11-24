#define _GNU_SOURCE
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
int main(int n,char **v){
 if(n<2||n>10)return 2;
 int fd=socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC,0);if(fd<0){perror("socket");return 3;}
 struct timeval tv={.tv_sec=6};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
 unsigned char query[]={0x71,0,1,0,0,1,0,0,0,0,0,0,7,'e','x','a','m','p','l','e',3,'c','o','m',0,0,1,0,1};
 struct sockaddr_in targets[9];int seen[9]={0};
 for(int i=1;i<n;i++){
  targets[i-1]=(struct sockaddr_in){.sin_family=AF_INET,.sin_port=htons(53)};if(inet_pton(AF_INET,v[i],&targets[i-1].sin_addr)!=1)return 4;
  query[1]=(unsigned char)i;
  if(sendto(fd,query,sizeof(query),0,(void*)&targets[i-1],sizeof(targets[i-1]))!=(ssize_t)sizeof(query)){perror("sendto");return 5;}
 }
 for(int i=1;i<n;i++){
  unsigned char answer[65536];struct sockaddr_in peer;socklen_t size=sizeof(peer);ssize_t got=recvfrom(fd,answer,sizeof(answer),0,(void*)&peer,&size);
  if(got<12){perror("recvfrom");return 6;}
  int id=answer[1];if(answer[0]!=0x71||id<1||id>=n||seen[id-1]||peer.sin_addr.s_addr!=targets[id-1].sin_addr.s_addr||peer.sin_port!=htons(53)||!(answer[2]&0x80)||(answer[3]&15)){puts("WRONG_RESPONSE");return 7;}
  seen[id-1]=1;char ip[64];inet_ntop(AF_INET,&peer.sin_addr,ip,sizeof(ip));printf("uid=%d id=%d peer=%s:%u bytes=%zd answers=%u\n",getuid(),id,ip,ntohs(peer.sin_port),got,answer[6]*256u+answer[7]);
 }
 close(fd);return 0;
}
