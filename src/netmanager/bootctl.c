#include "proxy-common.h"
#include "instance-guard.h"
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/system_properties.h>
#include <ifaddrs.h>
#define BOOT "/data/adb/arm13-boot.conf"

static const char *keys[]={"androidboot.redroid_width","androidboot.redroid_height","androidboot.redroid_dpi","androidboot.redroid_fps"};
static const char *props[]={"ro.boot.redroid_width","ro.boot.redroid_height","ro.boot.redroid_dpi","ro.boot.redroid_fps"};
static int valid(int *v){return v[0]>=240&&v[0]<=3840&&!(v[0]&1)&&v[1]>=240&&v[1]<=3840&&!(v[1]&1)&&v[2]>=72&&v[2]<=640&&v[3]>=15&&v[3]<=120;}
static void fail(const char *s){printf("{\"ok\":false,\"error\":\"%s\"}\n",s);exit(1);}
static unsigned long read_boot(int *v){
 int defaults[]={720,1280,320,60};memcpy(v,defaults,sizeof(defaults));uint32_t hash=2166136261u;
 int fd=open(BOOT,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0){if(errno==ENOENT)return hash;fail("BOOT_CONFIG_READ_FAILED");}
 struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid||(st.st_mode&022)){close(fd);fail("BOOT_CONFIG_UNSAFE");}
 FILE *f=fdopen(fd,"r");if(!f){close(fd);fail("BOOT_CONFIG_READ_FAILED");}char line[256];
 while(fgets(line,sizeof(line),f)){
  for(const unsigned char *p=(void*)line;*p;p++)hash=(hash^*p)*16777619u;
  line[strcspn(line,"\r\n")]=0;if(!line[0]||line[0]=='#')continue;
  char *eq=strchr(line,'=');if(!eq)fail("BOOT_CONFIG_INVALID");*eq++=0;int found=0;
  for(unsigned i=0;i<4;i++)if(!strcmp(line,keys[i])){char *end;errno=0;long n=strtol(eq,&end,10);if(errno||end==eq||*end||n<0||n>10000)fail("BOOT_CONFIG_INVALID");v[i]=(int)n;found=1;}
  if(!found)fail("BOOT_CONFIG_INVALID");
 }int error=ferror(f);fclose(f);if(error||!valid(v))fail("BOOT_CONFIG_INVALID");return hash;
}
static int write_boot(int *v){
 char path[]="/data/adb/.arm13-boot.XXXXXX";int fd=mkstemp(path);if(fd<0)return -1;
 if(fchmod(fd,0600)||fchown(fd,0,0)){close(fd);unlink(path);return -1;}
 FILE *f=fdopen(fd,"w");if(!f){close(fd);unlink(path);return -1;}
 for(unsigned i=0;i<4;i++)fprintf(f,"%s=%d\n",keys[i],v[i]);
 int bad=fflush(f)||fsync(fd);if(fclose(f))bad=1;
 if(!bad&&!rename(path,BOOT)){int dir=open("/data/adb",O_RDONLY|O_DIRECTORY);if(dir>=0){fsync(dir);close(dir);}return 0;}unlink(path);return -1;
}
static int check(void){
 pid_t p=fork();if(!p){int fd=open("/dev/null",O_WRONLY);if(fd>=0){dup2(fd,1);dup2(fd,2);}execl("/init.arm13-caps","/init.arm13-caps","--check",NULL);_exit(127);}
 if(p<0)return -1;int s;while(waitpid(p,&s,0)<0)if(errno!=EINTR)return -1;return WIFEXITED(s)?WEXITSTATUS(s):-1;
}
static void status(unsigned long rev,int *v){
 int actual[4];for(unsigned i=0;i<4;i++){char p[PROP_VALUE_MAX]={0};__system_property_get(props[i],p);actual[i]=atoi(p);}
 printf("{\"ok\":true,\"revision\":%lu,\"width\":%d,\"height\":%d,\"dpi\":%d,\"fps\":%d,\"currentWidth\":%d,\"currentHeight\":%d,\"currentDpi\":%d,\"currentFps\":%d,\"pending\":%s}\n",rev,v[0],v[1],v[2],v[3],actual[0],actual[1],actual[2],actual[3],memcmp(v,actual,sizeof(actual))?"true":"false");
}
int main(int argc,char **argv){
 if(getuid())fail("ROOT_REQUIRED");const char *guard=r13_instance_guard();if(guard)fail(guard);
 if(argc!=2||(strcmp(argv[1],"status")&&strcmp(argv[1],"save")))fail("BAD_OPERATION");
 int lock=open("/data/adb/arm13-boot.lock",O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);if(lock<0||flock(lock,LOCK_EX))fail("CONFIG_LOCK_FAILED");
 int v[4];unsigned long rev=read_boot(v);
 if(!strcmp(argv[1],"save")){
  unsigned long expected;int next[4];char extra;
  if(scanf("%lu%d%d%d%d",&expected,next,next+1,next+2,next+3)!=5||scanf(" %c",&extra)==1||!valid(next))fail("BAD_DISPLAY_CONFIG");
  if(expected!=rev)fail("STALE_CONFIG");
  if(write_boot(next))fail("CONFIG_WRITE_FAILED");
  if(check()){write_boot(v);fail("BOOT_CHECK_FAILED_ROLLED_BACK");}
  rev=read_boot(v);
 }
 status(rev,v);return 0;
}
