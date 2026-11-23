/* SPDX-License-Identifier: MIT
 * Standard IMapper4 metadata path adapted from Mesa u_gralloc_imapper4_api.cpp.
 * Roman Stratiienko / GlobalLogic / NXP (2021-2026).
 */
#include "arm13_gralloc_bridge.h"
#include <gralloctypes/Gralloc4.h>
#include <android/hardware/graphics/mapper/4.0/IMapper.h>
#include <errno.h>
#include <limits.h>
#include <vector>
using android::hardware::graphics::mapper::V4_0::IMapper;
using android::hardware::graphics::mapper::V4_0::Error;
using android::hardware::hidl_vec;
using aidl::android::hardware::graphics::common::PlaneLayout;
static int metadata(const android::sp<IMapper> &mapper, const native_handle_t *h,
                    const IMapper::MetadataType &type, hidl_vec<uint8_t> *out) {
    Error err = Error::NO_RESOURCES;
    auto ret = mapper->get(const_cast<native_handle_t *>(h), type,
                          [&](Error e, const hidl_vec<uint8_t> &v) { err = e; *out = v; });
    return ret.isOk() && err == Error::NONE ? 0 : -EINVAL;
}
extern "C" __attribute__((visibility("default")))
int arm13_gralloc_query_v1(const native_handle_t *h, arm13_gralloc_info *out) {
    if (!h || !out) return -EINVAL;
    static android::sp<IMapper> mapper = IMapper::getService();
    if (!mapper) return -ENODEV;
    arm13_gralloc_info info = {};
    hidl_vec<uint8_t> encoded;
    using namespace android::gralloc4;
    int err;
    if ((err = metadata(mapper,h,MetadataType_PixelFormatFourCC,&encoded))) return err;
    if ((err = decodePixelFormatFourCC(encoded,&info.drm_fourcc))) return err;
    if ((err = metadata(mapper,h,MetadataType_PixelFormatModifier,&encoded))) return err;
    if ((err = decodePixelFormatModifier(encoded,&info.modifier))) return err;
    if ((err = metadata(mapper,h,MetadataType_AllocationSize,&encoded))) return err;
    if ((err = decodeAllocationSize(encoded,&info.alloc_size))) return err;
    if ((err = metadata(mapper,h,MetadataType_LayerCount,&encoded))) return err;
    if ((err = decodeLayerCount(encoded,&info.layer_count))) return err;
    if ((err = metadata(mapper,h,MetadataType_PlaneLayouts,&encoded))) return err;
    std::vector<PlaneLayout> layouts;
    if ((err = decodePlaneLayouts(encoded,&layouts))) return err;
    if (layouts.empty() || layouts.size() > 4) return -EINVAL;
    info.num_planes = layouts.size();
    int fd_index = 0;
    for (size_t i = 0; i < layouts.size(); ++i) {
        auto &plane = layouts[i];
        if (plane.offsetInBytes < 0 || plane.offsetInBytes > INT_MAX ||
            plane.strideInBytes < 0 || plane.strideInBytes > INT_MAX) return -EOVERFLOW;
        if (plane.offsetInBytes == 0 && i > 0) ++fd_index;
        if (fd_index >= h->numFds) return -EINVAL;
        info.fds[i] = h->data[fd_index];
        info.offsets[i] = plane.offsetInBytes;
        info.strides[i] = plane.strideInBytes;
    }
    *out = info;
    return 0;
}
