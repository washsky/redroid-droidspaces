#include <linux/bpf.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
static int call(int cmd,union bpf_attr*a){return syscall(__NR_bpf,cmd,a,sizeof(*a));}
static int objinfo(int fd,void*p,unsigned len){union bpf_attr a={0};a.info.bpf_fd=fd;a.info.info_len=len;a.info.info=(uint64_t)p;return call(BPF_OBJ_GET_INFO_BY_FD,&a);}
static void prog(unsigned id){
 union bpf_attr a={0};a.prog_id=id;int fd=call(BPF_PROG_GET_FD_BY_ID,&a);if(fd<0){printf("PROG %u ERROR %s\n",id,strerror(errno));return;}
 struct bpf_prog_info p={0};if(objinfo(fd,&p,sizeof(p))){perror("proginfo");close(fd);return;}
 uint32_t maps[128]={0};memset(&p,0,sizeof(p));p.map_ids=(uint64_t)maps;p.nr_map_ids=128;
 if(objinfo(fd,&p,sizeof(p))){perror("proginfo2");close(fd);return;}
 printf("PROG id=%u name=%s type=%u MAP_IDS=",p.id,p.name,p.type);
 for(unsigned i=0;i<p.nr_map_ids;i++)printf("%s%u",i?",":"",maps[i]);puts("");close(fd);
}
static void mapread(const char*path){
 union bpf_attr a={0};a.pathname=(uint64_t)path;int fd=call(BPF_OBJ_GET,&a);
 if(fd<0){printf("MAP %s ERROR=%s\n",path,strerror(errno));return;}
 struct bpf_map_info m={0};if(objinfo(fd,&m,sizeof(m))){perror("mapinfo");close(fd);return;}
 printf("MAP path=%s id=%u name=%s key=%u value=%u\n",path,m.id,m.name,m.key_size,m.value_size);
 if(m.key_size!=4||m.value_size>64){close(fd);return;}
 uint32_t keys[]={0,1000,2000,10077,10078,10080,65534};
 for(unsigned i=0;i<sizeof(keys)/4;i++){
  unsigned char v[64]={0};memset(&a,0,sizeof(a));a.map_fd=fd;a.key=(uint64_t)&keys[i];a.value=(uint64_t)v;
  int r=call(BPF_MAP_LOOKUP_ELEM,&a);printf("  key=%u ",keys[i]);
  if(r)printf("MISSING errno=%d\n",errno);
  else{printf("value=");for(unsigned j=0;j<m.value_size;j++)printf("%02x",v[j]);puts("");}
 }
 close(fd);
}
int main(int argc,char**argv){
 const char*cg=argc>1?argv[1]:"/sys/fs/cgroup";int fd=open(cg,O_RDONLY|O_DIRECTORY);if(fd<0){perror("cgroup");return 2;}
 unsigned types[]={0,1,2,10,11};
 for(unsigned t=0;t<5;t++)for(unsigned effective=0;effective<2;effective++){
  uint32_t ids[128]={0};union bpf_attr a={0};a.query.target_fd=fd;a.query.attach_type=types[t];a.query.query_flags=effective?BPF_F_QUERY_EFFECTIVE:0;a.query.prog_cnt=128;a.query.prog_ids=(uint64_t)ids;
  if(call(BPF_PROG_QUERY,&a)){printf("QUERY type=%u effective=%u errno=%d\n",types[t],effective,errno);continue;}
  printf("CGROUP=%s type=%u effective=%u flags=0x%x count=%u\n",cg,types[t],effective,a.query.attach_flags,a.query.prog_cnt);
  if(!effective)for(unsigned i=0;i<a.query.prog_cnt;i++)prog(ids[i]);
 }
 close(fd);
 mapread("/sys/fs/bpf/netd_shared/map_netd_configuration_map");
 mapread("/sys/fs/bpf/netd_shared/map_netd_uid_owner_map");
 mapread("/sys/fs/bpf/netd_shared/map_netd_uid_permission_map");
 return 0;
}
