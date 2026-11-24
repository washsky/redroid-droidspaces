#include "proxy-common.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <signal.h>
#include <linux/capability.h>
#include <sys/syscall.h>
#define ORIGINAL_DST 80
static struct px_config cfg;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static unsigned accepted,success,failed,active;
static void stats(void){
 char path[96];snprintf(path,sizeof(path),"/dev/arm13-proxy/.stats.%d",getpid());int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0644);if(fd<0)return;
 dprintf(fd,"%lu %d %u %u %u %u\n",cfg.revision,getpid(),accepted,success,failed,active);close(fd);rename(path,PX_STATS);
}
struct buffer {unsigned char data[16384];size_t begin,end;int eof,shutdown;};
static void relay(int a,int b){
 int fd[2]={a,b};struct buffer q[2]={0};int64_t idle=px_now()+600000;
 for(;;){
  for(int i=0;i<2;i++)if(q[i].eof&&q[i].begin==q[i].end&&!q[i].shutdown){shutdown(fd[1-i],SHUT_WR);q[i].shutdown=1;}
  if(q[0].shutdown&&q[1].shutdown)return;
  struct pollfd p[2]={{a,0,0},{b,0,0}};
  for(int i=0;i<2;i++){if(!q[i].eof&&q[i].end<sizeof(q[i].data))p[i].events|=POLLIN;if(q[1-i].begin<q[1-i].end)p[i].events|=POLLOUT;}
  for(int i=0;i<2;i++)if(!p[i].events)p[i].fd=-1;
  int64_t left=idle-px_now();if(left<=0)return;int n=poll(p,2,(int)left);if(n<0&&errno==EINTR)continue;if(n<=0)return;
  for(int i=0;i<2;i++){
   if(p[i].revents&POLLNVAL)return;
   if((p[i].revents&(POLLIN|POLLHUP))&&!q[i].eof&&q[i].end<sizeof(q[i].data)){
    ssize_t got=recv(fd[i],q[i].data+q[i].end,sizeof(q[i].data)-q[i].end,0);
    if(got>0){q[i].end+=(size_t)got;idle=px_now()+600000;}else if(!got)q[i].eof=1;else if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)return;
   }
   struct buffer *out=&q[1-i];
   if((p[i].revents&POLLOUT)&&out->begin<out->end){ssize_t sent=send(fd[i],out->data+out->begin,out->end-out->begin,MSG_NOSIGNAL);if(sent>0){out->begin+=(size_t)sent;idle=px_now()+600000;if(out->begin==out->end)out->begin=out->end=0;}else if(sent<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)return;}
   if(p[i].revents&POLLERR)return;
  }
 }
}
static void *connection(void *arg){
 int client=(int)(intptr_t)arg,upstream=-1;struct sockaddr_in dst;socklen_t size=sizeof(dst);
 int ok=!getsockopt(client,SOL_IP,ORIGINAL_DST,&dst,&size)&&size==sizeof(dst)&&dst.sin_family==AF_INET;
 if(ok&&ntohl(dst.sin_addr.s_addr)==INADDR_LOOPBACK&&ntohs(dst.sin_port)==PX_PORT)ok=0;
 if(ok)upstream=px_tunnel(&cfg,&dst);
 pthread_mutex_lock(&lock);if(upstream>=0)success++;else failed++;stats();pthread_mutex_unlock(&lock);
 if(upstream>=0){relay(client,upstream);close(upstream);}close(client);
 pthread_mutex_lock(&lock);active--;stats();pthread_mutex_unlock(&lock);return NULL;
}
/* UDP TPROXY preserves each original destination via recvmsg; a narrowly
 * privileged reply socket binds that address. DNS uses a fixed upstream and
 * REDIRECT, whose conntrack mapping restores the DNS reply source. */
