#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <poll.h>
#include <time.h>
#include <arpa/inet.h>
#define PX_CONFIG "/data/misc/redroid-netbroker/proxy"
#define PX_STATS "/dev/arm13-proxy/stats"
#define PX_PORT 10809
#define PX_DNS_PORT 10810
struct px_config {
 unsigned long revision;
 int enabled,port,udp,dns,auth;
 char protocol[16],host[64],dns_host[64],username[256],password[256];
};
static inline void px_defaults(struct px_config *c){memset(c,0,sizeof(*c));c->port=10808;strcpy(c->protocol,"socks5");strcpy(c->host,"192.168.2.95");strcpy(c->dns_host,"1.1.1.1");}
static inline int px_ipv4(const char *s){struct in_addr a;return inet_pton(AF_INET,s,&a)==1&&a.s_addr!=0;}
static inline int px_valid(const struct px_config *c){
 return (c->enabled==0||c->enabled==1)&&(c->udp==0||c->udp==1)&&(c->dns==0||c->dns==1)&&(c->auth==0||c->auth==1)&&c->port>0&&c->port<65536&&
 (!strcmp(c->protocol,"http")||!strcmp(c->protocol,"socks5"))&&(!c->udp||!strcmp(c->protocol,"socks5"))&&px_ipv4(c->host)&&px_ipv4(c->dns_host)&&
 (!c->auth||(c->username[0]&&c->password[0]&&!strchr(c->username,':')));
}
static inline int px_unhex(const char *s,char *out,size_t cap){
 if(!strcmp(s,"-")){out[0]=0;return 0;}size_t n=strlen(s);if(n%2||n/2>=cap)return -1;
 for(size_t i=0;i<n;i+=2){unsigned a,b;char x=s[i],y=s[i+1];
  if(x>='0'&&x<='9')a=x-'0';else if(x>='a'&&x<='f')a=x-'a'+10;else return -1;
  if(y>='0'&&y<='9')b=y-'0';else if(y>='a'&&y<='f')b=y-'a'+10;else return -1;
  unsigned v=a*16+b;if(v<32||v==127)return -1;out[i/2]=(char)v;
 }out[n/2]=0;return 0;
}
static inline void px_hex(FILE *f,const char *s){if(!*s){fputs("-",f);return;}for(;*s;s++)fprintf(f,"%02x",(unsigned char)*s);}
static inline int px_parse(FILE *f,struct px_config *c,int disk){
 px_defaults(c);char magic[16],user[512],pass[512],extra;
 if(disk){
  if(fscanf(f,"%15s",magic)!=1)return -1;
  if(!strcmp(magic,"R13PROXY1")){
   return fscanf(f,"%lu%d%15s%63s%d",&c->revision,&c->enabled,c->protocol,c->host,&c->port)==5&&px_valid(c)&&fscanf(f," %c",&extra)!=1?0:-1;
  }if(strcmp(magic,"R13PROXY2"))return -1;
 }
 if(fscanf(f,"%lu%d%15s%63s%d%d%d%63s%d%511s%511s",&c->revision,&c->enabled,c->protocol,c->host,&c->port,&c->udp,&c->dns,c->dns_host,&c->auth,user,pass)!=11||
    px_unhex(user,c->username,sizeof(c->username))||px_unhex(pass,c->password,sizeof(c->password))||!px_valid(c)||fscanf(f," %c",&extra)==1)return -1;
 return 0;
}
static inline int px_read(struct px_config *c){
 px_defaults(c);int fd=open(PX_CONFIG,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return errno==ENOENT?0:-1;
 struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid||(st.st_mode&022)){close(fd);errno=EPERM;return -1;}
 FILE *f=fdopen(fd,"r");if(!f){close(fd);return -1;}int result=px_parse(f,c,1);fclose(f);
 if(!result&&c->auth&&(st.st_mode&0007)){errno=EPERM;return -1;}if(result)errno=EINVAL;return result;
}
static inline int64_t px_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static inline int px_wait(int fd,short events,int64_t deadline){
 for(;;){int64_t left=deadline-px_now();if(left<=0){errno=ETIMEDOUT;return -1;}struct pollfd p={fd,events,0};int n=poll(&p,1,(int)left);if(n>0)return 0;if(n<0&&errno==EINTR)continue;if(!n)errno=ETIMEDOUT;return -1;}
}
static inline int px_io(int fd,void *buf,size_t size,int writing,int64_t end){
 unsigned char *p=buf;while(size){ssize_t n=writing?send(fd,p,size,MSG_NOSIGNAL):recv(fd,p,size,0);if(n>0){p+=n;size-=n;continue;}if(n<0&&errno==EINTR)continue;if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK)){if(!px_wait(fd,writing?POLLOUT:POLLIN,end))continue;return -1;}if(!n)errno=ECONNRESET;return -1;}return 0;
}
static inline int px_connect(const struct px_config *c,int64_t end){
 int fd=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);if(fd<0)return -1;
 struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(c->port)};inet_pton(AF_INET,c->host,&a.sin_addr);
 if(connect(fd,(void*)&a,sizeof(a))){if(errno!=EINPROGRESS||px_wait(fd,POLLOUT,end)){close(fd);return -1;}int error=0;socklen_t size=sizeof(error);if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size)||error){close(fd);if(error)errno=error;return -1;}}
 return fd;
}
static inline int px_socks_auth(int fd,const struct px_config *c,int64_t end){
 unsigned char hello[]={5,1,c->auth?2:0},answer[2];
 if(px_io(fd,hello,3,1,end)||px_io(fd,answer,2,0,end))return -1;
 if(answer[0]!=5||answer[1]!=hello[2]){errno=EACCES;return -1;}
 if(c->auth){
  size_t u=strlen(c->username),p=strlen(c->password);unsigned char request[513]={1,(unsigned char)u};
  memcpy(request+2,c->username,u);request[2+u]=(unsigned char)p;memcpy(request+3+u,c->password,p);
  if(px_io(fd,request,3+u+p,1,end)||px_io(fd,answer,2,0,end))return -1;
  if(answer[0]!=1||answer[1]){errno=EACCES;return -1;}
 }return 0;
}
static inline int px_socks_command(int fd,int command,const struct sockaddr_in *dst,struct sockaddr_in *bound,int64_t end){
 unsigned char request[10]={5,(unsigned char)command,0,1},answer[4],tail[258];
 memcpy(request+4,&dst->sin_addr,4);memcpy(request+8,&dst->sin_port,2);
 if(px_io(fd,request,10,1,end)||px_io(fd,answer,4,0,end))return -1;
 if(answer[0]!=5||answer[1]||answer[2]){errno=ECONNREFUSED;return -1;}
 size_t n;if(answer[3]==1)n=6;else if(answer[3]==4)n=18;else if(answer[3]==3){unsigned char len;if(px_io(fd,&len,1,0,end))return -1;n=(size_t)len+2;}else{errno=EPROTO;return -1;}
 if(px_io(fd,tail,n,0,end))return -1;
 if(bound){if(answer[3]!=1){errno=EAFNOSUPPORT;return -1;}memset(bound,0,sizeof(*bound));bound->sin_family=AF_INET;memcpy(&bound->sin_addr,tail,4);memcpy(&bound->sin_port,tail+4,2);}
 return 0;
}
static inline void px_base64(const unsigned char *s,size_t n,char *out){
 static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";size_t k=0;
 for(size_t i=0;i<n;i+=3){unsigned a=s[i],b=i+1<n?s[i+1]:0,c=i+2<n?s[i+2]:0;out[k++]=t[a>>2];out[k++]=t[((a&3)<<4)|(b>>4)];out[k++]=i+1<n?t[((b&15)<<2)|(c>>6)]:'=';out[k++]=i+2<n?t[c&63]:'=';}out[k]=0;
}
static inline int px_tunnel(const struct px_config *c,const struct sockaddr_in *dst){
 int64_t end=px_now()+8000;int fd=px_connect(c,end);if(fd<0)return -1;
 if(!strcmp(c->protocol,"socks5")){
  if(px_socks_auth(fd,c,end)||px_socks_command(fd,1,dst,NULL,end))goto bad;
 }else{
  char ip[INET_ADDRSTRLEN],request[1200],reply[4097],header[768]="";inet_ntop(AF_INET,&dst->sin_addr,ip,sizeof(ip));
  if(c->auth){char credentials[512],encoded[684];int n=snprintf(credentials,sizeof(credentials),"%s:%s",c->username,c->password);px_base64((void*)credentials,(size_t)n,encoded);snprintf(header,sizeof(header),"Proxy-Authorization: Basic %s\r\n",encoded);}
  int n=snprintf(request,sizeof(request),"CONNECT %s:%u HTTP/1.1\r\nHost: %s:%u\r\n%s\r\n",ip,ntohs(dst->sin_port),ip,ntohs(dst->sin_port),header);
  if(px_io(fd,request,(size_t)n,1,end))goto bad;
  size_t used=0;for(;used<sizeof(reply)-1;){if(px_io(fd,reply+used,1,0,end))goto bad;used++;if(used>=4&&!memcmp(reply+used-4,"\r\n\r\n",4))break;}
  reply[used]=0;int status=0;if(used>=sizeof(reply)-1||sscanf(reply,"HTTP/%*d.%*d %d",&status)!=1||status!=200){errno=ECONNREFUSED;goto bad;}
 }return fd;
bad:{int error=errno;close(fd);errno=error;return -1;}
}
/* UDP associations keep their control connection alive. A nonzero remote bind
 * address must be the configured proxy, avoiding redirect loops or strangers. */
static inline int px_udp_associate(const struct px_config *c,int *control){
 int64_t end=px_now()+8000;int tcp=px_connect(c,end),udp=-1;struct sockaddr_in zero={.sin_family=AF_INET},bound;
 if(tcp<0)return -1;
 if(px_socks_auth(tcp,c,end)||px_socks_command(tcp,3,&zero,&bound,end))goto bad;
 struct in_addr server;inet_pton(AF_INET,c->host,&server);
 if(!bound.sin_addr.s_addr)bound.sin_addr=server;
 if(bound.sin_addr.s_addr!=server.s_addr||!bound.sin_port){errno=EPROTO;goto bad;}
 udp=socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
 if(udp<0||connect(udp,(void*)&bound,sizeof(bound)))goto bad;
 *control=tcp;return udp;
bad:{int error=errno;if(udp>=0)close(udp);close(tcp);errno=error;return -1;}
}
