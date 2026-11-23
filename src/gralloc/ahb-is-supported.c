#include <android/hardware_buffer.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
static void mapped(){FILE*f=fopen("/proc/self/maps","r");char l[512];unsigned long base=0;while(fgets(l,sizeof(l),f)){if(strstr(l,"libgralloccore.so")&&strstr(l,"00000000")){sscanf(l,"%lx-",&base);puts(l);break;}}fclose(f);if(base){uint64_t p=*(uint64_t*)(base+0x1a880)&0x00ffffffffffffffULL;printf("mapped branch=%x ptr=%llx owner=%x\n",*(uint32_t*)(base+0x12784),(unsigned long long)p,*(uint32_t*)(p+16));}}
static void *next_thread(void *data){AHardwareBuffer_Desc *d=data; d->format=1;d->usage=AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE|AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;printf("second-thread support=%d\n",AHardwareBuffer_isSupported(d));AHardwareBuffer *b=0;int r=AHardwareBuffer_allocate(d,&b);printf("second-thread alloc=%d buffer=%p\n",r,b);if(b)AHardwareBuffer_release(b);return 0;}
int main(int argc,char**argv){setbuf(stdout,NULL);unsigned f=argc>1?strtoul(argv[1],0,0):1;
AHardwareBuffer_Desc d={.width=4,.height=4,.layers=1,.format=f,.usage=AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE|AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT};
if(argc>2)d.usage=strtoull(argv[2],0,0); if(argc>3)d.width=d.height=strtoul(argv[3],0,0); printf("query format=%u usage=%llx size=%u\n",f,(unsigned long long)d.usage,d.width);printf("format=%u support=%d\n",f,AHardwareBuffer_isSupported(&d));
mapped();pthread_t t;int r=pthread_create(&t,0,next_thread,&d);if(r)return r;pthread_join(t,0);puts("CROSS_THREAD_PASS");return 0;}
