#ifndef R13_INSTANCE_GUARD_H
#define R13_INSTANCE_GUARD_H
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define R13_INSTANCE_CONFIG "/data/adb/arm13-instance.conf"
struct r13_instance { char name[64]; struct in_addr address; };

/* Exact three-line format. The name is a label, not a verified UUID. */
static inline int r13_instance_parse(char *data,size_t size,struct r13_instance *out){
 if(size<3||memchr(data,0,size)||data[size-1]!='\n')return -1;
 data[size-1]=0;char *name=strchr(data,'\n');if(!name)return -1;*name++=0;
 char *ip=strchr(name,'\n');if(!ip)return -1;*ip++=0;
 if(strcmp(data,"R13INSTANCE1")||strchr(ip,'\n')||!name[0]||strlen(name)>=sizeof(out->name))return -1;
 for(size_t i=0;name[i];i++){
  unsigned char c=(unsigned char)name[i];int alnum=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');
  if(!alnum&&(i==0||(c!='_'&&c!='-'&&c!='.')))return -1;
 }
 if(inet_pton(AF_INET,ip,&out->address)!=1)return -1;
 uint32_t a=ntohl(out->address.s_addr);
 if(!(a>>24)||(a>>24)==127||(a>>16)==0xa9fe||(a>>24)>=224)return -1;
 strcpy(out->name,name);return 0;
}

/* path is exposed only for the parser test; production always uses the literal
 * above. No environment variable, CLI override or compiled address fallback. */
static inline const char *r13_instance_read(const char *path,struct r13_instance *out){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(fd<0)return errno==ENOENT?"INSTANCE_CONFIG_REQUIRED":"INSTANCE_CONFIG_UNSAFE";
 struct stat st;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid||st.st_gid||(st.st_mode&07777)!=0600||st.st_nlink!=1){close(fd);return "INSTANCE_CONFIG_UNSAFE";}
 char data[192];size_t used=0;
 for(;;){ssize_t n=read(fd,data+used,sizeof(data)-used);if(n<0&&errno==EINTR)continue;
  if(n<0){close(fd);return "INSTANCE_CONFIG_READ_FAILED";}if(!n)break;used+=(size_t)n;
  if(used==sizeof(data)){close(fd);return "INSTANCE_CONFIG_INVALID";}
 }
 close(fd);return r13_instance_parse(data,used,out)?"INSTANCE_CONFIG_INVALID":NULL;
}

#ifdef __ANDROID__
#include <ifaddrs.h>
#include <sys/system_properties.h>
static inline const char *r13_instance_guard(void){
 struct r13_instance cfg;const char *error=r13_instance_read(R13_INSTANCE_CONFIG,&cfg);if(error)return error;
 const char *self[]={"/proc/self/ns/net","/proc/self/ns/pid"};
 const char *init[]={"/proc/1/ns/net","/proc/1/ns/pid"};
 for(unsigned i=0;i<2;i++){struct stat a,b;if(stat(self[i],&a)||stat(init[i],&b)||a.st_ino!=b.st_ino||a.st_dev!=b.st_dev)return "INSTANCE_NAMESPACE_REQUIRED";}
 struct stat entry;if(lstat("/init.arm13-caps",&entry)||entry.st_uid||(entry.st_mode&022)||!S_ISREG(entry.st_mode))return "INSTANCE_INIT_REQUIRED";
 char device[PROP_VALUE_MAX]={0};__system_property_get("ro.product.device",device);
 if(strcmp(device,"redroid_arm64_only"))return "INSTANCE_DEVICE_MISMATCH";
 struct ifaddrs *list;if(getifaddrs(&list))return "INSTANCE_ADDRESS_READ_FAILED";int yes=0;
 for(struct ifaddrs *p=list;p;p=p->ifa_next)if(p->ifa_addr&&p->ifa_addr->sa_family==AF_INET&&!strcmp(p->ifa_name,"eth0")){
  const struct sockaddr_in *in=(const void*)p->ifa_addr;if(in->sin_addr.s_addr==cfg.address.s_addr)yes=1;
 }
 freeifaddrs(list);return yes?NULL:"INSTANCE_ADDRESS_MISMATCH";
}
#endif
#endif