struct udp_flow {int queue[2],server,dns,reply;struct sockaddr_in peer,dst;};
static struct udp_flow *flows[64];
static void account(int ok){pthread_mutex_lock(&lock);if(ok)success++;else failed++;stats();pthread_mutex_unlock(&lock);}
static int udp_reply_socket(const struct sockaddr_in *src,const struct sockaddr_in *peer){
 int fd=socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0),one=1;if(fd<0)return -1;
 if(setsockopt(fd,SOL_IP,19,&one,sizeof(one))||setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one))||bind(fd,(const void*)src,sizeof(*src))||connect(fd,(const void*)peer,sizeof(*peer))){close(fd);return -1;}return fd;
}
static void *udp_flow_worker(void *arg){
 struct udp_flow *f=arg;int control=-1,up=-1;unsigned char packet[65536];int64_t idle=px_now()+60000;
 if(!f->dns){f->reply=udp_reply_socket(&f->dst,&f->peer);if(f->reply<0){account(0);goto done;}up=px_udp_associate(&cfg,&control);if(up<0){account(0);goto done;}}
 for(;;){
  int64_t left=idle-px_now();if(left<=0)break;
  struct pollfd p[3]={{f->queue[1],POLLIN,0},{up,POLLIN,0},{control,POLLIN,0}};
  int n=poll(p,3,(int)left);if(n<0&&errno==EINTR)continue;if(n<=0)break;
  if(p[2].revents)break;
  if(p[0].revents&POLLIN){
   ssize_t size=recv(f->queue[1],packet+10,sizeof(packet)-10,0);if(size<=0)break;idle=px_now()+60000;
   if(f->dns){
    struct sockaddr_in dst={.sin_family=AF_INET,.sin_port=htons(53)};inet_pton(AF_INET,cfg.dns_host,&dst.sin_addr);
    int fd=px_tunnel(&cfg,&dst),ok=0;int64_t end=px_now()+8000;
    unsigned char length[2]={(unsigned char)((size_t)size>>8),(unsigned char)size};
    if(fd>=0&&!px_io(fd,length,2,1,end)&&!px_io(fd,packet+10,(size_t)size,1,end)&&!px_io(fd,length,2,0,end)){
     size_t got=(size_t)length[0]*256+length[1];
     if(got>=12&&got<=sizeof(packet)-10&&!px_io(fd,packet,got,0,end))
      ok=sendto(f->server,packet,got,MSG_NOSIGNAL,(void*)&f->peer,sizeof(f->peer))==(ssize_t)got;
    }if(fd>=0)close(fd);account(ok);
   }else{
    memset(packet,0,4);packet[3]=1;memcpy(packet+4,&f->dst.sin_addr,4);memcpy(packet+8,&f->dst.sin_port,2);
    if(send(up,packet,(size_t)size+10,MSG_NOSIGNAL)!=size+10){account(0);break;}
   }
  }
  if(p[1].revents&POLLIN){
   ssize_t size=recv(up,packet,sizeof(packet),0);
   if(size>10&&!packet[0]&&!packet[1]&&!packet[2]&&packet[3]==1&&!memcmp(packet+4,&f->dst.sin_addr,4)&&!memcmp(packet+8,&f->dst.sin_port,2)){
    int ok=send(f->reply,packet+10,(size_t)size-10,MSG_NOSIGNAL)==size-10;
    account(ok);idle=px_now()+60000;
   }
  }
 }
done:
 if(f->reply>=0)close(f->reply);
 if(up>=0)close(up);
 if(control>=0)close(control);
 pthread_mutex_lock(&lock);for(unsigned i=0;i<64;i++)if(flows[i]==f)flows[i]=NULL;close(f->queue[0]);close(f->queue[1]);active--;stats();pthread_mutex_unlock(&lock);free(f);return NULL;
}
static int udp_listener(int port){
 int fd=socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0),one=1;
 struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(port),.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
 if(fd<0)return -1;
 if((port==PX_PORT&&setsockopt(fd,SOL_IP,19,&one,sizeof(one)))||setsockopt(fd,SOL_IP,20,&one,sizeof(one))||bind(fd,(void*)&a,sizeof(a))){close(fd);return -1;}return fd;
}
static void udp_receive(int server,int dns,pthread_attr_t *attr){
 unsigned char packet[65536],ancillary[CMSG_SPACE(sizeof(struct sockaddr_in))];struct sockaddr_in peer,dst={0};
 struct iovec iov={packet,sizeof(packet)};
 struct msghdr msg={.msg_name=&peer,.msg_namelen=sizeof(peer),.msg_iov=&iov,.msg_iovlen=1,.msg_control=ancillary,.msg_controllen=sizeof(ancillary)};
 ssize_t n=recvmsg(server,&msg,0);if(n<=0||(msg.msg_flags&(MSG_TRUNC|MSG_CTRUNC)))return;
 for(struct cmsghdr *c=CMSG_FIRSTHDR(&msg);c;c=CMSG_NXTHDR(&msg,c))if(c->cmsg_level==SOL_IP&&c->cmsg_type==20&&c->cmsg_len>=CMSG_LEN(sizeof(dst)))memcpy(&dst,CMSG_DATA(c),sizeof(dst));
 if(dst.sin_family!=AF_INET||peer.sin_family!=AF_INET||(!dns&&dst.sin_addr.s_addr==htonl(INADDR_LOOPBACK)))return;
 pthread_mutex_lock(&lock);
 struct udp_flow *f=NULL;int slot=-1;
 for(unsigned i=0;i<64;i++){if(!flows[i]){slot=(int)i;continue;}if(flows[i]->server==server&&flows[i]->peer.sin_addr.s_addr==peer.sin_addr.s_addr&&flows[i]->peer.sin_port==peer.sin_port&&flows[i]->dst.sin_addr.s_addr==dst.sin_addr.s_addr&&flows[i]->dst.sin_port==dst.sin_port)f=flows[i];}
 if(!f&&slot>=0&&active<128){
  f=calloc(1,sizeof(*f));if(f){f->reply=-1;f->server=server;f->dns=dns;f->peer=peer;f->dst=dst;
   if(socketpair(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0,f->queue)){free(f);f=NULL;}
   else {flows[slot]=f;active++;accepted++;pthread_t thread;
    if(pthread_create(&thread,attr,udp_flow_worker,f)){flows[slot]=NULL;active--;close(f->queue[0]);close(f->queue[1]);free(f);f=NULL;}
   }
  }
 }
 if(!f||send(f->queue[0],packet,(size_t)n,MSG_NOSIGNAL)!=n)failed++;
 stats();pthread_mutex_unlock(&lock);
}

