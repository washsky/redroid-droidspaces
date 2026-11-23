#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
int main(int argc,char **argv) {
 if(argc!=2 && argc!=3)return 2;
 int pid=atoi(argv[1]);if(pid<1)return 3;
 char path[128],line[1024];
 snprintf(path,sizeof(path),"/proc/%d/maps",pid);
 FILE *f=fopen(path,"r");if(!f){perror("maps");return 4;}
 unsigned long base=0;
 while(fgets(line,sizeof(line),f))if(strstr(line,"/vendor/lib64/libgralloccore.so")&&strstr(line,"00000000")){sscanf(line,"%lx-",&base);break;}
 fclose(f);if(!base)return 5;
 snprintf(path,sizeof(path),"/proc/%d/mem",pid);int fd=open(path,O_RDONLY);if(fd<0){perror("mem");return 6;}
 uint64_t ptr=0;
 if(pread(fd,&ptr,8,base+0x1a880)!=8){perror("global");return 7;}
 if(argc==3)ptr=strtoull(argv[2],NULL,0)-8;
 ptr&=0x00ffffffffffffffULL;
 uint32_t words[10];
 if(pread(fd,words,sizeof(words),ptr+8)!=sizeof(words)){perror("mutex");return 8;}
 printf("BASE=%lx BUFFER_MANAGER=%llx MUTEX=%llx\n",base,(unsigned long long)ptr,(unsigned long long)(ptr+8));
 for(int i=0;i<10;i++)printf("WORD%d=%08x DEC=%u\n",i,words[i],words[i]);
 return 0;
}
