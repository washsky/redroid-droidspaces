#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAGIC 0x5231334eU
struct request{uint32_t magic;int domain,type,protocol;};
typedef int(*socket_fn)(int,int,int);
static socket_fn real_socket(void){static socket_fn saved;socket_fn f=__atomic_load_n(&saved,__ATOMIC_ACQUIRE);if(!f){f=(socket_fn)dlsym(RTLD_NEXT,"socket");__atomic_store_n(&saved,f,__ATOMIC_RELEASE);}return f;}
static int eligible(void){
    uid_t uid=getuid();if(uid<10000)return 0;
    FILE*f=fopen("/dev/arm13-netbroker/uids","re");if(!f)return 0;
    char line[64];int yes=0;while(fgets(line,sizeof(line),f)){
        if(line[0]<'0'||line[0]>'9')continue;char*end;unsigned long u=strtoul(line,&end,10);
        if((*end=='\n'||*end==0)&&u==uid){yes=1;break;}
    }fclose(f);return yes;
}
__attribute__((visibility("default")))
int socket(int domain,int type,int protocol){
    socket_fn original=real_socket();int base=type&~(SOCK_CLOEXEC|SOCK_NONBLOCK);
    if((base!=SOCK_STREAM&&base!=SOCK_DGRAM)||(protocol!=0&&protocol!=(base==SOCK_STREAM?6:17))||(domain!=AF_INET&&domain!=AF_INET6)||!eligible())
        return original?original(domain,type,protocol):(int)syscall(SYS_socket,domain,type,protocol);
    if(!original){errno=ENOSYS;return -1;}
    int client=original(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0);if(client<0)return -1;
    struct timeval tv={.tv_sec=1};setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
    struct sockaddr_un a={.sun_family=AF_UNIX};strcpy(a.sun_path,"/dev/socket/r13_netbroker");
    if(connect(client,(void*)&a,sizeof(a))){int e=errno;close(client);errno=e;return -1;}
    struct request q={MAGIC,domain,type,protocol};
    if(send(client,&q,sizeof(q),MSG_NOSIGNAL)!=sizeof(q)){int e=errno?errno:EIO;close(client);errno=e;return -1;}
    int error=EIO;struct iovec iov={&error,sizeof(error)};char control[CMSG_SPACE(sizeof(int))]={0};
    struct msghdr m={.msg_iov=&iov,.msg_iovlen=1,.msg_control=control,.msg_controllen=sizeof(control)};
    ssize_t got=recvmsg(client,&m,MSG_CMSG_CLOEXEC);int re=errno;close(client);int fd=-1;
    for(struct cmsghdr*c=CMSG_FIRSTHDR(&m);c;c=CMSG_NXTHDR(&m,c))
        if(c->cmsg_level==SOL_SOCKET&&c->cmsg_type==SCM_RIGHTS&&c->cmsg_len==CMSG_LEN(sizeof(int)))memcpy(&fd,CMSG_DATA(c),sizeof(fd));
    if(got!=sizeof(error)||(m.msg_flags&MSG_CTRUNC)||error||fd<0){if(fd>=0)close(fd);errno=got<0?re:(error?error:EPROTO);return -1;}
    if(!(type&SOCK_CLOEXEC)&&fcntl(fd,F_SETFD,0)<0){int e=errno;close(fd);errno=e;return -1;}return fd;
}
