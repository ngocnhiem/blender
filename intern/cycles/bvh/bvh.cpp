/* SPDX-FileCopyrightText: 2009-2010 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted code from NVIDIA Corporation. */

#include "bvh/bvh.h"

#include "bvh/bvh2.h"
#include "bvh/multi.h"

#ifdef WITH_EMBREE
#  include "bvh/embree.h"
#endif
#ifdef WITH_HIPRT
#  include "bvh/hiprt.h"
#endif
#ifdef WITH_METAL
#  include "bvh/metal.h"
#endif
#ifdef WITH_OPTIX
#  include "bvh/optix.h"
#endif

#include "util/log.h"

CCL_NAMESPACE_BEGIN

/* BVH Parameters. */

const char *bvh_layout_name(BVHLayout layout)
{
  switch (layout) {
    case BVH_LAYOUT_NONE:
      return "NONE";
    case BVH_LAYOUT_BVH2:
      return "BVH2";
    case BVH_LAYOUT_EMBREE:
      return "EMBREE";
    case BVH_LAYOUT_OPTIX:
      return "OPTIX";
    case BVH_LAYOUT_METAL:
      return "METAL";
    case BVH_LAYOUT_HIPRT:
      return "HIPRT";
    case BVH_LAYOUT_EMBREEGPU:
      return "EMBREEGPU";
    case BVH_LAYOUT_MULTI_OPTIX:
    case BVH_LAYOUT_MULTI_METAL:
    case BVH_LAYOUT_MULTI_HIPRT:
    case BVH_LAYOUT_MULTI_EMBREEGPU:
    case BVH_LAYOUT_MULTI_OPTIX_EMBREE:
    case BVH_LAYOUT_MULTI_METAL_EMBREE:
    case BVH_LAYOUT_MULTI_HIPRT_EMBREE:
    case BVH_LAYOUT_MULTI_EMBREEGPU_EMBREE:
      return "MULTI";
    case BVH_LAYOUT_ALL:
      return "ALL";
  }
  LOG_DFATAL << "Unsupported BVH layout was passed.";
  return "";
}

BVHLayout BVHParams::best_bvh_layout(BVHLayout requested_layout, BVHLayoutMask supported_layouts)
{
  const BVHLayoutMask requested_layout_mask = (BVHLayoutMask)requested_layout;
  /* Check whether requested layout is supported, if so -- no need to do
   * any extra computation.
   */
  if (supported_layouts & requested_layout_mask) {
    return requested_layout;
  }
  /* Some bit magic to get widest supported BVH layout. */
  /* This is a mask of supported BVH layouts which are narrower than the
   * requested one.
   */
  BVHLayoutMask allowed_layouts_mask = (supported_layouts & (requested_layout_mask - 1));
  /* If the requested layout is not supported, choose from the supported layouts instead. */
  if (allowed_layouts_mask == 0) {
    allowed_layouts_mask = supported_layouts;
  }
  /* We get widest from allowed ones and convert mask to actual layout. */
  const BVHLayoutMask widest_allowed_layout_mask = __bsr((uint32_t)allowed_layouts_mask);
  return (BVHLayout)(1 << widest_allowed_layout_mask);
}

/* BVH */

BVH::BVH(const BVHParams &params_,
         const vector<Geometry *> &geometry_,
         const vector<Object *> &objects_)
    : params(params_), geometry(geometry_), objects(objects_)
{
}

unique_ptr<BVH> BVH::create(const BVHParams &params,
                            const vector<Geometry *> &geometry,
                            const vector<Object *> &objects,
                            Device *device)
{
  switch (params.bvh_layout) {
    case BVH_LAYOUT_BVH2:
      return make_unique<BVH2>(params, geometry, objects);
    case BVH_LAYOUT_EMBREE:
    case BVH_LAYOUT_EMBREEGPU:
#ifdef WITH_EMBREE
      return make_unique<BVHEmbree>(params, geometry, objects);
#else
      break;
#endif
    case BVH_LAYOUT_OPTIX:
#ifdef WITH_OPTIX
      return make_unique<BVHOptiX>(params, geometry, objects, device);
#else
      (void)device;
      break;
#endif
    case BVH_LAYOUT_METAL:
#ifdef WITH_METAL
      return bvh_metal_create(params, geometry, objects, device);
#else
      (void)device;
      break;
#endif
    case BVH_LAYOUT_HIPRT:
#ifdef WITH_HIPRT
      return make_unique<BVHHIPRT>(params, geometry, objects, device);
#else
      (void)device;
      break;
#endif
    case BVH_LAYOUT_MULTI_OPTIX:
    case BVH_LAYOUT_MULTI_METAL:
    case BVH_LAYOUT_MULTI_HIPRT:
    case BVH_LAYOUT_MULTI_EMBREEGPU:
    case BVH_LAYOUT_MULTI_OPTIX_EMBREE:
    case BVH_LAYOUT_MULTI_METAL_EMBREE:
    case BVH_LAYOUT_MULTI_HIPRT_EMBREE:
    case BVH_LAYOUT_MULTI_EMBREEGPU_EMBREE:
      return make_unique<BVHMulti>(params, geometry, objects);
    case BVH_LAYOUT_NONE:
    case BVH_LAYOUT_ALL:
      break;
  }
  LOG_DFATAL << "Requested unsupported BVH layout.";
  return nullptr;
}

CCL_NAMESPACE_END
