#include <android/hardware_buffer.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
static int sockets[2];
static _Atomic int received=0, err=0;
static void *receiver(void *unused) {
 (void)unused;
 for (int i=0;i<128;i++) {
  AHardwareBuffer *b=NULL;
  int r=AHardwareBuffer_recvHandleFromUnixSocket(sockets[1], &b);
  if(r || !b) { printf("RECV_FAIL %d %d\n",i,r); err=1; return NULL; }
  AHardwareBuffer_Desc d={0};
  AHardwareBuffer_describe(b,&d);
  if(d.width!=64 || d.height!=64) { err=1; return NULL; }
  AHardwareBuffer_release(b);
  received++;
 }
 return NULL;
}
int main(void) {
 setbuf(stdout,NULL); alarm(15);
 if(socketpair(AF_UNIX,SOCK_STREAM,0,sockets)) return 2;
 pthread_t t; if(pthread_create(&t,NULL,receiver,NULL)) return 3;
 for(int i=0;i<128;i++) {
  AHardwareBuffer_Desc d={.width=64,.height=64,.layers=1,
   .format=AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
   .usage=AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE|AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT};
  AHardwareBuffer *b=NULL;
  int r=AHardwareBuffer_allocate(&d,&b);
  if(r || !b) { printf("ALLOC_FAIL %d %d\n",i,r); return 4; }
  r=AHardwareBuffer_sendHandleToUnixSocket(b,sockets[0]);
  AHardwareBuffer_release(b);
  if(r) { printf("SEND_FAIL %d %d\n",i,r); return 5; }
  if(!(i%16)) printf("SENT %d RECEIVED %d\n",i+1,(int)received);
 }
 pthread_join(t,NULL);
 printf("AHB_IMPORT_RELEASE=%s FRAMES=%d\n",!err&&received==128?"PASS":"FAIL",(int)received);
 return err || received!=128;
}
