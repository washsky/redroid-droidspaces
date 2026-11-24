#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <unistd.h>

/* Official Magisk stays in its guarded private mount namespace. Clone only
 * newly created Android partition overlays into container init before Zygote.
 * Detached mount FDs avoid pathname traversal through another process's root.
 */
#ifndef OPEN_TREE_CLONE
#define OPEN_TREE_CLONE 1
#define OPEN_TREE_CLOEXEC O_CLOEXEC
#endif
#ifndef AT_RECURSIVE
#define AT_RECURSIVE 0x8000
#endif
#ifndef MOVE_MOUNT_F_EMPTY_PATH
#define MOVE_MOUNT_F_EMPTY_PATH 0x00000004
#endif
#define MAX_MOUNTS 2048
struct entry { int id, fd; char path[PATH_MAX]; };
static struct entry entries[MAX_MOUNTS];
static int ids[MAX_MOUNTS];
#define NS_MARKER "/dev/.arm13-magisk-mountns"

static void die(const char *what) { perror(what); exit(1); }
static int nsfd(int pid) {
    char p[64]; snprintf(p,sizeof(p),"/proc/%d/ns/mnt",pid);
    int fd=open(p,O_RDONLY|O_CLOEXEC); if(fd<0)die(p); return fd;
}
static int partition(const char *p) {
    const char *roots[]={"/system","/vendor","/product","/system_ext",NULL};
    for(int i=0;roots[i];i++) {
        size_t n=strlen(roots[i]);
        if(!strncmp(p,roots[i],n)&&(p[n]=='/'||p[n]==0))return 1;
    }
    return 0;
}
static void decode(char *s) {
    char *d=s;
    while(*s) {
        if(s[0]=='\\' && s[1]>='0' && s[1]<='7' && s[2]>='0' && s[2]<='7' && s[3]>='0' && s[3]<='7') {
            *d++=(char)((s[1]-'0')*64+(s[2]-'0')*8+s[3]-'0');s+=4;
        } else *d++=*s++;
    }
    *d=0;
}
static int by_length(const void *a,const void *b) {
    const struct entry *x=a,*y=b;
    size_t nx=strlen(x->path),ny=strlen(y->path);
    return nx<ny?-1:nx>ny?1:strcmp(x->path,y->path);
}
static unsigned long root_peer(void) {
    FILE *f=fopen("/proc/self/mountinfo","re");if(!f)die("mountinfo");
    char *line=NULL;size_t cap=0;unsigned long group=0;
    while(getline(&line,&cap,f)>0) {
        int id,parent;char dev[64],root[PATH_MAX],path[PATH_MAX];
        if(sscanf(line,"%d %d %63s %4095s %4095s",&id,&parent,dev,root,path)!=5||strcmp(path,"/"))continue;
        char *p=strstr(line," shared:");if(p)group=strtoul(p+8,NULL,10);
        break;
    }
    free(line);fclose(f);return group;
}
static void verify_init_namespace(int init) {
    int marker=open(NS_MARKER,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(marker<0)die("prepared namespace marker");
    struct stat st,ns;if(fstat(marker,&st)||fstat(init,&ns))die("namespace identity");
    if(!S_ISREG(st.st_mode)||st.st_uid!=0||(st.st_mode&0777)!=0600)exit(3);
    FILE *f=fdopen(marker,"r");if(!f)die("namespace marker stream");
    unsigned long long inode;unsigned long peer;
    int parsed=fscanf(f,"%llu %lu",&inode,&peer);fclose(f);
    if(parsed!=2||inode!=(unsigned long long)ns.st_ino||!peer||peer!=root_peer()) {
        fprintf(stderr,"container peer identity changed; operation refused\n");exit(3);
    }
}
static int same_node(const struct stat *a,const struct stat *b) {
    return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_rdev==b->st_rdev;
}
static void repair_pty(int init,int daemon,int pid) {
    char path[64],line[256];int parent=-1;unsigned uid=UINT_MAX;
    snprintf(path,sizeof(path),"/proc/%d/status",pid);
    FILE *f=fopen(path,"re");if(!f)die("daemon status");
    while(fgets(line,sizeof(line),f)) {
        if(!strncmp(line,"PPid:",5))sscanf(line+5,"%d",&parent);
        if(!strncmp(line,"Uid:",4))sscanf(line+4,"%u",&uid);
    }
    fclose(f);
    struct stat init_ns,daemon_ns,exe,magisk,ptmx,slave,daemon_ptmx;
    snprintf(path,sizeof(path),"/proc/%d/exe",pid);
    if(parent!=1||uid!=0||stat(path,&exe)||stat("/sbin/magisk",&magisk)||
       !same_node(&exe,&magisk)||fstat(init,&init_ns)||fstat(daemon,&daemon_ns)||
       init_ns.st_ino==daemon_ns.st_ino) {
        fprintf(stderr,"refusing unguarded daemon PTY repair\n");exit(3);
    }
    if(setns(init,CLONE_NEWNS))die("enter init for PTY source");
    verify_init_namespace(init);
    if(stat("/dev/ptmx",&ptmx)||stat("/dev/pts/ptmx",&slave)||
       !S_ISCHR(ptmx.st_mode)||!S_ISCHR(slave.st_mode)||ptmx.st_rdev!=slave.st_rdev) {
        fprintf(stderr,"invalid init PTY devices; repair refused\n");exit(3);
    }
    struct stat source_slave=slave;
    int tree=(int)syscall(__NR_open_tree,AT_FDCWD,"/dev/pts",OPEN_TREE_CLONE|OPEN_TREE_CLOEXEC);
    if(tree<0)die("clone container devpts");
    if(setns(daemon,CLONE_NEWNS))die("enter daemon for PTY repair");
    if(root_peer()!=0||stat("/dev/ptmx",&daemon_ptmx)||
       !S_ISCHR(daemon_ptmx.st_mode)||ptmx.st_rdev!=daemon_ptmx.st_rdev) {
        fprintf(stderr,"daemon namespace or ptmx identity changed; repair refused\n");exit(3);
    }
    if(syscall(__NR_move_mount,tree,"",AT_FDCWD,"/dev/pts",MOVE_MOUNT_F_EMPTY_PATH))die("attach daemon devpts");
    if(mount(NULL,"/dev/pts",NULL,MS_PRIVATE,NULL)||stat("/dev/pts/ptmx",&slave)||!same_node(&source_slave,&slave)) {
        int error=errno;umount2("/dev/pts",MNT_DETACH);errno=error;die("verify daemon devpts");
    }
    /* Android initially uses a legacy ptmx node; Droidspaces may later bind
     * an explicit node there. Pin BOTH aliases to this same devpts instance
     * inside the private daemon namespace before any interactive SU request. */
    if(mount("/dev/pts/ptmx","/dev/ptmx",NULL,MS_BIND,NULL)) {
        int error=errno;umount2("/dev/pts",MNT_DETACH);errno=error;die("bind daemon ptmx");
    }
    if(mount(NULL,"/dev/ptmx",NULL,MS_PRIVATE,NULL)||stat("/dev/ptmx",&daemon_ptmx)||!same_node(&slave,&daemon_ptmx)) {
        int error=errno;umount2("/dev/ptmx",MNT_DETACH);umount2("/dev/pts",MNT_DETACH);errno=error;die("verify daemon PTY aliases");
    }
    close(tree);puts("DAEMON_PTY_ALIASES_REPAIRED=1");
}
int main(int argc,char **argv) {
    char device[PROP_VALUE_MAX]={0};
    __system_property_get("ro.product.device",device);
    if(getuid()!=0||strcmp(device,"redroid_arm64_only")) {
        fprintf(stderr,"refusing outside arm64 ReDroid root\n");return 2;
    }
    int init=nsfd(1);
    if(argc==2&&!strcmp(argv[1],"prepare")) {
        if(setns(init,CLONE_NEWNS))die("enter container init namespace");
        /* Break all host peer relationships, then establish fresh groups for
         * Android's internal vold/app namespace propagation. Leaving these
         * private would break emulated storage mounted after post-fs-data. */
        if(mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL))die("private container mounts");
        if(mount(NULL,"/",NULL,MS_REC|MS_SHARED,NULL))die("container-local shared mounts");
        struct stat ns;if(fstat(init,&ns))die("init namespace identity");
        unsigned long peer=root_peer();if(!peer){errno=EINVAL;die("new root peer");}
        int fd=open(NS_MARKER,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);
        if(fd<0)die("namespace marker");
        if(dprintf(fd,"%llu %lu\n",(unsigned long long)ns.st_ino,peer)<0)die("write namespace marker");
        close(fd);printf("CONTAINER_LOCAL_PEER=%lu\n",peer);return 0;
    }
    int pty=argc==3&&!strcmp(argv[1],"repair-pty");
    if(!pty&&(argc!=4||strcmp(argv[1],"export"))) {
        fprintf(stderr,"usage: %s prepare | repair-pty DAEMON_PID | export DAEMON_PID BASELINE_MOUNTINFO\n",argv[0]);return 2;
    }
    char *end;long pid=strtol(argv[2],&end,10);
    if(*end||pid<2||pid>INT_MAX)return 2;
    char proc[64],comm[64]={0};snprintf(proc,sizeof(proc),"/proc/%ld/comm",pid);
    FILE *f=fopen(proc,"r");if(!f)die(proc);
    if(!fgets(comm,sizeof(comm),f))die("read daemon comm");fclose(f);
    if(strcmp(comm,"magiskd\n")){fprintf(stderr,"not magiskd\n");return 2;}
    int daemon=nsfd((int)pid),nids=0,n=0;
    if(pty){repair_pty(init,daemon,(int)pid);return 0;}
    f=fopen(argv[3],"re");if(!f)die("baseline mountinfo");
    char *line=NULL;size_t cap=0;
    while(getline(&line,&cap,f)>0) {
        if(nids==MAX_MOUNTS){errno=E2BIG;die("baseline mounts");}
        if(sscanf(line,"%d",&ids[nids])==1)nids++;
    }
    fclose(f);
    snprintf(proc,sizeof(proc),"/proc/%ld/mountinfo",pid);
    f=fopen(proc,"re");if(!f)die(proc);
    while(getline(&line,&cap,f)>0) {
        int id,parent;char dev[64],root[PATH_MAX],path[PATH_MAX];
        if(sscanf(line,"%d %d %63s %4095s %4095s",&id,&parent,dev,root,path)!=5)continue;
        int old=0;for(int i=0;i<nids;i++)if(ids[i]==id)old=1;
        decode(path);
        if(old||!partition(path))continue;
        if(n==MAX_MOUNTS){errno=E2BIG;die("module mounts");}
        entries[n].id=id;entries[n].fd=-1;strcpy(entries[n++].path,path);
    }
    fclose(f);free(line);
    qsort(entries,n,sizeof(entries[0]),by_length);
    if(setns(daemon,CLONE_NEWNS))die("enter guarded daemon namespace");
    for(int i=0;i<n;i++) {
        int covered=0;
        for(int j=0;j<i;j++) {
            size_t len=strlen(entries[j].path);
            if(entries[j].fd>=0&&!strncmp(entries[i].path,entries[j].path,len)&&entries[i].path[len]=='/')covered=1;
        }
        if(covered)continue;
        entries[i].fd=(int)syscall(__NR_open_tree,AT_FDCWD,entries[i].path,OPEN_TREE_CLONE|OPEN_TREE_CLOEXEC|AT_RECURSIVE);
        if(entries[i].fd<0)die("clone module mount tree");
    }
    if(setns(init,CLONE_NEWNS))die("return to container init namespace");
    /* Require the exact namespace and newly allocated root peer recorded by
     * prepare. Its private->shared sequence makes these container-only peers. */
    verify_init_namespace(init);
    int count=0;
    for(int i=0;i<n;i++)if(entries[i].fd>=0) {
        if(syscall(__NR_move_mount,entries[i].fd,"",AT_FDCWD,entries[i].path,MOVE_MOUNT_F_EMPTY_PATH)) {
            int saved=errno;
            for(int j=i-1;j>=0;j--)if(entries[j].fd==-2)
                if(umount2(entries[j].path,MNT_DETACH))perror("rollback module mount");
            errno=saved;die("attach container module mount");
        }
        printf("EXPORTED %s\n",entries[i].path);close(entries[i].fd);
        entries[i].fd=-2;count++;
    }
    printf("MODULE_MOUNTS_EXPORTED=%d\n",count);return 0;
}
