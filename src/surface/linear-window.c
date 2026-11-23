#include <jni.h>
#include <stdint.h>
#include <dlfcn.h>
#include <android/native_window_jni.h>
#include <apex/window.h>
static int repack(ANativeWindow*w,ANativeWindow_performFn original,int op,...){va_list a;va_start(a,op);int r=original(w,op,a);va_end(a);return r;}
static int linear_usage(ANativeWindow*w,ANativeWindow_performFn original,void*data,int op,va_list args){
 (void)data;
 if(op==ANATIVEWINDOW_PERFORM_SET_USAGE64){uint64_t usage=va_arg(args,uint64_t);return repack(w,original,op,usage|UINT64_C(3));}
 if(op==ANATIVEWINDOW_PERFORM_SET_USAGE){int usage=va_arg(args,int);return repack(w,original,op,usage|3);}
 return original(w,op,args);
}
JNIEXPORT jint JNICALL Java_SurfaceCodecProbe_forceLinear(JNIEnv*e,jclass cls,jobject surface){
 (void)cls;ANativeWindow*w=ANativeWindow_fromSurface(e,surface);if(!w)return -22;
 void*lib=dlopen("libnativewindow.so",RTLD_NOW|RTLD_LOCAL);if(!lib){ANativeWindow_release(w);return -38;}
 int(*set)(ANativeWindow*,uint64_t)=dlsym(lib,"ANativeWindow_setUsage");
 int(*hook)(ANativeWindow*,ANativeWindow_performInterceptor,void*)=dlsym(lib,"ANativeWindow_setPerformInterceptor");
 int r=hook&&set?hook(w,linear_usage,0):-38;if(!r)r=set(w,0x303);dlclose(lib);ANativeWindow_release(w);return r;
}

JNIEXPORT jint JNICALL Java_com_genymobile_scrcpy_video_LinearEncoderSupport_forceLinear(JNIEnv*e,jclass c,jobject s){return Java_SurfaceCodecProbe_forceLinear(e,c,s);}
