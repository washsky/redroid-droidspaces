#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <poll.h>
#include <android/log.h>
#include <stdarg.h>
#include <time.h>

/* Magisk 31.1 RequestCode::ZYGISK. Never adapt SU or administrative requests.
 * The shared host policy does not provide an inner Android zygote domain.
 * Translate ONLY the daemon's peer identity check after verifying a root
 * pre-specialization child of this PID namespace's real init-owned zygote.
 * This is userspace compatibility, not a successful SELinux transition. */
static int read_proc(pid_t pid,const char *name,char *buf,size_t size){
 char path[96];snprintf(path,sizeof(path),"/proc/%d/%s",pid,name);
 int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return -1;
 ssize_t n=read(fd,buf,size-1);close(fd);
 if(n<=0)return -1;buf[n]=0;return (int)n;
}
static int same_file(const char *a,const char *b){
 struct stat x,y;return !stat(a,&x)&&!stat(b,&y)&&x.st_dev==y.st_dev&&x.st_ino==y.st_ino;
}
static int process_identity(pid_t pid,unsigned expected_uid,pid_t *parent,unsigned long long *start){
 char s[2048],*p;unsigned uid[4];
 if(read_proc(pid,"status",s,sizeof(s))<0||(p=strstr(s,"\nUid:"))==NULL||
    sscanf(p,"\nUid:%u%u%u%u",uid,uid+1,uid+2,uid+3)!=4||
    uid[0]!=expected_uid||uid[1]!=expected_uid||uid[2]!=expected_uid||uid[3]!=expected_uid)return 0;
 if(read_proc(pid,"stat",s,sizeof(s))<0||(p=strrchr(s,')'))==NULL)return 0;
 char state;int ppid;unsigned long long field=0;
 if(sscanf(p+1," %c %d",&state,&ppid)!=2)return 0;
 p+=2; /* field 3, state */
 for(int f=3;f<22;f++){p=strchr(p,' ');if(!p)return 0;while(*p==' ')p++;}
 if(sscanf(p,"%llu",&field)!=1)return 0;
 *parent=ppid;*start=field;return 1;
}
static int root_process(pid_t pid,pid_t *parent,unsigned long long *start){return process_identity(pid,0,parent,start);}
static int verified_next_init_socket(void);
static int peek_next_request(int fd,unsigned char request[5]){
 struct pollfd ready={.fd=fd,.events=POLLIN};
 if(poll(&ready,1,100)<=0)return 0;
 for(int i=0;i<100;i++){
  ssize_t n=recv(fd,request,5,MSG_PEEK|MSG_DONTWAIT);
  if(n==5)return 1;
  if(!n||(n<0&&errno!=EAGAIN&&errno!=EINTR))return 0;
  struct timespec delay={.tv_nsec=1000000};nanosleep(&delay,NULL);
 }
 return 0;
}
static int verified_next_init_peer(int fd){
 if(!verified_next_init_socket())return 0;
 struct ucred cred;socklen_t size=sizeof(cred);
 if(syscall(SYS_getsockopt,fd,SOL_SOCKET,SO_PEERCRED,&cred,&size)||cred.pid<=1)return 0;
 unsigned char action;struct pollfd ready={.fd=fd,.events=POLLIN};
 if(poll(&ready,1,100)<=0||recv(fd,&action,1,MSG_PEEK|MSG_DONTWAIT)!=1||action!=0)return 0;
 unsigned char request[5];
 if(!peek_next_request(fd,request)||request[0]!=0)return 0;
 unsigned claimed=(unsigned)request[1]|((unsigned)request[2]<<8)|((unsigned)request[3]<<16)|((unsigned)request[4]<<24);
 if(claimed!=(unsigned)cred.pid)return 0;
 char path[96];snprintf(path,sizeof(path),"/proc/%d/ns/pid",cred.pid);
 if(!same_file(path,"/proc/1/ns/pid"))return 0;
 snprintf(path,sizeof(path),"/proc/%d/exe",cred.pid);
 if(!same_file(path,"/proc/1/exe"))return 0;
 pid_t parent,again;unsigned long long start,after;
 if(!process_identity(cred.pid,cred.uid,&parent,&start)||parent!=1||
    !process_identity(cred.pid,cred.uid,&again,&after)||again!=parent||after!=start)return 0;
 static unsigned logged;
 if(__atomic_fetch_add(&logged,1,__ATOMIC_RELAXED)<8)
  __android_log_print(ANDROID_LOG_INFO,"Arm13NextPeer","verified pre-exec init child pid=%d uid=%d",cred.pid,cred.uid);
 return 1;
}
static int verified_next_zygote_peer(int fd){
 if(!verified_next_init_socket())return 0;
 struct ucred cred;socklen_t size=sizeof(cred);
 if(syscall(SYS_getsockopt,fd,SOL_SOCKET,SO_PEERCRED,&cred,&size)||cred.uid||cred.pid<=1)return 0;
 unsigned char request;struct pollfd ready={.fd=fd,.events=POLLIN};
 if(poll(&ready,1,100)<=0||recv(fd,&request,1,MSG_PEEK|MSG_DONTWAIT)!=1)return 0;
 /* Next 1.5.3: ReadModules, RequestCompanionSocket, ZygiskReport,
  * RequestCleanNs, UnshareLock. Root-management actions are not adapted. */
 if(request!=22&&request!=23&&request!=25&&request!=27&&request!=29)return 0;
 char path[96],cmd[256];pid_t parent,again,grandparent;unsigned long long start,after,parent_start;
 snprintf(path,sizeof(path),"/proc/%d/ns/pid",cred.pid);
 if(!same_file(path,"/proc/1/ns/pid"))return 0;
 snprintf(path,sizeof(path),"/proc/%d/exe",cred.pid);
 if(!same_file(path,"/system/bin/app_process64")||!root_process(cred.pid,&parent,&start))return 0;
 /* Companion requests must come from a root pre-specialization child. */
 if(request==23&&parent==1)return 0;
 pid_t zygote=parent==1?cred.pid:parent;
 if(!root_process(zygote,&grandparent,&parent_start)||grandparent!=1)return 0;
 int cmd_size=read_proc(zygote,"cmdline",cmd,sizeof(cmd));
 if(cmd_size<0)return 0;
 if(strcmp(cmd,"zygote64")){
  /* The initial hook report can precede app_process's argv0 rename. */
  static const char early[]="/system/bin/app_process64\0-Xzygote\0/system/bin\0--zygote\0--start-system-server";
  if(request!=25||parent!=1||cmd_size!=sizeof(early)||memcmp(cmd,early,sizeof(early)))return 0;
 }
 snprintf(path,sizeof(path),"/proc/%d/exe",zygote);
 if(!same_file(path,"/system/bin/app_process64")||
    !root_process(cred.pid,&again,&after)||again!=parent||after!=start)return 0;
 static unsigned logged;
 if(__atomic_fetch_add(&logged,1,__ATOMIC_RELAXED)<12)
  __android_log_print(ANDROID_LOG_INFO,"Arm13NextZygote","verified zygote request=%u pid=%d",request,cred.pid);
 return 1;
}
static int verified_zygisk_peer(int fd){
 struct ucred cred;socklen_t size=sizeof(cred);
 if(syscall(SYS_getsockopt,fd,SOL_SOCKET,SO_PEERCRED,&cred,&size)||cred.uid||cred.pid<=1)return 0;
 int request=-1;struct pollfd ready={.fd=fd,.events=POLLIN};
 if(poll(&ready,1,100)<=0||recv(fd,&request,sizeof(request),MSG_PEEK|MSG_DONTWAIT)!=sizeof(request)||request!=10)return 0;
 char path[96],parent_path[96],cmd[128];
 snprintf(path,sizeof(path),"/proc/%d/ns/pid",cred.pid);
 if(!same_file(path,"/proc/self/ns/pid"))return 0;
 snprintf(path,sizeof(path),"/proc/%d/exe",cred.pid);
 if(!same_file(path,"/system/bin/app_process64"))return 0;
 pid_t parent,grandparent,again;unsigned long long start,parent_start,after;
 if(!root_process(cred.pid,&parent,&start)||parent<=1||
    !root_process(parent,&grandparent,&parent_start)||grandparent!=1)return 0;
 snprintf(parent_path,sizeof(parent_path),"/proc/%d/exe",parent);
 if(!same_file(path,parent_path)||read_proc(parent,"cmdline",cmd,sizeof(cmd))<0||strcmp(cmd,"zygote64"))return 0;
 if(!root_process(cred.pid,&again,&after)||again!=parent||after!=start)return 0;
 static unsigned logged;
 if(__atomic_fetch_add(&logged,1,__ATOMIC_RELAXED)<8)
  __android_log_print(ANDROID_LOG_INFO,"Arm13ZygiskPeer","verified pre-specialization peer pid=%d parent=%d uid=0",cred.pid,parent);
 return 1;
}
__attribute__((visibility("default")))
int getsockopt(int fd,int level,int option,void *value,socklen_t *length){
 socklen_t capacity=length?*length:0;
 int result=syscall(SYS_getsockopt,fd,level,option,value,length);
 if(!result&&level==SOL_SOCKET&&option==SO_PEERSEC&&value&&length&&
    (*length==strlen("u:r:droidspacesd:s0")||*length==sizeof("u:r:droidspacesd:s0"))&&
    !memcmp(value,"u:r:droidspacesd:s0",strlen("u:r:droidspacesd:s0"))){
  const char *identity=NULL;
  if(verified_zygisk_peer(fd))identity="u:r:zygote:s0";
  else if(verified_next_init_peer(fd))identity="u:r:init:s0";
  else if(verified_next_zygote_peer(fd))identity="u:r:zygote:s0";
  if(identity){size_t size=strlen(identity)+1;
   if(capacity<size){*length=size;errno=ERANGE;return -1;}
   memcpy(value,identity,size);*length=size-1;
  }
 }
 return result;
}
/* Zygisk Next 1.5.3 labels its init-injection sender socket as init. In this
 * container the actual receiving init socket belongs to droidspacesd. The
 * kernel checks socket SIDs (not just the current process SID) for SENDTO.
 * Use the verified receiver's real domain; never change the host policy. */
