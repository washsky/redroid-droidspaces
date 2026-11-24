#include <sys/types.h>
#include "zygisk.hpp"
#include <android/log.h>
#include <unistd.h>
#include <cstring>
class Arm13ZygiskProbe : public zygisk::ModuleBase {
 JNIEnv *env = nullptr;
 bool target = false;
public:
 void onLoad(zygisk::Api *, JNIEnv *jni) override { env = jni; }
 void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
  const char *name = env->GetStringUTFChars(args->nice_name, nullptr);
  target = name && (!strcmp(name,"lab.arm13.probe") || !strcmp(name,"lab.arm13.gpuprobe"));
  if(target) __android_log_print(ANDROID_LOG_INFO,"Arm13ZygiskProbe","preAppSpecialize process=%s uid=%d pid=%d",name,args->uid,getpid());
  if(name) env->ReleaseStringUTFChars(args->nice_name,name);
 }
 void postAppSpecialize(const zygisk::AppSpecializeArgs *) override {
  if(target) __android_log_print(ANDROID_LOG_INFO,"Arm13ZygiskProbe","postAppSpecialize uid=%d pid=%d",getuid(),getpid());
 }
 void preServerSpecialize(zygisk::ServerSpecializeArgs *) override {
  __android_log_print(ANDROID_LOG_INFO,"Arm13ZygiskProbe","preServerSpecialize pid=%d",getpid());
 }
 void postServerSpecialize(const zygisk::ServerSpecializeArgs *) override {
  __android_log_print(ANDROID_LOG_INFO,"Arm13ZygiskProbe","postServerSpecialize uid=%d pid=%d",getuid(),getpid());
 }
};
REGISTER_ZYGISK_MODULE(Arm13ZygiskProbe)
