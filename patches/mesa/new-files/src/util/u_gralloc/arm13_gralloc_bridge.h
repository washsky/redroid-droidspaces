#pragma once
#include <stdint.h>
#include <cutils/native_handle.h>
#ifdef __cplusplus
extern "C" {
#endif
struct arm13_gralloc_info {
    uint32_t drm_fourcc;
    uint64_t modifier;
    int num_planes;
    int fds[4], offsets[4], strides[4];
    uint64_t alloc_size, layer_count;
};
int arm13_gralloc_query_v1(const native_handle_t *, struct arm13_gralloc_info *);
#ifdef __cplusplus
}
#endif
