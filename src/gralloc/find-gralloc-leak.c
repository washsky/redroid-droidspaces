#include <android/hardware_buffer.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
static unsigned long base;
static unsigned owner(){if(!base){FILE*f=fopen("/proc/self/maps","r");char l[512];while(fgets(l,sizeof(l),f))if(strstr(l,"libgralloccore.so")&&strstr(l,"00000000")){sscanf(l,"%lx-",&base);break;}fclose(f);}if(!base)return 0;uint64_t p=*(uint64_t*)(base+0x1a880)&0x00ffffffffffffffULL;return p?*(uint32_t*)(p+16):0;}
static void *allocate(void*p){AHardwareBuffer_Desc d={.width=16,.height=16,.layers=1,.format=1,.usage=0x300};AHardwareBuffer*b=0;int r=AHardwareBuffer_allocate(&d,&b);printf("SECOND_THREAD_ALLOC=%d\n",r);if(b)AHardwareBuffer_release(b);return 0;}
int main(){setbuf(stdout,0);uint64_t us[]={0,0x100,0x200,0x300,3,0x30,0x33,0x103,0x130,0x133,0x1000000,0x2000100,0x4000100};int n=0;for(unsigned u=0;u<sizeof(us)/sizeof(*us);u++)for(unsigned f=1;f<512;f++){AHardwareBuffer_Desc d={.width=16,.height=16,.layers=1,.format=f,.usage=us[u]};int r=AHardwareBuffer_isSupported(&d);n++;unsigned o=owner();if(o){printf("LOCK_LEAK format=%x usage=%llx support=%d owner=%x query=%d\n",f,(unsigned long long)us[u],r,o,n);pthread_t t;pthread_create(&t,0,allocate,0);pthread_join(t,0);return 4;}}printf("NO_LEAK queries=%d\n",n);return 0;}
