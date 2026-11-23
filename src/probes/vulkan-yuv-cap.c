#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
 VkApplicationInfo a={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="ARM13 YUV capability probe",.apiVersion=VK_API_VERSION_1_1};
 VkInstanceCreateInfo ci={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&a};
 VkInstance i;VkResult r=vkCreateInstance(&ci,NULL,&i);if(r){printf("INSTANCE=%d\n",r);return 2;}
 uint32_t n=1;VkPhysicalDevice p;r=vkEnumeratePhysicalDevices(i,&n,&p);if(r||!n)return 3;
 VkPhysicalDeviceProperties prop;vkGetPhysicalDeviceProperties(p,&prop);printf("DEVICE=%s\n",prop.deviceName);
 uint32_t ecount=0;vkEnumerateDeviceExtensionProperties(p,NULL,&ecount,NULL);
 VkExtensionProperties *e=calloc(ecount,sizeof(*e));vkEnumerateDeviceExtensionProperties(p,NULL,&ecount,e);
 int ext=0;for(uint32_t j=0;j<ecount;j++)if(!strcmp(e[j].extensionName,"VK_ANDROID_external_format_resolve"))ext=1;
 printf("ANDROID_EXTERNAL_FORMAT_RESOLVE=%d\n",ext);free(e);
 VkFormat formats[2]={VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_G8_B8R8_2PLANE_420_UNORM};
 for(int j=0;j<2;j++){
  VkFormatProperties f;vkGetPhysicalDeviceFormatProperties(p,formats[j],&f);
  VkImageFormatProperties ip;
  r=vkGetPhysicalDeviceImageFormatProperties(p,formats[j],VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,0,&ip);
  printf("FORMAT=%s SAMPLED=%d COLOR_ATTACHMENT=%d IMAGE_QUERY=%d\n",j?"NV12":"RGBA8",!!(f.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT),!!(f.optimalTilingFeatures&VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT),r);
 }
 vkDestroyInstance(i,NULL);return 0;
}