int main(void){
 if(getuid()!=2000||px_read(&cfg)||!cfg.enabled)return 2;
 if(!cfg.udp){struct __user_cap_header_struct h={_LINUX_CAPABILITY_VERSION_3,0};struct __user_cap_data_struct data[2]={{0},{0}};if(syscall(SYS_capset,&h,data))return 8;}
 signal(SIGPIPE,SIG_IGN);prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0);struct rlimit lim={1024,1024};setrlimit(RLIMIT_NOFILE,&lim);
 int server=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0),one=1;if(server<0)return 3;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
 struct sockaddr_in addr={.sin_family=AF_INET,.sin_port=htons(PX_PORT),.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
 if(bind(server,(void*)&addr,sizeof(addr))||listen(server,128))return 4;
 int udp=cfg.udp?udp_listener(PX_PORT):-1,dns=cfg.dns?udp_listener(PX_DNS_PORT):-1;
 if((cfg.udp&&udp<0)||(cfg.dns&&dns<0))return 6;
 stats();
 pthread_attr_t attr;pthread_attr_init(&attr);pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);pthread_attr_setstacksize(&attr,256*1024);
 for(;;){struct pollfd p[3]={{server,POLLIN,0},{udp,POLLIN,0},{dns,POLLIN,0}};int n=poll(p,3,-1);if(n<0&&errno==EINTR)continue;if(n<0)return 7;
  if(p[1].revents&POLLIN)udp_receive(udp,0,&attr);
  if(p[2].revents&POLLIN)udp_receive(dns,1,&attr);
  if(!(p[0].revents&POLLIN))continue;
  int client=accept4(server,NULL,NULL,SOCK_CLOEXEC|SOCK_NONBLOCK);if(client<0){if(errno==EINTR)continue;return 5;}
  pthread_mutex_lock(&lock);if(active>=128){pthread_mutex_unlock(&lock);close(client);continue;}active++;accepted++;stats();pthread_mutex_unlock(&lock);
  pthread_t thread;if(pthread_create(&thread,&attr,connection,(void*)(intptr_t)client)){close(client);pthread_mutex_lock(&lock);active--;failed++;stats();pthread_mutex_unlock(&lock);}
 }
}