static int verified_next_init_socket(void){
 char current[128],init[128];pid_t parent;unsigned long long start;
 struct stat binary;
 const char *next="/data/adb/modules/zygisksu/bin/zygiskd64";
 if(!root_process(getpid(),&parent,&start)||
    !same_file("/proc/self/ns/pid","/proc/1/ns/pid")||
    !same_file("/proc/self/exe",next)||stat(next,&binary)||
    !S_ISREG(binary.st_mode)||binary.st_uid||(binary.st_mode&022)||
    read_proc(getpid(),"attr/current",current,sizeof(current))<0||
    read_proc(1,"attr/current",init,sizeof(init))<0||
    strcmp(current,"u:r:droidspacesd:s0")||strcmp(init,current))return 0;
 return 1;
}
/* Next's two early injection buffers are unlinked after opening. Keep their
 * file lifecycle, but place them on executable /data rather than noexec /dev.
 * This changes neither /dev mount flags nor any other program's paths. */
static const char *next_exec_path(const char *path){
 const char *replacement=NULL;
 if(!path)return path;
 if(!strcmp(path,"/dev/.zn_payload"))replacement="/data/adb/arm13-next-exec/.zn_payload";
 else if(!strcmp(path,"/dev/.zn_helper"))replacement="/data/adb/arm13-next-exec/.zn_helper";
 if(replacement){
  struct stat dir;
  if(verified_next_init_socket()&&!lstat("/data/adb/arm13-next-exec",&dir)&&
     S_ISDIR(dir.st_mode)&&!dir.st_uid&&!(dir.st_mode&077))return replacement;
 }
 return path;
}
static int next_openat(int dirfd,const char *path,int flags,mode_t mode){
 const char *actual=next_exec_path(path);
 if(actual!=path)flags|=O_NOFOLLOW;
 return syscall(SYS_openat,dirfd,actual,flags,mode);
}
#define MODE_ARG(flags,ap) (((flags)&O_CREAT)||(((flags)&O_TMPFILE)==O_TMPFILE)?va_arg(ap,int):0)
int open(const char *path,int flags,...){
 va_list ap;va_start(ap,flags);mode_t mode=MODE_ARG(flags,ap);va_end(ap);
 return next_openat(AT_FDCWD,path,flags,mode);
}
int open64(const char *path,int flags,...){
 va_list ap;va_start(ap,flags);mode_t mode=MODE_ARG(flags,ap);va_end(ap);
 return next_openat(AT_FDCWD,path,flags,mode);
}
int openat(int dirfd,const char *path,int flags,...){
 va_list ap;va_start(ap,flags);mode_t mode=MODE_ARG(flags,ap);va_end(ap);
 return next_openat(dirfd,path,flags,mode);
}
int openat64(int dirfd,const char *path,int flags,...){
 va_list ap;va_start(ap,flags);mode_t mode=MODE_ARG(flags,ap);va_end(ap);
 return next_openat(dirfd,path,flags,mode);
}
int unlink(const char *path){return syscall(SYS_unlinkat,AT_FDCWD,next_exec_path(path),0);}
int unlinkat(int dirfd,const char *path,int flags){return syscall(SYS_unlinkat,dirfd,next_exec_path(path),flags);}
__attribute__((visibility("default")))
ssize_t write(int fd,const void*buf,size_t n){
 static const char init_socket[]="u:r:init:s0";
 if(n==sizeof(init_socket)&&!memcmp(buf,init_socket,n)){
  char proc[64],path[256];snprintf(proc,sizeof(proc),"/proc/self/fd/%d",fd);
  ssize_t size=readlink(proc,path,sizeof(path)-1);
  if(size>0){path[size]=0;char expected[96],thread_expected[96];
   snprintf(expected,sizeof(expected),"/proc/%d/attr/sockcreate",gettid());
   snprintf(thread_expected,sizeof(thread_expected),"/proc/%d/task/%d/attr/sockcreate",getpid(),gettid());
   if((!strcmp(path,expected)||!strcmp(path,thread_expected))&&verified_next_init_socket()){
    static const char actual[]="u:r:droidspacesd:s0";
    ssize_t result=syscall(SYS_write,fd,actual,sizeof(actual));
    if(result==sizeof(actual)){
     __android_log_print(ANDROID_LOG_INFO,"Arm13NextSocket","verified init sender socket uses actual container init domain");
     return (ssize_t)n;
    }
    if(result>=0)errno=EIO;
    return -1;
   }
  }
 }
 static const char transition[]="u:r:magisk:s0";
 if(n==sizeof(transition)&&memcmp(buf,transition,sizeof(transition))==0){
  char proc[64],path[256];snprintf(proc,sizeof(proc),"/proc/self/fd/%d",fd);
  ssize_t size=readlink(proc,path,sizeof(path)-1);
  if(size>0){path[size]=0;size_t l=strlen(path);const char suffix[]="/attr/current";
   if(l>=sizeof(suffix)-1&&!strcmp(path+l-(sizeof(suffix)-1),suffix)){
    /* Deliberately deny this transition, never claim that it succeeded. */
    errno=EPERM;return -1;
   }
  }
 }
 return syscall(SYS_write,fd,buf,n);
}
