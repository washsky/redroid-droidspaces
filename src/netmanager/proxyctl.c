#include "proxy-common.h"
#include "instance-guard.h"
#include <sys/file.h>
#include <sys/wait.h>
#include <ifaddrs.h>
#include <grp.h>
#include <signal.h>
#include <sys/system_properties.h>
#define CHAIN "ARM13_PROXY"
static int run(char *const args[],const char *input){
 int fds[2];if(pipe(fds))return -1;pid_t p=fork();if(p<0){close(fds[0]);close(fds[1]);return -1;}
 if(!p){dup2(fds[0],0);close(fds[0]);close(fds[1]);int dev=open("/dev/null",O_WRONLY);if(dev>=0){dup2(dev,1);dup2(dev,2);close(dev);}unsetenv("LD_PRELOAD");execv(args[0],args);_exit(127);}
 close(fds[0]);size_t size=input?strlen(input):0,done=0;while(done<size){ssize_t n=write(fds[1],input+done,size-done);if(n<0&&errno==EINTR)continue;if(n<=0)break;done+=(size_t)n;}close(fds[1]);int status;while(waitpid(p,&status,0)<0){if(errno!=EINTR)return -1;}return done==size&&WIFEXITED(status)?WEXITSTATUS(status):-1;
}
static int jump(const char *op,const char *protocol){char *args[]={"/system/bin/iptables","-w","2","-t","nat",(char*)op,"OUTPUT","-p",(char*)protocol,"-j",CHAIN,NULL};return run(args,NULL);}
/* Only our named chains and exact jumps, after the per-container guard. */
static int ipt(const char *op){return jump(op,"tcp");}
static int write_cfg(const struct px_config *c){
 char path[]="/data/misc/redroid-netbroker/.proxy.XXXXXX";int fd=mkstemp(path);if(fd<0)return -1;if(fchmod(fd,0640)||fchown(fd,0,2000)){close(fd);unlink(path);return -1;}FILE *f=fdopen(fd,"w");if(!f){close(fd);unlink(path);return -1;}
 fprintf(f,"R13PROXY2\n%lu\n%d\n%s\n%s\n%d\n%d\n%d\n%s\n%d\n",c->revision,c->enabled,c->protocol,c->host,c->port,c->udp,c->dns,c->dns_host,c->auth);px_hex(f,c->username);fputc('\n',f);px_hex(f,c->password);fputc('\n',f);int bad=fflush(f)||fsync(fd);if(fclose(f))bad=1;if(!bad&&!rename(path,PX_CONFIG)){int dir=open("/data/misc/redroid-netbroker",O_DIRECTORY|O_RDONLY);if(dir>=0){fsync(dir);close(dir);}return 0;}unlink(path);return -1;
}
static void prop(const char *name,const char *value){char *args[]={"/system/bin/setprop",(char*)name,(char*)value,NULL};run(args,NULL);}
static int service(const char *expected){char state[PROP_VALUE_MAX]={0};__system_property_get("init.svc.redroid.proxy",state);return !strcmp(state,expected);}
static int ready(const struct px_config *c){unsigned long revision;int pid;FILE *f=fopen(PX_STATS,"re");if(!f)return 0;int ok=fscanf(f,"%lu%d",&revision,&pid)==2&&revision==c->revision&&pid>1;fclose(f);return ok&&service("running");}

