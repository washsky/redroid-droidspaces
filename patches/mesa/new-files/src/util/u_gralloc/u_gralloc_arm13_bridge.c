/* Android 13 platform adapter: query real Gralloc4 metadata via libui.
 * No inferred QTI handle layout and no synthesized success results. */
#include "u_gralloc_internal.h"
#include "arm13_gralloc_bridge.h"
#include "drm-uapi/drm_fourcc.h"
#include "util/log.h"
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
struct arm13_gralloc {
   struct u_gralloc base;
   void *library;
   int (*query)(const native_handle_t *, struct arm13_gralloc_info *);
};
static int query_info(struct u_gralloc *base, struct u_gralloc_buffer_handle *h,
                      struct u_gralloc_buffer_basic_info *out) {
   struct arm13_gralloc *gr = (struct arm13_gralloc *)base;
   struct arm13_gralloc_info info = {0};
   int ret = gr->query(h->handle, &info);
   if (ret) {
      mesa_loge("Android13 Gralloc4 metadata query failed: %d (format 0x%x)", ret, h->hal_format);
      return ret;
   }
   /* QTI NV12 UBWC exposes Y/UV payload planes followed by their two
    * compression metadata planes. Vulkan's QCOM_COMPRESSED modifier keeps
    * the metadata implicit: each logical plane starts at its metadata,
    * with rowPitch taken from its payload. All four offsets address the
    * pixel dma-buf, not QTI's separate gralloc bookkeeping fd.
    */
   if (h->hal_format == 0x7fa30c06 &&
       info.drm_fourcc == DRM_FORMAT_NV12 &&
       info.modifier == DRM_FORMAT_MOD_QCOM_COMPRESSED &&
       info.num_planes == 4) {
      if (h->handle->numFds < 1 ||
          info.fds[0] != h->handle->data[0] ||
          info.fds[1] != h->handle->data[0] ||
          info.offsets[2] != 0 || info.offsets[0] <= info.offsets[2] ||
          info.offsets[3] <= info.offsets[0] ||
          info.offsets[1] <= info.offsets[3] ||
          info.strides[0] <= 0 || info.strides[1] != info.strides[0] ||
          info.strides[2] <= 0 || info.strides[3] <= 0 ||
          info.strides[2] >= info.strides[0] ||
          info.strides[3] >= info.strides[1] ||
          (uint64_t)info.offsets[1] >= info.alloc_size) {
         mesa_loge("Unrecognized QTI NV12 UBWC plane topology");
         return -EINVAL;
      }
      info.offsets[0] = info.offsets[2];
      info.offsets[1] = info.offsets[3];
      info.num_planes = 2;
      info.fds[0] = info.fds[1] = h->handle->data[0];
      info.fds[2] = info.fds[3] = -1;
      info.offsets[2] = info.offsets[3] = 0;
      info.strides[2] = info.strides[3] = 0;
   }
   _Static_assert(sizeof(info) == sizeof(*out), "metadata ABI mismatch");
   _Static_assert(offsetof(struct arm13_gralloc_info, strides) ==
                  offsetof(struct u_gralloc_buffer_basic_info, strides), "metadata ABI mismatch");
   memcpy(out, &info, sizeof(info));
   return 0;
}
static int destroy_bridge(struct u_gralloc *base) {
   struct arm13_gralloc *gr = (struct arm13_gralloc *)base;
   dlclose(gr->library);
   free(gr);
   return 0;
}
struct u_gralloc *u_gralloc_arm13_bridge_create(void) {
   void *lib = dlopen("libarm13_mesa_gralloc.so", RTLD_NOW | RTLD_LOCAL);
   if (!lib) return NULL;
   void *query = dlsym(lib, "arm13_gralloc_query_v1");
   if (!query) { dlclose(lib); return NULL; }
   struct arm13_gralloc *gr = calloc(1, sizeof(*gr));
   if (!gr) { dlclose(lib); return NULL; }
   gr->library = lib;
   gr->query = query;
   gr->base.ops.get_buffer_basic_info = query_info;
   gr->base.ops.destroy = destroy_bridge;
   mesa_logi("Using Android13 platform Gralloc4 metadata bridge");
   return &gr->base;
}
