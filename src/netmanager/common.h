#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#define CFGDIR "/data/misc/redroid-netbroker"
#define CONFIG CFGDIR "/packages"
#define RUNDIR "/dev/arm13-netbroker"
#define UIDS RUNDIR "/uids"
#define PKGLIST "/data/system/packages.list"
#define LIMIT 512
struct config { int enabled; unsigned long revision; int count; char packages[LIMIT][256]; };
struct resolved { int count; unsigned uids[LIMIT]; };
static int valid_package(const char *s) {
    size_t n=strlen(s); if(n<3||n>255||!strchr(s,'.')||!isalpha((unsigned char)s[0]))return 0;
    for(size_t i=0;i<n;i++)if(!isalnum((unsigned char)s[i])&&s[i]!='.'&&s[i]!='_')return 0;
    return 1;
}
static void trim(char *s){s[strcspn(s,"\r\n")]=0;}
static int read_config(struct config *c){
    memset(c,0,sizeof(*c)); FILE *f=fopen(CONFIG,"re");if(!f)return -1;
    char b[512];int ok=fgets(b,sizeof(b),f)&&!strcmp(b,"R13NET1\n");
    if(!ok||!fgets(b,sizeof(b),f)){fclose(f);errno=EINVAL;return -1;}trim(b);
    if(strcmp(b,"0")&&strcmp(b,"1")){fclose(f);errno=EINVAL;return -1;} c->enabled=atoi(b);
    if(!fgets(b,sizeof(b),f)){fclose(f);errno=EINVAL;return -1;}
    char *end;c->revision=strtoul(b,&end,10);if(end==b||(*end!='\n'&&*end)){fclose(f);errno=EINVAL;return -1;}
    while(fgets(b,sizeof(b),f)){
        trim(b);if(!valid_package(b)||c->count>=LIMIT){fclose(f);errno=EINVAL;return -1;}
        int duplicate=0;for(int i=0;i<c->count;i++)if(!strcmp(c->packages[i],b))duplicate=1;
        if(!duplicate){memcpy(c->packages[c->count++],b,strlen(b)+1);}
    }
    int error=ferror(f);fclose(f);return error?-1:0;
}
static int contains_uid(const struct resolved *r,unsigned u){for(int i=0;i<r->count;i++)if(r->uids[i]==u)return 1;return 0;}
static int resolve_packages(const struct config*c,struct resolved*r){
    memset(r,0,sizeof(*r));FILE*f=fopen(PKGLIST,"re");if(!f)return -1;
    char *line=NULL;size_t cap=0;while(getline(&line,&cap,f)>=0){
        char *save,*name=strtok_r(line," \t\r\n",&save);if(!name)continue;
        int selected=0;for(int i=0;i<c->count;i++)if(!strcmp(name,c->packages[i])){selected=1;break;}if(!selected)continue;
        char *id=strtok_r(NULL," \t\r\n",&save);if(!id)continue;
        unsigned long u=strtoul(id,NULL,10);if(u<10000||u>2147483647UL)continue;
        for(int j=0;j<3;j++)if(!strtok_r(NULL," \t\r\n",&save)){id=NULL;break;}
        if(!id)continue;char*gids=strtok_r(NULL," \t\r\n",&save);if(!gids)continue;
        int inet=0;char*gs,*g=strtok_r(gids,",",&gs);while(g){if(!strcmp(g,"3003"))inet=1;g=strtok_r(NULL,",",&gs);}
        if(inet&&!contains_uid(r,(unsigned)u)&&r->count<LIMIT)r->uids[r->count++]=(unsigned)u;
    }
    free(line);int error=ferror(f);fclose(f);return error?-1:0;
}
static int atomic_uids(const struct config*c,const struct resolved*r){
    char name[]=RUNDIR "/.uids.XXXXXX";int fd=mkstemp(name);if(fd<0)return -1;
    fchmod(fd,0644);FILE*f=fdopen(fd,"w");if(!f){close(fd);unlink(name);return -1;}
    fprintf(f,"# revision=%lu\n",c->revision);
    if(c->enabled)for(int i=0;i<r->count;i++)fprintf(f,"%u\n",r->uids[i]);
    int bad=fflush(f)||fsync(fd);if(fclose(f))bad=1;
    if(!bad&&rename(name,UIDS)==0)return 0;unlink(name);return -1;
}