/* Own mark bit, route table and chains, strictly after r13_instance_guard(). */
static int mangle_jump(const char *op,const char *hook,const char *chain){
 char *a[]={"/system/bin/iptables","-w","2","-t","mangle",(char*)op,(char*)hook,"-p","udp","-j",(char*)chain,NULL};return run(a,NULL);
}
static int udp_rules(int enabled,int dns,const char *host){
 const char *hooks[]={"OUTPUT","PREROUTING"},*chains[]={"ARM13_UDP_OUT","ARM13_UDP_IN"};
 if(!enabled){
  for(int p=0;p<2;p++){
   for(int i=0;i<8&&mangle_jump("-C",hooks[p],chains[p])==0;i++)if(mangle_jump("-D",hooks[p],chains[p]))return -1;
   char *flush[]={"/system/bin/iptables","-w","2","-t","mangle","-F",(char*)chains[p],NULL};run(flush,NULL);
   char *del[]={"/system/bin/iptables","-w","2","-t","mangle","-X",(char*)chains[p],NULL};run(del,NULL);
  }
  char *rule[]={"/system/bin/ip","rule","del","pref","9001","fwmark","0x40000000/0x40000000","lookup","41136",NULL};run(rule,NULL);
  char *route[]={"/system/bin/ip","route","del","local","default","dev","lo","table","41136",NULL};run(route,NULL);return 0;
 }
 char rules[4096];int n=snprintf(rules,sizeof(rules),"*mangle\n:ARM13_UDP_OUT - [0:0]\n:ARM13_UDP_IN - [0:0]\n-F ARM13_UDP_OUT\n-F ARM13_UDP_IN\n");
 if(dns)n+=snprintf(rules+n,sizeof(rules)-(size_t)n,"-A ARM13_UDP_OUT -p udp --dport 53 -j RETURN\n");
 const char *bypass[]={"0.0.0.0/8","127.0.0.0/8","10.0.0.0/8","172.16.0.0/12","192.168.0.0/16","169.254.0.0/16","224.0.0.0/4","240.0.0.0/4"};
 for(unsigned i=0;i<sizeof(bypass)/sizeof(bypass[0]);i++)n+=snprintf(rules+n,sizeof(rules)-(size_t)n,"-A ARM13_UDP_OUT -d %s -j RETURN\n",bypass[i]);
 n+=snprintf(rules+n,sizeof(rules)-(size_t)n,"-A ARM13_UDP_OUT -d %s/32 -j RETURN\n-A ARM13_UDP_OUT -j MARK --or-mark 0x40000000\n-A ARM13_UDP_IN -p udp -m mark --mark 0x40000000/0x40000000 -j TPROXY --on-ip 127.0.0.1 --on-port %d --tproxy-mark 0x40000000/0x40000000\nCOMMIT\n",host,PX_PORT);
 char *restore[]={"/system/bin/iptables-restore","--noflush","-w","2",NULL};if(run(restore,rules))return -1;
 char *route[]={"/system/bin/ip","route","replace","local","default","dev","lo","table","41136",NULL};if(run(route,NULL))return -1;
 /* Delete/add only our exact rule; no netd table or rule is replaced. */
 char *del[]={"/system/bin/ip","rule","del","pref","9001","fwmark","0x40000000/0x40000000","lookup","41136",NULL};run(del,NULL);
 char *add[]={"/system/bin/ip","rule","add","pref","9001","fwmark","0x40000000/0x40000000","lookup","41136",NULL};if(run(add,NULL))return -1;
 for(int p=0;p<2;p++)if(mangle_jump("-C",hooks[p],chains[p])&&mangle_jump("-I",hooks[p],chains[p]))return -1;
 return 0;
}

