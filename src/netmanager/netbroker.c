#include "common.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/inotify.h>
#include <netinet/in.h>
#include <signal.h>
#include <poll.h>
#define MAGIC 0x5231334eU
struct request{unsigned magic;int domain,type,protocol;};
static volatile sig_atomic_t stopping;
static struct config cfg;
static struct resolved permitted;
static struct stat cs,ps;
static int good;
static int changed(const struct stat*a,const struct stat*b){return a->st_ino!=b->st_ino||a->st_size!=b->st_size||a->st_mtim.tv_sec!=b->st_mtim.tv_sec||a->st_mtim.tv_nsec!=b->st_mtim.tv_nsec;}
static void refresh(int force){
    struct stat a,b;int readable=!stat(CONFIG,&a)&&!stat(PKGLIST,&b);
    if(!force&&readable&&good&&!changed(&a,&cs)&&!changed(&b,&ps))return;
    struct config next;struct resolved resolved;
    if(!readable||read_config(&next)||resolve_packages(&next,&resolved)){
        memset(&cfg,0,sizeof(cfg));memset(&permitted,0,sizeof(permitted));good=0;
        atomic_uids(&cfg,&permitted);return;
    }
    cfg=next;permitted=resolved;cs=a;ps=b;good=1;
    if(atomic_uids(&cfg,&permitted))perror("runtime uid list");
}
static void stop_handler(int n){(void)n;stopping=1;}
static void reply(int c,int error,int fd){
    struct iovec i={&error,sizeof(error)};char control[CMSG_SPACE(sizeof(int))]={0};struct msghdr m={.msg_iov=&i,.msg_iovlen=1};
    if(fd>=0){m.msg_control=control;m.msg_controllen=sizeof(control);struct cmsghdr*x=CMSG_FIRSTHDR(&m);x->cmsg_level=SOL_SOCKET;x->cmsg_type=SCM_RIGHTS;x->cmsg_len=CMSG_LEN(sizeof(int));memcpy(CMSG_DATA(x),&fd,sizeof(fd));}
    sendmsg(c,&m,MSG_NOSIGNAL);
}
int main(void){
    if(getuid()!=2000)return 2;const char*s=getenv("ANDROID_SOCKET_r13_netbroker");if(!s)return 3;
    int server=atoi(s);prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0);struct rlimit lim={256,256};setrlimit(RLIMIT_NOFILE,&lim);
    if(listen(server,64))return 4;
    struct sigaction a={.sa_handler=stop_handler};sigemptyset(&a.sa_mask);sigaction(SIGTERM,&a,NULL);sigaction(SIGINT,&a,NULL);
    int notify=inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
    if(notify>=0){inotify_add_watch(notify,CFGDIR,IN_CLOSE_WRITE|IN_MOVED_TO|IN_DELETE|IN_ATTRIB);inotify_add_watch(notify,"/data/system",IN_CLOSE_WRITE|IN_MOVED_TO|IN_DELETE|IN_ATTRIB);}
    refresh(1);
    while(!stopping){
        struct pollfd fds[2]={{server,POLLIN,0},{notify,POLLIN,0}};
        int n=poll(fds,notify>=0?2:1,5000);if(n<0){if(errno==EINTR)continue;break;}
        if(notify>=0&&(fds[1].revents&POLLIN)){
            char events[8192] __attribute__((aligned(__alignof__(struct inotify_event))));ssize_t bytes;int relevant=0;
            while((bytes=read(notify,events,sizeof(events)))>0){
                for(char*p=events;p<events+bytes;){struct inotify_event*e=(void*)p;
                    if((e->mask&IN_Q_OVERFLOW)||(e->len&&(!strcmp(e->name,"packages")||!strcmp(e->name,"packages.list"))))relevant=1;
                    p+=sizeof(*e)+e->len;
                }
            }if(relevant)refresh(1);
        }
        refresh(0);if(!(fds[0].revents&POLLIN))continue;
        int c=accept4(server,NULL,NULL,SOCK_CLOEXEC);if(c<0)continue;
        struct timeval tv={.tv_sec=1};setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        struct ucred cred;socklen_t len=sizeof(cred);
        refresh(0);
        if(getsockopt(c,SOL_SOCKET,SO_PEERCRED,&cred,&len)||!good||!cfg.enabled||!contains_uid(&permitted,cred.uid)){reply(c,EPERM,-1);close(c);continue;}
        struct request q={0};ssize_t got=recv(c,&q,sizeof(q),MSG_TRUNC);int t=q.type&~(SOCK_CLOEXEC|SOCK_NONBLOCK);
        if(got!=sizeof(q)||q.magic!=MAGIC||(q.domain!=AF_INET&&q.domain!=AF_INET6)||(t!=SOCK_STREAM&&t!=SOCK_DGRAM)||(q.protocol!=0&&q.protocol!=(t==SOCK_STREAM?IPPROTO_TCP:IPPROTO_UDP))){reply(c,EINVAL,-1);close(c);continue;}
        int fd=socket(q.domain,q.type|SOCK_CLOEXEC,q.protocol);reply(c,fd<0?errno:0,fd);if(fd>=0)close(fd);close(c);
    }
    close(server);if(notify>=0)close(notify);return 0;
}
