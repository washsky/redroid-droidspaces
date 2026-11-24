#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include "../src/netmanager/instance-guard.h"

static void require(int yes,const char *name){if(!yes){fprintf(stderr,"FAIL %s\n",name);exit(1);}printf("PASS %s\n",name);}
static void write_case(const char *path,const char *data,size_t size){
 int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);require(fd>=0,"create fixture");
 require(write(fd,data,size)==(ssize_t)size&&close(fd)==0,"write fixture");
}
int main(void){
 if(getuid()){fputs("Run as root in the authorized test container.\n",stderr);return 77;}
 char dir[]="/data/adb/.instance-guard-test.XXXXXX";require(mkdtemp(dir)!=NULL,"private temporary directory");
 char path[256],link[256];snprintf(path,sizeof(path),"%s/config",dir);snprintf(link,sizeof(link),"%s/link",dir);
 struct r13_instance cfg;const char *e;
 require((e=r13_instance_read(path,&cfg))&&!strcmp(e,"INSTANCE_CONFIG_REQUIRED"),"missing config rejected");
 const char good[]="R13INSTANCE1\nredroid13repro\n172.28.41.37\n";
 write_case(path,good,sizeof(good)-1);require(!r13_instance_read(path,&cfg)&&!strcmp(cfg.name,"redroid13repro"),"valid repro config");
 const char arm[]="R13INSTANCE1\nredroid13arm\n172.28.41.36\n";
 write_case(path,arm,sizeof(arm)-1);require(!r13_instance_read(path,&cfg)&&!strcmp(cfg.name,"redroid13arm"),"same reader accepts another instance config");
 require(!chmod(path,0644),"set unsafe permissions");require((e=r13_instance_read(path,&cfg))&&!strcmp(e,"INSTANCE_CONFIG_UNSAFE"),"public config rejected");
 require(!chmod(path,0600),"restore permissions");require(!symlink(path,link),"create symlink");require(r13_instance_read(link,&cfg)!=NULL,"symlink rejected");require(!unlink(link),"remove symlink");
 require(!chown(path,2000,2000),"set wrong owner");require(r13_instance_read(path,&cfg)!=NULL,"non-root config rejected");require(!chown(path,0,0),"restore root owner");
 const char *bad[]={"R13INSTANCE1\nname\n127.0.0.1\n","R13INSTANCE1\nname\n0.0.0.0\n","R13INSTANCE1\nname\n224.0.0.1\n","R13INSTANCE1\nname\n169.254.1.2\n","R13INSTANCE1\n../name\n172.28.41.37\n","R13INSTANCE2\nname\n172.28.41.37\n","R13INSTANCE1\nname\n172.28.41.37\nextra\n","R13INSTANCE1\nname\n172.28.41.37"};
 for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++){write_case(path,bad[i],strlen(bad[i]));require((e=r13_instance_read(path,&cfg))&&!strcmp(e,"INSTANCE_CONFIG_INVALID"),"malformed config rejected");}
 const char embedded[]="R13INSTANCE1\nname\n172.28.41.37\n\0junk\n";write_case(path,embedded,sizeof(embedded)-1);require(r13_instance_read(path,&cfg)!=NULL,"embedded NUL rejected");
 char longdata[256];memset(longdata,'a',sizeof(longdata));write_case(path,longdata,sizeof(longdata));require(r13_instance_read(path,&cfg)!=NULL,"oversized config rejected");
 require(!unlink(path)&&!rmdir(dir),"fixtures removed");puts("INSTANCE_CONFIG_TESTS_PASS");return 0;
}