static int apply(const struct px_config *c){
 if(r13_instance_guard())return -1;
 if(!c->enabled){
  if(udp_rules(0,0,c->host))return -1;
  for(int p=0;p<2;p++)for(int i=0;i<8&&jump("-C",p?"udp":"tcp")==0;i++)if(jump("-D",p?"udp":"tcp"))return -1;
  prop("ctl.stop","redroid.proxy");char *flush[]={"/system/bin/iptables","-w","2","-t","nat","-F",CHAIN,NULL};run(flush,NULL);char *del[]={"/system/bin/iptables","-w","2","-t","nat","-X",CHAIN,NULL};run(del,NULL);return 0;
 }
 /* Install capture rules before starting the relay. A missing relay must
  * fail closed rather than briefly use the original route on boot. */
 char rules[4096];int n=snprintf(rules,sizeof(rules),"*nat\n:" CHAIN " - [0:0]\n-F " CHAIN "\n");
 if(c->dns)n+=snprintf(rules+n,sizeof(rules)-(size_t)n,"-A " CHAIN " -p udp --dport 53 -j REDIRECT --to-ports %d\n",PX_DNS_PORT);
 const char *bypass[]={"0.0.0.0/8","127.0.0.0/8","10.0.0.0/8","172.16.0.0/12","192.168.0.0/16","169.254.0.0/16","224.0.0.0/4","240.0.0.0/4"};
 for(unsigned i=0;i<sizeof(bypass)/sizeof(bypass[0]);i++)n+=snprintf(rules+n,sizeof(rules)-(size_t)n,"-A " CHAIN " -d %s -j RETURN\n",bypass[i]);
 n+=snprintf(rules+n,sizeof(rules)-(size_t)n,"-A " CHAIN " -d %s/32 -j RETURN\n-A " CHAIN " -p tcp -j REDIRECT --to-ports %d\n",c->host,PX_PORT);
 
 snprintf(rules+n,sizeof(rules)-(size_t)n,"COMMIT\n");
 char *restore[]={"/system/bin/iptables-restore","--noflush","-w","2",NULL};if(run(restore,rules))return -1;
 if(ipt("-C")&&ipt("-I"))return -1;
 if(udp_rules(c->udp,c->dns,c->host))return -1;
 if(c->dns){if(jump("-C","udp")&&jump("-I","udp"))return -1;}
 else for(int i=0;i<8&&jump("-C","udp")==0;i++)if(jump("-D","udp"))return -1;
 prop("ctl.stop","redroid.proxy");for(int i=0;i<50&&service("running");i++)usleep(100000);if(service("running"))return -1;
 unlink(PX_STATS);prop("ctl.start","redroid.proxy");for(int i=0;i<50&&!ready(c);i++)usleep(100000);if(!ready(c))return -1;
 return 0;
}
static void fail(const char *s){printf("{\"ok\":false,\"error\":\"%s\"}\n",s);exit(1);}
static void status(const struct px_config *c){
 unsigned long revision=0;int pid=0;unsigned accepted=0,success=0,failed=0,active=0;FILE *f=fopen(PX_STATS,"re");if(f){if(fscanf(f,"%lu%d%u%u%u%u",&revision,&pid,&accepted,&success,&failed,&active)!=6)revision=0;fclose(f);}
 int running=ready(c),rules=ipt("-C")==0&&(!c->dns||jump("-C","udp")==0)&&(!c->udp||(mangle_jump("-C","OUTPUT","ARM13_UDP_OUT")==0&&mangle_jump("-C","PREROUTING","ARM13_UDP_IN")==0));
 printf("{\"ok\":true,\"revision\":%lu,\"enabled\":%s,\"protocol\":\"%s\",\"host\":\"%s\",\"port\":%d,\"udp\":%s,\"dns\":%s,\"dnsHost\":\"%s\",\"auth\":%s,\"usernameHex\":\"",c->revision,c->enabled?"true":"false",c->protocol,c->host,c->port,c->udp?"true":"false",c->dns?"true":"false",c->dns_host,c->auth?"true":"false");
 px_hex(stdout,c->username);
 printf("\",\"hasPassword\":%s,\"active\":%s,\"service\":\"%s\",\"rules\":%s,\"connections\":%u,\"failures\":%u,\"scope\":\"arm-ipv4\"}\n",c->password[0]?"true":"false",c->enabled&&running&&rules?"true":"false",running?"running":"stopped",rules?"true":"false",revision==c->revision?success:0,revision==c->revision?failed:0);
}
int main(int argc,char **argv){
 signal(SIGPIPE,SIG_IGN);if(argc!=2)fail("BAD_OPERATION");
 struct px_config c;if(px_read(&c))fail("PROXY_CONFIG_INVALID");
 if(!strcmp(argv[1],"probe")){
  if(getuid()!=2000)fail("SHELL_UID_REQUIRED");struct sockaddr_in dst={.sin_family=AF_INET,.sin_port=htons(443)};inet_pton(AF_INET,"1.1.1.1",&dst.sin_addr);int fd=px_tunnel(&c,&dst);if(fd<0)fail("PROXY_HANDSHAKE_FAILED");close(fd);puts("{\"ok\":true,\"message\":\"TCP proxy handshake succeeded\"}");return 0;
 }
 if(getuid()!=0)fail("ROOT_REQUIRED");const char *guard=r13_instance_guard();if(guard)fail(guard);
 mkdir("/data/misc/redroid-netbroker",0755);int lock=open("/data/misc/redroid-netbroker/proxy.lock",O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);if(lock<0||flock(lock,LOCK_EX))fail("CONFIG_LOCK_FAILED");if(px_read(&c))fail("PROXY_CONFIG_INVALID");
 if(!strcmp(argv[1],"status")){status(&c);return 0;}
 if(!strcmp(argv[1],"test")){
  pid_t p=fork();if(!p){gid_t gid=3003;if(setgroups(1,&gid)||setgid(2000)||setuid(2000))_exit(125);char *args[]={"/vendor/bin/arm13-proxyctl","probe",NULL};execv(args[0],args);_exit(127);}if(p<0)fail("PROBE_FAILED");int s;waitpid(p,&s,0);return WIFEXITED(s)?WEXITSTATUS(s):1;
 }
 if(!strcmp(argv[1],"save")){
  struct px_config next;unsigned long expected;
  if(px_parse(stdin,&next,0))fail("BAD_PROXY_CONFIG");expected=next.revision;
  if(next.auth&&!strcmp(next.password,"__KEEP_STORED_PASSWORD__"))snprintf(next.password,sizeof(next.password),"%s",c.password);
  if(!next.auth){next.username[0]=0;next.password[0]=0;}
  if(!px_valid(&next))fail("BAD_PROXY_CONFIG");
  if(expected!=c.revision)fail("STALE_CONFIG");next.revision=c.revision+1;if(write_cfg(&next))fail("CONFIG_WRITE_FAILED");
  if(apply(&next)){write_cfg(&c);apply(&c);fail("PROXY_APPLY_FAILED_ROLLED_BACK");}c=next;
 }else if(!strcmp(argv[1],"boot")){if(apply(&c))fail("PROXY_BOOT_FAILED");}
 else fail("BAD_OPERATION");status(&c);return 0;
}
