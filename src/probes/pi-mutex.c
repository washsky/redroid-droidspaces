#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <sys/wait.h>
static pthread_mutex_t m;
static unsigned count;
static _Atomic int errors;
static void *work(void *unused) {
 (void)unused;
 for(int i=0;i<20000;i++) {
  int r=pthread_mutex_lock(&m);
  if(r){errors=r;return NULL;}
  count++;
  r=pthread_mutex_unlock(&m);
  if(r){errors=r;return NULL;}
 }
 return NULL;
}
int main(void) {
 setbuf(stdout,NULL);alarm(15);
 int (*setprotocol)(pthread_mutexattr_t*,int)=dlsym(RTLD_DEFAULT,"pthread_mutexattr_setprotocol");
 if(!setprotocol)return 2;
 pthread_mutexattr_t a;pthread_mutexattr_init(&a);
 int r=setprotocol(&a,1);
 printf("SET_PI=%d\n",r);if(r)return 3;
 r=pthread_mutex_init(&m,&a);pthread_mutexattr_destroy(&a);if(r)return 4;
 pthread_t threads[4];
 for(int i=0;i<4;i++){r=pthread_create(&threads[i],NULL,work,NULL);if(r)return 5;}
 for(int i=0;i<4;i++)pthread_join(threads[i],NULL);
 printf("PI_CONTENTION=%s COUNT=%u ERR=%d\n",count==80000&&!errors?"PASS":"FAIL",count,(int)errors);
 pid_t p=fork();if(p==0){alarm(5);work(NULL);_exit(errors?7:0);}
 int status=0;waitpid(p,&status,0);
 printf("PI_POST_FORK=%s STATUS=%d\n",WIFEXITED(status)&&WEXITSTATUS(status)==0?"PASS":"FAIL",status);
 pthread_mutex_destroy(&m);
 return errors||count!=80000||status;
}
