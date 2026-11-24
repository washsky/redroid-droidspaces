#define _GNU_SOURCE
#include <linux/capability.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>

/* Container PID 1 entry only. Preserve the audited legacy boot behavior,
 * while dropping a conservative subset before any Android process
 * starts. The runtime must still apply its jail masks and seccomp filters.
 * No host sysctl, SELinux, BPF, firewall or module operation is performed. */
static int values[]={720,1280,320,60};
static const char *keys[]={"androidboot.redroid_width","androidboot.redroid_height","androidboot.redroid_dpi","androidboot.redroid_fps"};
static int parameter(char *line){
 line[strcspn(line,"\r\n")]=0;if(!line[0]||line[0]=='#')return 0;
 char *equal=strchr(line,'=');if(!equal)return -1;*equal++=0;
 for(unsigned i=0;i<4;i++)if(!strcmp(line,keys[i])){
  char *end;errno=0;long n=strtol(equal,&end,10);
  int min=i<2?240:i==2?72:15,max=i<2?3840:i==2?640:120;
  if(errno||end==equal||*end||n<min||n>max||(i<2&&(n&1)))return -1;
  values[i]=(int)n;return 0;
 }
 return -1;
}
int main(int argc,char **argv) {
 int check=argc>1&&!strcmp(argv[1],"--check");
 int fd=open("/data/adb/arm13-boot.conf",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
 if(fd>=0){struct stat c;if(fstat(fd,&c)||!S_ISREG(c.st_mode)||c.st_uid||(c.st_mode&022)){close(fd);return 124;}
  FILE *f=fdopen(fd,"r");if(!f){close(fd);return 124;}char line[256];
  while(fgets(line,sizeof(line),f))if(parameter(line)){fclose(f);fputs("arm13-cap-init: invalid boot parameter\n",stderr);return 124;}
  int error=ferror(f);fclose(f);if(error)return 124;
 }else if(errno!=ENOENT)return 124;
 for(int i=check?2:1;i<argc;i++){char line[256];if(strlen(argv[i])>=sizeof(line))return 124;strcpy(line,argv[i]);if(parameter(line))return 124;}
 char display[4][96];for(unsigned i=0;i<4;i++)snprintf(display[i],sizeof(display[i]),"%s=%d",keys[i],values[i]);
 char *const args[]={"/init","qemu=1","androidboot.hardware=redroid","androidboot.redroid_gpu_mode=guest",display[0],display[1],display[2],display[3],"androidboot.selinux=permissive",NULL};
 if(check){for(unsigned i=0;args[i];i++)puts(args[i]);return 0;}
 const char *original="/init.droidspaces";
 struct stat st;
 if(getpid()!=1||getuid()!=0||geteuid()!=0||stat(original,&st)||
    !S_ISREG(st.st_mode)||st.st_uid||(st.st_mode&022)){
  fputs("arm13-cap-init: refused outside verified root container init\n",stderr);
  return 125;
 }
 const int drops[]={CAP_SYS_MODULE,CAP_SYS_RAWIO,CAP_SYS_PACCT,
                    CAP_MAC_OVERRIDE,CAP_AUDIT_READ};
 for(unsigned i=0;i<sizeof(drops)/sizeof(drops[0]);i++){
  if(prctl(PR_CAPBSET_DROP,drops[i],0,0,0)){
   perror("arm13-cap-init: capability drop");return 126;
  }
 }
 /* Equivalent to the audited legacy wrapper: private root, then /init with
  * the same fixed platform arguments plus the validated display arguments. */
 if(mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL)){perror("arm13-cap-init: private root");return 126;}
 execv(args[0],args);
 perror("arm13-cap-init: original init");return 127;
}
