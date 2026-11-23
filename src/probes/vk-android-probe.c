#define VK_NO_PROTOTYPES
#include <hardware/hwvulkan.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shaders.h"

#define CHECK(call) do { VkResult result=(call); if(result!=VK_SUCCESS) { \
 fprintf(stderr,"%s returned %d\n",#call,result); exit(1); } } while(0)
#define LOAD_INSTANCE(name) PFN_##name name=(PFN_##name)hal->GetInstanceProcAddr(instance,#name); \
 if(!name) { fprintf(stderr,"Missing %s\n",#name); return 1; }
#define LOAD_DEVICE(name) PFN_##name name=(PFN_##name)vkGetDeviceProcAddr(device,#name); \
 if(!name) { fprintf(stderr,"Missing %s\n",#name); return 1; }

static uint32_t memory_type(VkPhysicalDeviceMemoryProperties *properties,
                            uint32_t bits,VkMemoryPropertyFlags flags) {
    for(uint32_t i=0;i<properties->memoryTypeCount;i++)
        if((bits&(1u<<i)) && (properties->memoryTypes[i].propertyFlags&flags)==flags)
            return i;
    fprintf(stderr,"No memory type for flags %#x\n",flags);
    exit(1);
}
int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    if(argc!=3) return 2;
    void *library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library) { fprintf(stderr,"dlopen: %s\n",dlerror()); return 1; }
    hwvulkan_module_t *module=dlsym(library,HAL_MODULE_INFO_SYM_AS_STR);
    if(!module || strcmp(module->common.id,HWVULKAN_HARDWARE_MODULE_ID)) {
        fprintf(stderr,"Invalid Android Vulkan HAL\n");return 1;
    }
    hwvulkan_device_t *hal=NULL;
    int status=module->common.methods->open(&module->common,HWVULKAN_DEVICE_0,
                                          (hw_device_t **)&hal);
    if(status || !hal) { fprintf(stderr,"HAL open returned %d\n",status);return 1; }
    uint32_t extension_count=0;
    CHECK(hal->EnumerateInstanceExtensionProperties(NULL,&extension_count,NULL));
    VkExtensionProperties *extensions=calloc(extension_count,sizeof(*extensions));
    CHECK(hal->EnumerateInstanceExtensionProperties(NULL,&extension_count,extensions));
    int android_surface=0;
    for(uint32_t i=0;i<extension_count;i++)
        if(!strcmp(extensions[i].extensionName,"VK_KHR_android_surface")) android_surface=1;
    free(extensions);
    VkApplicationInfo application={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName="ReDroid13 KGSL acceptance",.apiVersion=VK_API_VERSION_1_1};
    VkInstanceCreateInfo instance_info={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo=&application};
    VkInstance instance=VK_NULL_HANDLE;
    CHECK(hal->CreateInstance(&instance_info,NULL,&instance));
    LOAD_INSTANCE(vkEnumeratePhysicalDevices)
    LOAD_INSTANCE(vkGetPhysicalDeviceProperties)
    LOAD_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties)
    LOAD_INSTANCE(vkGetPhysicalDeviceMemoryProperties)
    LOAD_INSTANCE(vkEnumerateDeviceExtensionProperties)
    LOAD_INSTANCE(vkCreateDevice)
    LOAD_INSTANCE(vkGetDeviceProcAddr)
    LOAD_INSTANCE(vkDestroyInstance)
    uint32_t physical_count=0;
    CHECK(vkEnumeratePhysicalDevices(instance,&physical_count,NULL));
    if(!physical_count) {fprintf(stderr,"No GPU\n");return 1;}
    VkPhysicalDevice *physical_devices=calloc(physical_count,sizeof(*physical_devices));
    CHECK(vkEnumeratePhysicalDevices(instance,&physical_count,physical_devices));
    VkPhysicalDevice physical=physical_devices[0];
    free(physical_devices);
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical,&properties);
    printf("ANDROID_HAL=PASS GPU=%s VENDOR=%#x DEVICE=%#x API=%u.%u.%u ANDROID_SURFACE=%d\n",
           properties.deviceName,properties.vendorID,properties.deviceID,
           VK_VERSION_MAJOR(properties.apiVersion),VK_VERSION_MINOR(properties.apiVersion),
           VK_VERSION_PATCH(properties.apiVersion),android_surface);
    extension_count=0;
    CHECK(vkEnumerateDeviceExtensionProperties(physical,NULL,&extension_count,NULL));
    extensions=calloc(extension_count,sizeof(*extensions));
    CHECK(vkEnumerateDeviceExtensionProperties(physical,NULL,&extension_count,extensions));
    int ahb=0;
    for(uint32_t i=0;i<extension_count;i++)
        if(!strcmp(extensions[i].extensionName,"VK_ANDROID_external_memory_android_hardware_buffer")) ahb=1;
    printf("ANDROID_HARDWARE_BUFFER_EXTENSION=%d\n",ahb);
    free(extensions);
    uint32_t family_count=0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&family_count,NULL);
    VkQueueFamilyProperties *families=calloc(family_count,sizeof(*families));
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&family_count,families);
    uint32_t family=0;
    while(family<family_count && !(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)) family++;
    free(families);
    if(family==family_count) return 1;
    float priority=1.0f;
    VkDeviceQueueCreateInfo queue_info={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority};
    VkDeviceCreateInfo device_info={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount=1,.pQueueCreateInfos=&queue_info};
    VkDevice device=VK_NULL_HANDLE;
    CHECK(vkCreateDevice(physical,&device_info,NULL,&device));
    LOAD_DEVICE(vkGetDeviceQueue)
    LOAD_DEVICE(vkCreateImage)
    LOAD_DEVICE(vkGetImageMemoryRequirements)
    LOAD_DEVICE(vkAllocateMemory)
    LOAD_DEVICE(vkBindImageMemory)
    LOAD_DEVICE(vkCreateImageView)
    LOAD_DEVICE(vkCreateBuffer)
    LOAD_DEVICE(vkGetBufferMemoryRequirements)
    LOAD_DEVICE(vkBindBufferMemory)
    LOAD_DEVICE(vkCreateRenderPass)
    LOAD_DEVICE(vkCreateFramebuffer)
    LOAD_DEVICE(vkCreateShaderModule)
    LOAD_DEVICE(vkCreatePipelineLayout)
    LOAD_DEVICE(vkCreateGraphicsPipelines)
    LOAD_DEVICE(vkCreateCommandPool)
    LOAD_DEVICE(vkAllocateCommandBuffers)
    LOAD_DEVICE(vkBeginCommandBuffer)
    LOAD_DEVICE(vkCmdBeginRenderPass)
    LOAD_DEVICE(vkCmdBindPipeline)
    LOAD_DEVICE(vkCmdDraw)
    LOAD_DEVICE(vkCmdEndRenderPass)
    LOAD_DEVICE(vkCmdPipelineBarrier)
    LOAD_DEVICE(vkCmdCopyImageToBuffer)
    LOAD_DEVICE(vkEndCommandBuffer)
    LOAD_DEVICE(vkCreateFence)
    LOAD_DEVICE(vkQueueSubmit)
    LOAD_DEVICE(vkWaitForFences)
    LOAD_DEVICE(vkMapMemory)
    LOAD_DEVICE(vkUnmapMemory)
    LOAD_DEVICE(vkDeviceWaitIdle)
    LOAD_DEVICE(vkDestroyFence)
    LOAD_DEVICE(vkDestroyCommandPool)
    LOAD_DEVICE(vkDestroyPipeline)
    LOAD_DEVICE(vkDestroyPipelineLayout)
    LOAD_DEVICE(vkDestroyShaderModule)
    LOAD_DEVICE(vkDestroyFramebuffer)
    LOAD_DEVICE(vkDestroyRenderPass)
    LOAD_DEVICE(vkDestroyBuffer)
    LOAD_DEVICE(vkDestroyImageView)
    LOAD_DEVICE(vkDestroyImage)
    LOAD_DEVICE(vkFreeMemory)
    LOAD_DEVICE(vkDestroyDevice)
    VkQueue queue;
    vkGetDeviceQueue(device,family,0,&queue);
    VkPhysicalDeviceMemoryProperties memory;
    vkGetPhysicalDeviceMemoryProperties(physical,&memory);
    VkImageCreateInfo image_info={.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType=VK_IMAGE_TYPE_2D,.format=VK_FORMAT_R8G8B8A8_UNORM,
        .extent={64,64,1},.mipLevels=1,.arrayLayers=1,.samples=VK_SAMPLE_COUNT_1_BIT,
        .tiling=VK_IMAGE_TILING_OPTIMAL,
        .usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
    VkImage image;
    CHECK(vkCreateImage(device,&image_info,NULL,&image));
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device,image,&requirements);
    VkMemoryAllocateInfo allocation={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize=requirements.size,
        .memoryTypeIndex=memory_type(&memory,requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    VkDeviceMemory image_memory;
    CHECK(vkAllocateMemory(device,&allocation,NULL,&image_memory));
    CHECK(vkBindImageMemory(device,image,image_memory,0));
    VkImageViewCreateInfo view_info={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image=image,.viewType=VK_IMAGE_VIEW_TYPE_2D,.format=image_info.format,
        .subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
    VkImageView view;
    CHECK(vkCreateImageView(device,&view_info,NULL,&view));
    VkBufferCreateInfo buffer_info={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size=64*64*4,.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    VkBuffer buffer;
    CHECK(vkCreateBuffer(device,&buffer_info,NULL,&buffer));
    vkGetBufferMemoryRequirements(device,buffer,&requirements);
    allocation.allocationSize=requirements.size;
    allocation.memoryTypeIndex=memory_type(&memory,requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkDeviceMemory buffer_memory;
    CHECK(vkAllocateMemory(device,&allocation,NULL,&buffer_memory));
    CHECK(vkBindBufferMemory(device,buffer,buffer_memory,0));
    VkAttachmentDescription attachment={.format=image_info.format,.samples=VK_SAMPLE_COUNT_1_BIT,
        .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
    VkAttachmentReference reference={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount=1,.pColorAttachments=&reference};
    VkRenderPassCreateInfo pass_info={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount=1,.pAttachments=&attachment,.subpassCount=1,.pSubpasses=&subpass};
    VkRenderPass render_pass;
    CHECK(vkCreateRenderPass(device,&pass_info,NULL,&render_pass));
    VkFramebufferCreateInfo framebuffer_info={.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass=render_pass,.attachmentCount=1,.pAttachments=&view,.width=64,.height=64,.layers=1};
    VkFramebuffer framebuffer;
    CHECK(vkCreateFramebuffer(device,&framebuffer_info,NULL,&framebuffer));
    VkShaderModuleCreateInfo shader_info={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize=sizeof(vertex_spv),.pCode=vertex_spv};
    VkShaderModule vertex,fragment;
    CHECK(vkCreateShaderModule(device,&shader_info,NULL,&vertex));
    shader_info.codeSize=sizeof(fragment_spv);shader_info.pCode=fragment_spv;
    CHECK(vkCreateShaderModule(device,&shader_info,NULL,&fragment));
    VkPipelineShaderStageCreateInfo stages[2]={
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage=VK_SHADER_STAGE_VERTEX_BIT,.module=vertex,.pName="main"},
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage=VK_SHADER_STAGE_FRAGMENT_BIT,.module=fragment,.pName="main"}
    };
    VkPipelineLayoutCreateInfo layout_info={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkPipelineLayout layout;
    CHECK(vkCreatePipelineLayout(device,&layout_info,NULL,&layout));
    VkPipelineVertexInputStateCreateInfo vertex_input={.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly={.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkViewport viewport={0,0,64,64,0,1};
    VkRect2D scissor={{0,0},{64,64}};
    VkPipelineViewportStateCreateInfo viewport_info={.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount=1,.pViewports=&viewport,.scissorCount=1,.pScissors=&scissor};
    VkPipelineRasterizationStateCreateInfo raster={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode=VK_POLYGON_MODE_FILL,.cullMode=VK_CULL_MODE_NONE,
        .frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,.lineWidth=1.0f};
    VkPipelineMultisampleStateCreateInfo samples={.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples=VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState blend_attachment={.colorWriteMask=0xf};
    VkPipelineColorBlendStateCreateInfo blend={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount=1,.pAttachments=&blend_attachment};
    VkGraphicsPipelineCreateInfo pipeline_info={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount=2,.pStages=stages,.pVertexInputState=&vertex_input,.pInputAssemblyState=&assembly,
        .pViewportState=&viewport_info,.pRasterizationState=&raster,.pMultisampleState=&samples,
        .pColorBlendState=&blend,.layout=layout,.renderPass=render_pass};
    VkPipeline pipeline;
    CHECK(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipeline_info,NULL,&pipeline));
    VkCommandPoolCreateInfo pool_info={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.queueFamilyIndex=family};
    VkCommandPool pool;
    CHECK(vkCreateCommandPool(device,&pool_info,NULL,&pool));
    VkCommandBufferAllocateInfo command_info={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
    VkCommandBuffer command;
    CHECK(vkAllocateCommandBuffers(device,&command_info,&command));
    VkCommandBufferBeginInfo begin_info={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    CHECK(vkBeginCommandBuffer(command,&begin_info));
    VkClearValue clear={.color={{0,0,0,1}}};
    VkRenderPassBeginInfo pass_begin={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass=render_pass,.framebuffer=framebuffer,.renderArea={{0,0},{64,64}},
        .clearValueCount=1,.pClearValues=&clear};
    vkCmdBeginRenderPass(command,&pass_begin,VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    vkCmdDraw(command,3,1,0,0);
    vkCmdEndRenderPass(command);
    VkImageMemoryBarrier image_barrier={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .image=image,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&image_barrier);
    VkBufferImageCopy copy={.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},.imageExtent={64,64,1}};
    vkCmdCopyImageToBuffer(command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
    VkBufferMemoryBarrier buffer_barrier={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT,
        .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .buffer=buffer,.size=VK_WHOLE_SIZE};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
        0,0,NULL,1,&buffer_barrier,0,NULL);
    CHECK(vkEndCommandBuffer(command));
    VkFenceCreateInfo fence_info={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence;
    CHECK(vkCreateFence(device,&fence_info,NULL,&fence));
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&command};
    CHECK(vkQueueSubmit(queue,1,&submit,fence));
    CHECK(vkWaitForFences(device,1,&fence,VK_TRUE,10000000000ULL));
    unsigned char *pixels;
    CHECK(vkMapMemory(device,buffer_memory,0,VK_WHOLE_SIZE,0,(void **)&pixels));
    unsigned a=(16*64+16)*4,b=(48*64+48)*4;
    printf("PIXEL16=%u,%u,%u,%u PIXEL48=%u,%u,%u,%u\n",
        pixels[a],pixels[a+1],pixels[a+2],pixels[a+3],pixels[b],pixels[b+1],pixels[b+2],pixels[b+3]);
    int valid=abs(pixels[a]-66)<=2 && abs(pixels[a+1]-66)<=2 &&
        abs(pixels[b]-193)<=2 && abs(pixels[b+1]-193)<=2 &&
        abs(pixels[a+2]-64)<=2 && pixels[a+3]==255;
    FILE *file=fopen(argv[2],"wb");
    if(!file) {perror("output");return 1;}
    fprintf(file,"P6\n64 64\n255\n");
    for(unsigned i=0;i<64*64;i++) fwrite(pixels+i*4,1,3,file);
    fclose(file);
    vkUnmapMemory(device,buffer_memory);
    CHECK(vkDeviceWaitIdle(device));
    vkDestroyFence(device,fence,NULL);
    vkDestroyCommandPool(device,pool,NULL);
    vkDestroyPipeline(device,pipeline,NULL);
    vkDestroyPipelineLayout(device,layout,NULL);
    vkDestroyShaderModule(device,vertex,NULL);
    vkDestroyShaderModule(device,fragment,NULL);
    vkDestroyFramebuffer(device,framebuffer,NULL);
    vkDestroyRenderPass(device,render_pass,NULL);
    vkDestroyImageView(device,view,NULL);
    vkDestroyImage(device,image,NULL);
    vkDestroyBuffer(device,buffer,NULL);
    vkFreeMemory(device,image_memory,NULL);
    vkFreeMemory(device,buffer_memory,NULL);
    vkDestroyDevice(device,NULL);
    vkDestroyInstance(instance,NULL);
    printf("ANDROID_VULKAN_SHADER_DRAW=%s\n",valid?"PASS":"FAIL");
    return valid?0:1;
}
