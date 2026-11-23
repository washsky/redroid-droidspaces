#define _GNU_SOURCE
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
static const char *selected(const char *name,const char *value){return name&&!strcmp(name,"ro.hardware.vulkan")?"adreno":value;}
int __system_property_get(const char *name,char *value){
 if(name&&!strcmp(name,"ro.hardware.vulkan")){memcpy(value,"adreno",7);return 6;}
 int(*next)(const char*,char*)=dlsym(RTLD_NEXT,"__system_property_get");return next?next(name,value):0;
}
struct context {void(*callback)(void*,const char*,const char*,uint32_t);void*cookie;};
static void callback(void *cookie,const char *name,const char *value,uint32_t serial){struct context*c=cookie;c->callback(c->cookie,name,selected(name,value),serial);}
void __system_property_read_callback(const prop_info*pi,void(*cb)(void*,const char*,const char*,uint32_t),void*cookie){
 void(*next)(const prop_info*,void(*)(void*,const char*,const char*,uint32_t),void*)=dlsym(RTLD_NEXT,"__system_property_read_callback");
 struct context c={cb,cookie};if(next)next(pi,callback,&c);
}
