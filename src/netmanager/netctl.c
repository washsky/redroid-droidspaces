#include "common.h"
#include <sys/wait.h>
#include <time.h>
static int prop(const char*name,const char*value){
    pid_t p=fork();if(!p){execl("/system/bin/setprop","setprop",name,value,NULL);_exit(127);}int s;
    if(p<0||waitpid(p,&s,0)<0)return -1;return WIFEXITED(s)?WEXITSTATUS(s):-1;
}
static void getprop(const char*name,char*out,size_t cap){
    int fds[2];out[0]=0;if(pipe(fds))return;pid_t p=fork();
    if(!p){dup2(fds[1],1);close(fds[0]);close(fds[1]);execl("/system/bin/getprop","getprop",name,NULL);_exit(127);}
    close(fds[1]);ssize_t n=read(fds[0],out,cap-1);close(fds[0]);if(n>0){out[n]=0;trim(out);}if(p>0)waitpid(p,NULL,0);
}
static void fail(const char*s){printf("{\"ok\":false,\"error\":\"%s\"}\n",s);exit(1);}
static int write_config(const struct config*c){
    char path[]=CFGDIR "/.packages.XXXXXX";int fd=mkstemp(path);if(fd<0)return -1;fchmod(fd,0644);
    FILE*f=fdopen(fd,"w");if(!f){close(fd);unlink(path);return -1;}
    fprintf(f,"R13NET1\n%d\n%lu\n",c->enabled,c->revision);
    for(int i=0;i<c->count;i++)fprintf(f,"%s\n",c->packages[i]);
    int bad=fflush(f)||fsync(fd);if(fclose(f))bad=1;
    if(!bad&&rename(path,CONFIG)==0){int d=open(CFGDIR,O_DIRECTORY|O_RDONLY);if(d>=0){fsync(d);close(d);}return 0;}
    unlink(path);return -1;
}
static unsigned long active_revision(void){FILE*f=fopen(UIDS,"re");unsigned long v=0;if(f){fscanf(f,"# revision=%lu",&v);fclose(f);}return v;}
static void status(const struct config*c){
    char state[64];getprop("init.svc.redroid.netbroker",state,sizeof(state));
    struct resolved r;if(resolve_packages(c,&r))memset(&r,0,sizeof(r));
    printf("{\"ok\":true,\"enabled\":%s,\"revision\":%lu,\"activeRevision\":%lu,\"service\":\"%s\",\"packages\":[",c->enabled?"true":"false",c->revision,active_revision(),state);
    for(int i=0;i<c->count;i++)printf("%s\"%s\"",i?",":"",c->packages[i]);
    printf("],\"configuredUids\":[");for(int i=0;i<r.count;i++)printf("%s%u",i?",":"",r.uids[i]);
    printf("],\"uids\":[");FILE*f=fopen(UIDS,"re");int first=1;if(f){char line[64];while(fgets(line,sizeof(line),f)){if(!isdigit((unsigned char)line[0]))continue;char*end;unsigned long u=strtoul(line,&end,10);if((*end=='\n'||*end==0)&&u>=10000&&u<=2147483647UL){printf("%s%lu",first?"":",",u);first=0;}}fclose(f);}
    printf("],\"socketUid\":2000,\"installedConfig\":true}\n");
}
int main(int argc,char**argv){
    if(getuid()!=0)fail("ROOT_REQUIRED");if(argc!=2)fail("BAD_OPERATION");
    mkdir(CFGDIR,0755);chmod(CFGDIR,0755);
    int lock=open(CFGDIR "/lock",O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
    if(lock<0||flock(lock,LOCK_EX))fail("CONFIG_LOCK_FAILED");
    struct config c;if(read_config(&c))fail("CONFIG_INVALID");
    if(!strcmp(argv[1],"status")){status(&c);return 0;}
    if(!strcmp(argv[1],"save")){
        struct config next={0};char b[512],*end;
        if(!fgets(b,sizeof(b),stdin))fail("BAD_REVISION");
        unsigned long expected=strtoul(b,&end,10);if(end==b||(*end!='\n'&&*end))fail("BAD_REVISION");
        if(expected!=c.revision)fail("STALE_CONFIG");
        if(!fgets(b,sizeof(b),stdin))fail("BAD_ENABLED");trim(b);
        if(strcmp(b,"0")&&strcmp(b,"1"))fail("BAD_ENABLED");next.enabled=atoi(b);next.revision=c.revision+1;
        while(fgets(b,sizeof(b),stdin)){
            trim(b);if(!valid_package(b)||next.count>=LIMIT)fail("BAD_PACKAGE");
            int dup=0;for(int i=0;i<next.count;i++)if(!strcmp(b,next.packages[i]))dup=1;
            if(!dup){memcpy(next.packages[next.count++],b,strlen(b)+1);}
        }
        if(ferror(stdin))fail("INPUT_FAILED");if(write_config(&next))fail("CONFIG_WRITE_FAILED");c=next;
    } else if(strcmp(argv[1],"boot")&&strcmp(argv[1],"restart"))fail("BAD_OPERATION");
    if(!c.enabled){
        struct resolved none={0};if(atomic_uids(&c,&none))fail("RUNTIME_WRITE_FAILED");
        if(prop("ctl.stop","redroid.netbroker"))fail("SERVICE_STOP_FAILED");
    }else{
        if(!strcmp(argv[1],"restart")){
            if(prop("ctl.stop","redroid.netbroker"))fail("SERVICE_STOP_FAILED");
            for(int i=0;i<50;i++){char state[64];getprop("init.svc.redroid.netbroker",state,sizeof(state));if(!strcmp(state,"stopped"))break;usleep(100000);}
        }
        if(prop("ctl.start","redroid.netbroker"))fail("SERVICE_START_FAILED");
        for(int i=0;i<30;i++){if(active_revision()==c.revision)break;usleep(100000);}
    }
    status(&c);return 0;
}
