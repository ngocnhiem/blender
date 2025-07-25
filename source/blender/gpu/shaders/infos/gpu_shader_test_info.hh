/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "GPU_shader_shared.hh"
#endif

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_test)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
FRAGMENT_OUT(0, uint4, out_test)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_math_test)
FRAGMENT_SOURCE("gpu_math_test.glsl")
ADDITIONAL_INFO(gpu_shader_test)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_compute_1d_test)
LOCAL_GROUP_SIZE(1)
IMAGE(1, SFLOAT_32_32_32_32, write, image1D, img_output)
COMPUTE_SOURCE("gpu_compute_1d_test.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_compute_2d_test)
LOCAL_GROUP_SIZE(1, 1)
IMAGE(1, SFLOAT_32_32_32_32, write, image2D, img_output)
COMPUTE_SOURCE("gpu_compute_2d_test.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_compute_ibo_test)
LOCAL_GROUP_SIZE(1)
STORAGE_BUF(0, write, uint, out_indices[])
COMPUTE_SOURCE("gpu_compute_ibo_test.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_compute_vbo_test)
LOCAL_GROUP_SIZE(1)
STORAGE_BUF(0, write, vec4, out_positions[])
COMPUTE_SOURCE("gpu_compute_vbo_test.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_compute_ssbo_test)
LOCAL_GROUP_SIZE(1)
STORAGE_BUF(0, write, int, data_out[])
COMPUTE_SOURCE("gpu_compute_ssbo_test.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_compute_ssbo_binding_test)
LOCAL_GROUP_SIZE(1)
STORAGE_BUF(0, write, int, data0[])
STORAGE_BUF(1, write, int, data1[])
COMPUTE_SOURCE("gpu_compute_dummy_test.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Push constants. */
GPU_SHADER_CREATE_INFO(gpu_push_constants_base_test)
LOCAL_GROUP_SIZE(1)
STORAGE_BUF(0, write, float, data_out[])
COMPUTE_SOURCE("gpu_push_constants_test.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_push_constants_test)
ADDITIONAL_INFO(gpu_push_constants_base_test)
PUSH_CONSTANT(float, float_in)
PUSH_CONSTANT(float2, vec2_in)
PUSH_CONSTANT(float3, vec3_in)
PUSH_CONSTANT(float4, vec4_in)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Push constants size test. */
GPU_SHADER_CREATE_INFO(gpu_push_constants_128bytes_test)
ADDITIONAL_INFO(gpu_push_constants_test)
PUSH_CONSTANT_ARRAY(float, filler, 20)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_push_constants_256bytes_test)
ADDITIONAL_INFO(gpu_push_constants_128bytes_test)
PUSH_CONSTANT_ARRAY(float, filler2, 32)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_push_constants_512bytes_test)
ADDITIONAL_INFO(gpu_push_constants_256bytes_test)
PUSH_CONSTANT_ARRAY(float, filler3, 64)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_push_constants_8192bytes_test)
ADDITIONAL_INFO(gpu_push_constants_512bytes_test)
PUSH_CONSTANT_ARRAY(float, filler4, 1920)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_buffer_texture_test)
LOCAL_GROUP_SIZE(1)
SAMPLER(0, samplerBuffer, bufferTexture)
STORAGE_BUF(0, write, float, data_out[])
COMPUTE_SOURCE("gpu_buffer_texture_test.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Specialization constants. */

GPU_SHADER_CREATE_INFO(gpu_specialization_constants_base_test)
STORAGE_BUF(0, write, int, data_out[])
SPECIALIZATION_CONSTANT(float, float_in, 2)
SPECIALIZATION_CONSTANT(uint, uint_in, 3)
SPECIALIZATION_CONSTANT(int, int_in, 4)
SPECIALIZATION_CONSTANT(bool, bool_in, true)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_compute_specialization_test)
LOCAL_GROUP_SIZE(1)
ADDITIONAL_INFO(gpu_specialization_constants_base_test)
COMPUTE_SOURCE("gpu_specialization_test.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_graphic_specialization_test)
ADDITIONAL_INFO(gpu_specialization_constants_base_test)
VERTEX_SOURCE("gpu_specialization_test.glsl")
FRAGMENT_SOURCE("gpu_specialization_test.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* EEVEE test. */

GPU_SHADER_CREATE_INFO(eevee_shadow_test)
FRAGMENT_SOURCE("eevee_shadow_test.glsl")
ADDITIONAL_INFO(gpu_shader_test)
ADDITIONAL_INFO(eevee_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_occupancy_test)
FRAGMENT_SOURCE("eevee_occupancy_test.glsl")
ADDITIONAL_INFO(gpu_shader_test)
ADDITIONAL_INFO(eevee_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_gbuffer_normal_test)
FRAGMENT_SOURCE("eevee_gbuffer_normal_test.glsl")
ADDITIONAL_INFO(gpu_shader_test)
ADDITIONAL_INFO(eevee_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_gbuffer_closure_test)
FRAGMENT_SOURCE("eevee_gbuffer_closure_test.glsl")
ADDITIONAL_INFO(gpu_shader_test)
ADDITIONAL_INFO(eevee_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
