/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef TENSORFLOW_LITE_KERNELS_INTERNAL_OPTIMIZED_DEPTHWISECONV_UINT8_3X3_FILTER_H_
#define TENSORFLOW_LITE_KERNELS_INTERNAL_OPTIMIZED_DEPTHWISECONV_UINT8_3X3_FILTER_H_

#include <memory>

#include "profiling/instrumentation.h"
#include "tensorflow/lite/kernels/internal/common.h"
#include "tensorflow/lite/kernels/internal/optimized/depthwiseconv_3x3_filter_common.h"
#include "tensorflow/lite/kernels/internal/reference/depthwiseconv_uint8.h"
#include "tensorflow/lite/kernels/internal/types.h"

namespace tflite {
namespace optimized_ops {
namespace depthwise_conv {

#ifdef USE_NEON
// Lane operations are for clarity and convenience. We want to load and store
// 4 8-bit lanes together. So these are treated much like 32-bit loads and
// 32-bit stores. Stores require 32-bit alignment.

#define vst1_lane_8x4(dst, reg, lane_num)                         \
  TFLITE_DCHECK_EQ(reinterpret_cast<std::uintptr_t>(dst) % 4, 0); \
  vst1_lane_u32(reinterpret_cast<uint32_t*>(dst), reg, lane_num)
#define vst1q_lane_8x4(dst, reg, lane_num)                        \
  TFLITE_DCHECK_EQ(reinterpret_cast<std::uintptr_t>(dst) % 4, 0); \
  vst1q_lane_u32(reinterpret_cast<uint32_t*>(dst), reg, lane_num)

// Important! Most compilation configurations will compile and run without
// reinterpret_cast. Sanitizers may fail silently on lane-loading, with an
// obscure bug or mis-feature probably in unhygienic macro expansion.
#define vld1q_lane_s8x8(src, reg, lane_num) \
  vld1q_lane_u64(reinterpret_cast<const uint64_t*>(src), reg, lane_num)
#define vld1_lane_8x4(src, reg, lane_num) \
  vld1_lane_s32(reinterpret_cast<const int32*>(src), reg, lane_num)
#define vld1q_lane_8x4(src, reg, lane_num) \
  vld1q_lane_s32(reinterpret_cast<const int32*>(src), reg, lane_num)
#define vld1q_dup_s8x4(src) vld1q_dup_s32(reinterpret_cast<const int32*>(src))

#define STR(s) STR_UNEXPANDED(s)
#define STR_UNEXPANDED(s) #s

// Enable for arm64 except for the Nvidia Linux 4 Tegra (L4T) running on
// Jetson TX-2. This compiler does not support the offsetof() macro.
#if defined(__aarch64__) && !defined(GOOGLE_L4T)
#include <stddef.h>

// Represents the number of bytes offset from the start of the
// DepthwiseConvParams struct. This is used in the asm to load parameters.
// Keep these values in sync with the static_asserts below.
#define OFFSET_INPUT_DEPTH 0
#define OFFSET_INPUT_ROW_SIZE 8
#define OFFSET_OUTPUT_DEPTH 16
#define OFFSET_OUTPUT_ROW_SIZE 24
#define OFFSET_FILTER_ROW_SIZE 32
#define OFFSET_INPUT_OFFSET 40
#define OFFSET_OUTPUT_OFFSET 44
#define OFFSET_FILTER_OFFSET 48
#define OFFSET_OUTPUT_MULTIPLIER 52
#define OFFSET_OUTPUT_ACTIVATION_MIN 56
#define OFFSET_OUTPUT_ACTIVATION_MAX 60
#define OFFSET_OUTPUT_RIGHT_SHIFT 64
#define OFFSET_INPUT_WIDTH 68
#define OFFSET_INPUT_HEIGHT 72
#define OFFSET_STRIDE_WIDTH 76
#define OFFSET_STRIDE_HEIGHT 80
#define OFFSET_OUTPUT_WIDTH 84
#define OFFSET_OUTPUT_HEIGHT 88

static_assert(offsetof(DepthwiseConvParams, input_depth) == OFFSET_INPUT_DEPTH,
              "");
static_assert(offsetof(DepthwiseConvParams, input_row_size) ==
                  OFFSET_INPUT_ROW_SIZE,
              "");
static_assert(offsetof(DepthwiseConvParams, output_depth) ==
                  OFFSET_OUTPUT_DEPTH,
              "");
static_assert(offsetof(DepthwiseConvParams, output_row_size) ==
                  OFFSET_OUTPUT_ROW_SIZE,
              "");
static_assert(offsetof(DepthwiseConvParams, filter_row_size) ==
                  OFFSET_FILTER_ROW_SIZE,
              "");
static_assert(offsetof(DepthwiseConvParams, input_offset) ==
                  OFFSET_INPUT_OFFSET,
              "");
static_assert(offsetof(DepthwiseConvParams, output_offset) ==
                  OFFSET_OUTPUT_OFFSET,
              "");
static_assert(offsetof(DepthwiseConvParams, filter_offset) ==
                  OFFSET_FILTER_OFFSET,
              "");
static_assert(offsetof(DepthwiseConvParams, output_multiplier) ==
                  OFFSET_OUTPUT_MULTIPLIER,
              "");
static_assert(offsetof(DepthwiseConvParams, output_activation_min) ==
                  OFFSET_OUTPUT_ACTIVATION_MIN,
              "");
static_assert(offsetof(DepthwiseConvParams, output_activation_max) ==
                  OFFSET_OUTPUT_ACTIVATION_MAX,
              "");
static_assert(offsetof(DepthwiseConvParams, output_right_shift) ==
                  OFFSET_OUTPUT_RIGHT_SHIFT,
              "");
static_assert(offsetof(DepthwiseConvParams, input_width) == OFFSET_INPUT_WIDTH,
              "");
static_assert(offsetof(DepthwiseConvParams, input_height) ==
                  OFFSET_INPUT_HEIGHT,
              "");
static_assert(offsetof(DepthwiseConvParams, stride_width) ==
                  OFFSET_STRIDE_WIDTH,
              "");
static_assert(offsetof(DepthwiseConvParams, stride_height) ==
                  OFFSET_STRIDE_HEIGHT,
              "");
static_assert(offsetof(DepthwiseConvParams, output_width) ==
                  OFFSET_OUTPUT_WIDTH,
              "");
static_assert(offsetof(DepthwiseConvParams, output_height) ==
                  OFFSET_OUTPUT_HEIGHT,
              "");
#endif  // __aarch64__
#endif  // ARM NEON

#ifdef USE_NEON
#if defined(__aarch64__) && !defined(GOOGLE_L4T)
// Dot product ops hard-coded

// Represents the number of bytes offset from the start of the
// DepthwiseConvDotProdParams struct. This is used in the asm to load
// parameters. Keep these values in sync with the static_asserts below.

#define DP_OFFSET_INPUT_DEPTH 0
#define DP_OFFSET_OUTPUT_DEPTH DP_OFFSET_INPUT_DEPTH + 8
#define DP_OFFSET_STRIDE DP_OFFSET_OUTPUT_DEPTH + 8
#define DP_OFFSET_BIAS_INCREMENT DP_OFFSET_STRIDE + 4
//
#define DP_OFFSET_INPUT_OFFSET 24
#define DP_OFFSET_OUTPUT_OFFSET DP_OFFSET_INPUT_OFFSET + 4
#define DP_OFFSET_OUTPUT_MULTIPLIER DP_OFFSET_OUTPUT_OFFSET + 4
#define DP_OFFSET_OUTPUT_SHIFT DP_OFFSET_OUTPUT_MULTIPLIER + 4
#define DP_OFFSET_QUANTIZED_ACTIVATION_MIN DP_OFFSET_OUTPUT_SHIFT + 4
#define DP_OFFSET_QUANTIZED_ACTIVATION_MAX \
  DP_OFFSET_QUANTIZED_ACTIVATION_MIN + 4
//
#define DP_OFFSET_PADDING_LEFT 48
#define DP_OFFSET_PADDING_RIGHT DP_OFFSET_PADDING_LEFT + 4
#define DP_OFFSET_PADDING_TOP DP_OFFSET_PADDING_RIGHT + 4
#define DP_OFFSET_PADDING_BOTTOM DP_OFFSET_PADDING_TOP + 4
//
#define DP_OFFSET_DEPTH_MICRO_REPEATS DP_OFFSET_PADDING_BOTTOM + 4
//
#define DP_OFFSET_WIDTH_MACRO_COUNT 68
#define DP_OFFSET_INPUT_WIDTH_OVERALL_MICRO_REPEATS \
  DP_OFFSET_WIDTH_MACRO_COUNT + 4
#define DP_OFFSET_INPUT_WIDTH_MICRO_REPEATS \
  DP_OFFSET_INPUT_WIDTH_OVERALL_MICRO_REPEATS + 4
#define DP_OFFSET_RESIDUAL_WIDTH DP_OFFSET_INPUT_WIDTH_MICRO_REPEATS + 4
#define DP_OFFSET_OUTPUT_WIDTH_OVERALL_MICRO_REPEATS \
  DP_OFFSET_RESIDUAL_WIDTH + 4
#define DP_OFFSET_OUTPUT_WIDTH_MICRO_REPEATS \
  DP_OFFSET_OUTPUT_WIDTH_OVERALL_MICRO_REPEATS + 4
#define DP_OFFSET_OUTPUT_RESIDUAL_WIDTH DP_OFFSET_OUTPUT_WIDTH_MICRO_REPEATS + 4
#define DP_OFFSET_WORKSPACE_WIDTH_MICRO_REPEATS \
  DP_OFFSET_OUTPUT_RESIDUAL_WIDTH + 4
//
#define DP_OFFSET_HEIGHT_MACRO_COUNT 100
#define DP_OFFSET_INBOUND_BLOCK_HEIGHT DP_OFFSET_HEIGHT_MACRO_COUNT + 4
#define DP_OFFSET_OUTBOUND_BLOCK_HEIGHT DP_OFFSET_INBOUND_BLOCK_HEIGHT + 4
#define DP_OFFSET_INPUT_HEIGHT_STRIDE DP_OFFSET_OUTBOUND_BLOCK_HEIGHT + 4
#define DP_OFFSET_OUTPUT_HEIGHT_STRIDE DP_OFFSET_INPUT_HEIGHT_STRIDE + 4
#define DP_OFFSET_WORKSPACE_HEIGHT_STRIDE DP_OFFSET_OUTPUT_HEIGHT_STRIDE + 4
//
#define DP_OFFSET_FOUR_OVER_STRIDE DP_OFFSET_WORKSPACE_HEIGHT_STRIDE + 4

static_assert(offsetof(DepthwiseConvDotProdParams, input_depth) ==
                  DP_OFFSET_INPUT_DEPTH,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, output_depth) ==
                  DP_OFFSET_OUTPUT_DEPTH,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, stride) == DP_OFFSET_STRIDE,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, bias_increment) ==
                  DP_OFFSET_BIAS_INCREMENT,
              "");
//
static_assert(offsetof(DepthwiseConvDotProdParams, input_offset) ==
                  DP_OFFSET_INPUT_OFFSET,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, output_offset) ==
                  DP_OFFSET_OUTPUT_OFFSET,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, output_multiplier) ==
                  DP_OFFSET_OUTPUT_MULTIPLIER,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, output_shift) ==
                  DP_OFFSET_OUTPUT_SHIFT,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, quantized_activation_min) ==
                  DP_OFFSET_QUANTIZED_ACTIVATION_MIN,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, quantized_activation_max) ==
                  DP_OFFSET_QUANTIZED_ACTIVATION_MAX,
              "");
//
static_assert(offsetof(DepthwiseConvDotProdParams, padding_left) ==
                  DP_OFFSET_PADDING_LEFT,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, padding_right) ==
                  DP_OFFSET_PADDING_RIGHT,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, padding_top) ==
                  DP_OFFSET_PADDING_TOP,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, padding_bottom) ==
                  DP_OFFSET_PADDING_BOTTOM,
              "");
//
static_assert(offsetof(DepthwiseConvDotProdParams, depth_micro_repeats) ==
                  DP_OFFSET_DEPTH_MICRO_REPEATS,
              "");
//
static_assert(offsetof(DepthwiseConvDotProdParams, width_macro_count) ==
                  DP_OFFSET_WIDTH_MACRO_COUNT,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams,
                       input_width_overall_micro_repeats) ==
                  DP_OFFSET_INPUT_WIDTH_OVERALL_MICRO_REPEATS,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, input_width_micro_repeats) ==
                  DP_OFFSET_INPUT_WIDTH_MICRO_REPEATS,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, residual_width) ==
                  DP_OFFSET_RESIDUAL_WIDTH,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams,
                       output_width_overall_micro_repeats) ==
                  DP_OFFSET_OUTPUT_WIDTH_OVERALL_MICRO_REPEATS,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams,
                       output_width_micro_repeats) ==
                  DP_OFFSET_OUTPUT_WIDTH_MICRO_REPEATS,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, output_residual_width) ==
                  DP_OFFSET_OUTPUT_RESIDUAL_WIDTH,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams,
                       workspace_width_micro_repeats) ==
                  DP_OFFSET_WORKSPACE_WIDTH_MICRO_REPEATS,
              "");
//
static_assert(offsetof(DepthwiseConvDotProdParams, height_macro_count) ==
                  DP_OFFSET_HEIGHT_MACRO_COUNT,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, inbound_block_height) ==
                  DP_OFFSET_INBOUND_BLOCK_HEIGHT,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, outbound_block_height) ==
                  DP_OFFSET_OUTBOUND_BLOCK_HEIGHT,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, input_height_stride) ==
                  DP_OFFSET_INPUT_HEIGHT_STRIDE,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, output_height_stride) ==
                  DP_OFFSET_OUTPUT_HEIGHT_STRIDE,
              "");
static_assert(offsetof(DepthwiseConvDotProdParams, workspace_height_stride) ==
                  DP_OFFSET_WORKSPACE_HEIGHT_STRIDE,
              "");
//
static_assert(offsetof(DepthwiseConvDotProdParams, four_over_stride) ==
                  DP_OFFSET_FOUR_OVER_STRIDE,
              "");
#endif  // __aarch64__ && !GOOGLE_L4T - Dot product ops hard-coded

#if defined(__aarch64__) && !defined(GOOGLE_L4T)

template <>
struct DepthwiseConvWindow<DepthwiseConvOutputRounding::kAwayFromZero, 8, 1,
                           1> {
 public:
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         int64_t input_depth, int64_t input_row_size,
                         int32 output_window_height, int32 output_window_width,
                         const DepthwiseConvParams* params_ptr) {
    const int64_t input_width_increment = 2 * input_depth;
    const int64_t input_height_increment = 2 * input_row_size;
    const int64_t output_height_increment = 2 * params_ptr->output_row_size;

#define DEPTHWISECONV_LABEL_HEIGHT_2_LOOP "1"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP "2"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "3"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER "4"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP "5"
#define DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP "6"
#define DEPTHWISECONV_LABEL_HEIGHT_1 "7"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP "8"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "9"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER "10"
#define DEPTHWISECONV_LABEL_HEIGHT_1_END "11"

    asm volatile(
        // Performs depthwise convolutions for a window specified by
        // |output_window_height| and |output_window_width|. The inner-most loop
        // processes 2x2 outputs, and any leftovers at the end.
        //
        // Algorithm works as follows:
        //
        //   1. Load filters of 8 depth (8x3x3). Registers v0--v8 hold filter
        //      values.
        //   2. For 2 output heights at a time:
        //        i.  For 2 output widths at a time, load inputs for a 2x1 (2
        //            height, 1 width) output window (4x3 input window).
        //            Registers v9--v20 hold input values. Mul-add with
        //            accumulators v21--v24. Then run activation, downquantize
        //            and store. Repeat for the next 2x1 output window,
        //            leveraging overlapping inputs.
        //        ii. Handle single leftover width if exists.
        //   3. Handle single leftover height if exists.
        //        i.  For 2 output widths at a time, load inputs for a 1x2 (1
        //            height, 2 width) output window (3x4 input window).
        //            Registers v9--v20 hold input values. Mul-add with
        //            accumulators v21--v24. Then run activation, downquantize
        //            and store. Repeat for the next 1x2 output window,
        //            leveraging overlapping inputs.
        //        ii. Handle single leftover width if exists.
        //
        // Loads are placed as soon as the register is no longer needed and
        // interleaved with arithmetic operations to take advantage of
        // dual-issue pipelines. We also add input offsets as far from the loads
        // as possible to give loads enough cycles to fetch data from memory.

        // Set "constant" registers. These registers may be replaced with temp
        // values from time to time when there are not enough NEON registers.
        // We use x9--x15 general purpose registers as they are caller-saved
        // temporary registers (see
        // http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf).  // NOLINT
        "ldr w9, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "ldr x3, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"
        "cmp %w[output_window_height], #2\n"
        "dup v26.8h, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "ldr w2, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "dup v29.8h, w2\n"
        "ldr w4, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v30.16b, w4\n"
        "ldr w0, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v31.16b, w0\n"
        "dup v28.4s, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"
        "add x10, %[bias_ptr], #16\n"
        "ldr x1, [%[params_ptr], #" STR(OFFSET_OUTPUT_ROW_SIZE) "]\n"
        "dup v9.8h, w9\n"

        // Load filters and add offsets.
        "ld1 {v0.8b}, [%[filter_ptr]], x3\n"
        "ld1 {v1.8b}, [%[filter_ptr]], x3\n"
        "uaddw v0.8h, v9.8h, v0.8b\n"
        "ld1 {v2.8b}, [%[filter_ptr]], x3\n"
        "uaddw v1.8h, v9.8h, v1.8b\n"
        "ld1 {v3.8b}, [%[filter_ptr]], x3\n"
        "uaddw v2.8h, v9.8h, v2.8b\n"
        "ld1 {v4.8b}, [%[filter_ptr]], x3\n"
        "uaddw v3.8h, v9.8h, v3.8b\n"
        "ld1 {v5.8b}, [%[filter_ptr]], x3\n"
        "uaddw v4.8h, v9.8h, v4.8b\n"
        "ld1 {v6.8b}, [%[filter_ptr]], x3\n"
        "uaddw v5.8h, v9.8h, v5.8b\n"
        "ld1 {v7.8b}, [%[filter_ptr]], x3\n"
        "uaddw v6.8h, v9.8h, v6.8b\n"
        "ld1 {v8.8b}, [%[filter_ptr]], x3\n"
        "uaddw v7.8h, v9.8h, v7.8b\n"
        "uaddw v8.8h, v9.8h, v8.8b\n"

        "blt " DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_HEIGHT_2_LOOP ":\n"
          // This loop processes 2x2 outputs. To avoid register exhaustion,
          // inputs for the left 2 outputs are loaded first, then the right
          // two outputs.
          "mov x11, %[input_ptr]\n"
          "mov x12, x11\n"
          "ld1 {v9.8b}, [x12], %[input_depth]\n"
          "add x13, x11, %[input_row_size]\n"
          "ld1 {v10.8b}, [x12], %[input_depth]\n"
          "add x14, x13, %[input_row_size]\n"
          "ld1 {v11.8b}, [x12], %[input_depth]\n"
          "add x15, x14, %[input_row_size]\n"
          "ld1 {v12.8b}, [x13], %[input_depth]\n"
          "mov w5, %w[output_window_width]\n"
          "ld1 {v13.8b}, [x13], %[input_depth]\n"
          "mov x6, %[output_ptr]\n"
          "ld1 {v14.8b}, [x13], %[input_depth]\n"
          "add x7, %[output_ptr], x1\n"
          "ld1 {v15.8b}, [x14], %[input_depth]\n"
          // The height 2 / width 2 loop loads an extra 2x1 outputs (2 height,
          // 1 width) in anticipation for the next iteration. Make sure
          // |output_window_width| is large enough to handle the additional
          // loads, otherwise jump to specific the appropriate label to handle
          // smaller widths.
          "cmp w5, #2\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "ld1 {v16.8b}, [x14], %[input_depth]\n"
          "uaddw v10.8h, v26.8h, v10.8b\n"
          "ld1 {v17.8b}, [x14], %[input_depth]\n"
          "uaddw v11.8h, v26.8h, v11.8b\n"
          "ld1 {v18.8b}, [x15], %[input_depth]\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "ld1 {v19.8b}, [x15], %[input_depth]\n"
          "uaddw v13.8h, v26.8h, v13.8b\n"
          "ld1 {v20.8b}, [x15], %[input_depth]\n"
          "uaddw v14.8h, v26.8h, v14.8b\n"
          "ld1 {v21.4s}, [%[bias_ptr]]\n"
          "uaddw v15.8h, v26.8h, v15.8b\n"
          "ld1 {v22.4s}, [x10]\n"
          "uaddw v16.8h, v26.8h, v16.8b\n"
          "ld1 {v23.4s}, [%[bias_ptr]]\n"
          "uaddw v17.8h, v26.8h, v17.8b\n"
          "ld1 {v24.4s}, [x10]\n"
          "uaddw v18.8h, v26.8h, v18.8b\n"
          "uaddw v19.8h, v26.8h, v19.8b\n"
          "uaddw v20.8h, v26.8h, v20.8b\n"

          "beq " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER "f\n"
          "cmp w5, #1\n"
          "beq " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "f\n"

          //"loop_%=:\n"
          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP ":\n"
            // Mul-add left outputs.
            "smlal v21.4s, v0.4h, v9.4h\n"
            "subs w5, w5, #2\n"
            "smlal2 v22.4s, v0.8h, v9.8h\n"
            "cmp w5, #3\n"
            "smlal v23.4s, v0.4h, v12.4h\n"
            "ld1 {v9.8b}, [x12]\n"
            "smlal2 v24.4s, v0.8h, v12.8h\n"
            "smlal v21.4s, v1.4h, v10.4h\n"
            "smlal2 v22.4s, v1.8h, v10.8h\n"
            "smlal v23.4s, v1.4h, v13.4h\n"
            "smlal2 v24.4s, v1.8h, v13.8h\n"
            "smlal v21.4s, v2.4h, v11.4h\n"
            "smlal2 v22.4s, v2.8h, v11.8h\n"
            "smlal v23.4s, v2.4h, v14.4h\n"
            "smlal2 v24.4s, v2.8h, v14.8h\n"
            "smlal v21.4s, v3.4h, v12.4h\n"
            "smlal2 v22.4s, v3.8h, v12.8h\n"
            "ld1 {v12.8b}, [x13]\n"
            "smlal v23.4s, v3.4h, v15.4h\n"
            "smlal2 v24.4s, v3.8h, v15.8h\n"
            "smlal v21.4s, v4.4h, v13.4h\n"
            "smlal2 v22.4s, v4.8h, v13.8h\n"
            "smlal v23.4s, v4.4h, v16.4h\n"
            "smlal2 v24.4s, v4.8h, v16.8h\n"
            "smlal v21.4s, v5.4h, v14.4h\n"
            "smlal2 v22.4s, v5.8h, v14.8h\n"
            "smlal v23.4s, v5.4h, v17.4h\n"
            "smlal2 v24.4s, v5.8h, v17.8h\n"
            "smlal v21.4s, v6.4h, v15.4h\n"
            "smlal2 v22.4s, v6.8h, v15.8h\n"
            "ld1 {v15.8b}, [x14]\n"
            "smlal v23.4s, v6.4h, v18.4h\n"
            "smlal2 v24.4s, v6.8h, v18.8h\n"
            "ld1 {v18.8b}, [x15]\n"
            "smlal v21.4s, v7.4h, v16.4h\n"
            "smlal2 v22.4s, v7.8h, v16.8h\n"
            "smlal v23.4s, v7.4h, v19.4h\n"
            "smlal2 v24.4s, v7.8h, v19.8h\n"
            "smlal v21.4s, v8.4h, v17.4h\n"
            "smlal2 v22.4s, v8.8h, v17.8h\n"
            "smlal v23.4s, v8.4h, v20.4h\n"
            "smlal2 v24.4s, v8.8h, v20.8h\n"

            "sqrdmulh v21.4s, v21.4s, v27.4s\n"
            "sqrdmulh v22.4s, v22.4s, v27.4s\n"
            "sqrdmulh v23.4s, v23.4s, v27.4s\n"
            "sqrdmulh v24.4s, v24.4s, v27.4s\n"
            "and v25.16b, v21.16b, v28.16b\n"
            "and v29.16b, v22.16b, v28.16b\n"
            "and v30.16b, v23.16b, v28.16b\n"
            "and v31.16b, v24.16b, v28.16b\n"
            "sshr v25.4s, v25.4s, #31\n"
            "sshr v29.4s, v29.4s, #31\n"
            "sshr v30.4s, v30.4s, #31\n"
            "sshr v31.4s, v31.4s, #31\n"
            "sqadd v21.4s, v21.4s, v25.4s\n"
            "sqadd v22.4s, v22.4s, v29.4s\n"
            "dup v29.8h, w2\n"
            "sqadd v23.4s, v23.4s, v30.4s\n"
            "dup v30.16b, w4\n"
            "sqadd v24.4s, v24.4s, v31.4s\n"
            "dup v31.16b, w0\n"
            "srshl v21.4s, v21.4s, v28.4s\n"
            "srshl v22.4s, v22.4s, v28.4s\n"
            "srshl v23.4s, v23.4s, v28.4s\n"
            "srshl v24.4s, v24.4s, v28.4s\n"
            "sqxtn v21.4h, v21.4s\n"
            "sqxtn2 v21.8h, v22.4s\n"
            "sqxtn v23.4h, v23.4s\n"
            "sqxtn2 v23.8h, v24.4s\n"
            "sqadd v21.8h, v21.8h, v29.8h\n"
            "sqadd v23.8h, v23.8h, v29.8h\n"
            "sqxtun v21.8b, v21.8h\n"
            "sqxtun2 v21.16b, v23.8h\n"
            "ld1 {v22.4s}, [x10]\n"
            "umax v21.16b, v21.16b, v30.16b\n"
            "umin v21.16b, v21.16b, v31.16b\n"
            "ld1 {v24.4s}, [x10]\n"
            "uaddw v9.8h, v26.8h, v9.8b\n"
            "st1 {v21.8b}, [x6], x3\n"
            "uaddw v12.8h, v26.8h, v12.8b\n"
            "mov v23.d[0], v21.d[1]\n"
            "st1 {v23.8b}, [x7], x3\n"
            "uaddw v15.8h, v26.8h, v15.8b\n"
            "ld1 {v21.4s}, [%[bias_ptr]]\n"
            "uaddw v18.8h, v26.8h, v18.8b\n"
            "ld1 {v23.4s}, [%[bias_ptr]]\n"

            // Mul-add right outputs.
            "smlal v21.4s, v0.4h, v10.4h\n"
            "add x11, x11, %[input_width_increment]\n"
            "smlal2 v22.4s, v0.8h, v10.8h\n"
            "mov x12, x11\n"
            "smlal v23.4s, v0.4h, v13.4h\n"
            "add x13, x11, %[input_row_size]\n"
            "smlal2 v24.4s, v0.8h, v13.8h\n"
            "add x14, x13, %[input_row_size]\n"
            "smlal v21.4s, v1.4h, v11.4h\n"
            "add x15, x14, %[input_row_size]\n"
            "smlal2 v22.4s, v1.8h, v11.8h\n"
            "smlal v23.4s, v1.4h, v14.4h\n"
            "smlal2 v24.4s, v1.8h, v14.8h\n"
            "smlal v21.4s, v2.4h, v9.4h\n"
            "smlal2 v22.4s, v2.8h, v9.8h\n"
            "ld1 {v9.8b}, [x12], %[input_depth]\n"
            "smlal v23.4s, v2.4h, v12.4h\n"
            "ld1 {v10.8b}, [x12], %[input_depth]\n"
            "smlal2 v24.4s, v2.8h, v12.8h\n"
            "ld1 {v11.8b}, [x12], %[input_depth]\n"
            "smlal v21.4s, v3.4h, v13.4h\n"
            "smlal2 v22.4s, v3.8h, v13.8h\n"
            "smlal v23.4s, v3.4h, v16.4h\n"
            "smlal2 v24.4s, v3.8h, v16.8h\n"
            "smlal v21.4s, v4.4h, v14.4h\n"
            "smlal2 v22.4s, v4.8h, v14.8h\n"
            "smlal v23.4s, v4.4h, v17.4h\n"
            "smlal2 v24.4s, v4.8h, v17.8h\n"
            "smlal v21.4s, v5.4h, v12.4h\n"
            "smlal2 v22.4s, v5.8h, v12.8h\n"
            "ld1 {v12.8b}, [x13], %[input_depth]\n"
            "smlal v23.4s, v5.4h, v15.4h\n"
            "ld1 {v13.8b}, [x13], %[input_depth]\n"
            "smlal2 v24.4s, v5.8h, v15.8h\n"
            "ld1 {v14.8b}, [x13], %[input_depth]\n"
            "smlal v21.4s, v6.4h, v16.4h\n"
            "smlal2 v22.4s, v6.8h, v16.8h\n"
            "smlal v23.4s, v6.4h, v19.4h\n"
            "smlal2 v24.4s, v6.8h, v19.8h\n"
            "smlal v21.4s, v7.4h, v17.4h\n"
            "smlal2 v22.4s, v7.8h, v17.8h\n"
            "smlal v23.4s, v7.4h, v20.4h\n"
            "smlal2 v24.4s, v7.8h, v20.8h\n"
            "smlal v21.4s, v8.4h, v15.4h\n"
            "smlal2 v22.4s, v8.8h, v15.8h\n"
            "ld1 {v15.8b}, [x14], %[input_depth]\n"
            "smlal v23.4s, v8.4h, v18.4h\n"
            "ld1 {v16.8b}, [x14], %[input_depth]\n"
            "smlal2 v24.4s, v8.8h, v18.8h\n"
            "ld1 {v17.8b}, [x14], %[input_depth]\n"

            "sqrdmulh v21.4s, v21.4s, v27.4s\n"
            "ld1 {v18.8b}, [x15], %[input_depth]\n"
            "sqrdmulh v22.4s, v22.4s, v27.4s\n"
            "ld1 {v19.8b}, [x15], %[input_depth]\n"
            "sqrdmulh v23.4s, v23.4s, v27.4s\n"
            "ld1 {v20.8b}, [x15], %[input_depth]\n"
            "sqrdmulh v24.4s, v24.4s, v27.4s\n"
            "and v25.16b, v21.16b, v28.16b\n"
            "and v29.16b, v22.16b, v28.16b\n"
            "and v30.16b, v23.16b, v28.16b\n"
            "and v31.16b, v24.16b, v28.16b\n"
            "sshr v25.4s, v25.4s, #31\n"
            "sshr v29.4s, v29.4s, #31\n"
            "sshr v30.4s, v30.4s, #31\n"
            "sshr v31.4s, v31.4s, #31\n"
            "sqadd v21.4s, v21.4s, v25.4s\n"
            "sqadd v22.4s, v22.4s, v29.4s\n"
            "dup v29.8h, w2\n"
            "sqadd v23.4s, v23.4s, v30.4s\n"
            "dup v30.16b, w4\n"
            "sqadd v24.4s, v24.4s, v31.4s\n"
            "dup v31.16b, w0\n"
            "srshl v21.4s, v21.4s, v28.4s\n"
            "srshl v22.4s, v22.4s, v28.4s\n"
            "srshl v23.4s, v23.4s, v28.4s\n"
            "srshl v24.4s, v24.4s, v28.4s\n"
            "sqxtn v21.4h, v21.4s\n"
            "sqxtn2 v21.8h, v22.4s\n"
            "sqxtn v23.4h, v23.4s\n"
            "sqxtn2 v23.8h, v24.4s\n"
            "sqadd v21.8h, v21.8h, v29.8h\n"
            "sqadd v23.8h, v23.8h, v29.8h\n"
            "sqxtun v21.8b, v21.8h\n"
            "sqxtun2 v21.16b, v23.8h\n"
            "ld1 {v22.4s}, [x10]\n"
            "umax v21.16b, v21.16b, v30.16b\n"
            "umin v21.16b, v21.16b, v31.16b\n"
            "ld1 {v24.4s}, [x10]\n"
            "uaddw v9.8h, v26.8h, v9.8b\n"
            "st1 {v21.8b}, [x6], x3\n"
            "uaddw v10.8h, v26.8h, v10.8b\n"
            "mov v23.d[0], v21.d[1]\n"
            "st1 {v23.8b}, [x7], x3\n"
            "uaddw v11.8h, v26.8h, v11.8b\n"
            "uaddw v12.8h, v26.8h, v12.8b\n"
            "uaddw v13.8h, v26.8h, v13.8b\n"
            "uaddw v14.8h, v26.8h, v14.8b\n"
            "uaddw v15.8h, v26.8h, v15.8b\n"
            "ld1 {v21.4s}, [%[bias_ptr]]\n"
            "uaddw v16.8h, v26.8h, v16.8b\n"
            "ld1 {v23.4s}, [%[bias_ptr]]\n"
            "uaddw v17.8h, v26.8h, v17.8b\n"
            "uaddw v18.8h, v26.8h, v18.8b\n"
            "uaddw v19.8h, v26.8h, v19.8b\n"
            "uaddw v20.8h, v26.8h, v20.8b\n"

            "bge " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP "b\n"

          // At this point, there will be one of 2 width or 1 width leftover,
          // not both.
          "cmp w5, #2\n"
          "blt " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "f\n"

          // Handle last 2 columns if exists.
          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER ":\n"
          // Mul-add left outputs.
          "smlal v21.4s, v0.4h, v9.4h\n"
          "smlal2 v22.4s, v0.8h, v9.8h\n"
          "smlal v23.4s, v0.4h, v12.4h\n"
          "ld1 {v9.8b}, [x12]\n"
          "smlal2 v24.4s, v0.8h, v12.8h\n"
          "smlal v21.4s, v1.4h, v10.4h\n"
          "smlal2 v22.4s, v1.8h, v10.8h\n"
          "smlal v23.4s, v1.4h, v13.4h\n"
          "smlal2 v24.4s, v1.8h, v13.8h\n"
          "smlal v21.4s, v2.4h, v11.4h\n"
          "smlal2 v22.4s, v2.8h, v11.8h\n"
          "smlal v23.4s, v2.4h, v14.4h\n"
          "smlal2 v24.4s, v2.8h, v14.8h\n"
          "smlal v21.4s, v3.4h, v12.4h\n"
          "smlal2 v22.4s, v3.8h, v12.8h\n"
          "ld1 {v12.8b}, [x13]\n"
          "smlal v23.4s, v3.4h, v15.4h\n"
          "smlal2 v24.4s, v3.8h, v15.8h\n"
          "smlal v21.4s, v4.4h, v13.4h\n"
          "smlal2 v22.4s, v4.8h, v13.8h\n"
          "smlal v23.4s, v4.4h, v16.4h\n"
          "smlal2 v24.4s, v4.8h, v16.8h\n"
          "smlal v21.4s, v5.4h, v14.4h\n"
          "smlal2 v22.4s, v5.8h, v14.8h\n"
          "smlal v23.4s, v5.4h, v17.4h\n"
          "smlal2 v24.4s, v5.8h, v17.8h\n"
          "smlal v21.4s, v6.4h, v15.4h\n"
          "smlal2 v22.4s, v6.8h, v15.8h\n"
          "ld1 {v15.8b}, [x14]\n"
          "smlal v23.4s, v6.4h, v18.4h\n"
          "smlal2 v24.4s, v6.8h, v18.8h\n"
          "ld1 {v18.8b}, [x15]\n"
          "smlal v21.4s, v7.4h, v16.4h\n"
          "smlal2 v22.4s, v7.8h, v16.8h\n"
          "smlal v23.4s, v7.4h, v19.4h\n"
          "smlal2 v24.4s, v7.8h, v19.8h\n"
          "smlal v21.4s, v8.4h, v17.4h\n"
          "smlal2 v22.4s, v8.8h, v17.8h\n"
          "smlal v23.4s, v8.4h, v20.4h\n"
          "smlal2 v24.4s, v8.8h, v20.8h\n"

          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "and v25.16b, v21.16b, v28.16b\n"
          "and v29.16b, v22.16b, v28.16b\n"
          "and v30.16b, v23.16b, v28.16b\n"
          "and v31.16b, v24.16b, v28.16b\n"
          "sshr v25.4s, v25.4s, #31\n"
          "sshr v29.4s, v29.4s, #31\n"
          "sshr v30.4s, v30.4s, #31\n"
          "sshr v31.4s, v31.4s, #31\n"
          "sqadd v21.4s, v21.4s, v25.4s\n"
          "sqadd v22.4s, v22.4s, v29.4s\n"
          "dup v29.8h, w2\n"
          "sqadd v23.4s, v23.4s, v30.4s\n"
          "dup v30.16b, w4\n"
          "sqadd v24.4s, v24.4s, v31.4s\n"
          "dup v31.16b, w0\n"
          "srshl v21.4s, v21.4s, v28.4s\n"
          "srshl v22.4s, v22.4s, v28.4s\n"
          "srshl v23.4s, v23.4s, v28.4s\n"
          "srshl v24.4s, v24.4s, v28.4s\n"
          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "sqxtun2 v21.16b, v23.8h\n"
          "ld1 {v22.4s}, [x10]\n"
          "umax v21.16b, v21.16b, v30.16b\n"
          "umin v21.16b, v21.16b, v31.16b\n"
          "ld1 {v24.4s}, [x10]\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "st1 {v21.8b}, [x6], x3\n"
          "mov v23.d[0], v21.d[1]\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "st1 {v23.8b}, [x7], x3\n"
          "uaddw v15.8h, v26.8h, v15.8b\n"
          "ld1 {v21.4s}, [%[bias_ptr]]\n"
          "uaddw v18.8h, v26.8h, v18.8b\n"
          "ld1 {v23.4s}, [%[bias_ptr]]\n"

          // Mul-add right outputs.
          "smlal v21.4s, v0.4h, v10.4h\n"
          "smlal2 v22.4s, v0.8h, v10.8h\n"
          "smlal v23.4s, v0.4h, v13.4h\n"
          "smlal2 v24.4s, v0.8h, v13.8h\n"
          "smlal v21.4s, v1.4h, v11.4h\n"
          "smlal2 v22.4s, v1.8h, v11.8h\n"
          "smlal v23.4s, v1.4h, v14.4h\n"
          "smlal2 v24.4s, v1.8h, v14.8h\n"
          "smlal v21.4s, v2.4h, v9.4h\n"
          "smlal2 v22.4s, v2.8h, v9.8h\n"
          "smlal v23.4s, v2.4h, v12.4h\n"
          "smlal2 v24.4s, v2.8h, v12.8h\n"
          "smlal v21.4s, v3.4h, v13.4h\n"
          "smlal2 v22.4s, v3.8h, v13.8h\n"
          "smlal v23.4s, v3.4h, v16.4h\n"
          "smlal2 v24.4s, v3.8h, v16.8h\n"
          "smlal v21.4s, v4.4h, v14.4h\n"
          "smlal2 v22.4s, v4.8h, v14.8h\n"
          "smlal v23.4s, v4.4h, v17.4h\n"
          "smlal2 v24.4s, v4.8h, v17.8h\n"
          "smlal v21.4s, v5.4h, v12.4h\n"
          "smlal2 v22.4s, v5.8h, v12.8h\n"
          "smlal v23.4s, v5.4h, v15.4h\n"
          "smlal2 v24.4s, v5.8h, v15.8h\n"
          "smlal v21.4s, v6.4h, v16.4h\n"
          "smlal2 v22.4s, v6.8h, v16.8h\n"
          "smlal v23.4s, v6.4h, v19.4h\n"
          "smlal2 v24.4s, v6.8h, v19.8h\n"
          "smlal v21.4s, v7.4h, v17.4h\n"
          "smlal2 v22.4s, v7.8h, v17.8h\n"
          "smlal v23.4s, v7.4h, v20.4h\n"
          "smlal2 v24.4s, v7.8h, v20.8h\n"
          "smlal v21.4s, v8.4h, v15.4h\n"
          "smlal2 v22.4s, v8.8h, v15.8h\n"
          "smlal v23.4s, v8.4h, v18.4h\n"
          "smlal2 v24.4s, v8.8h, v18.8h\n"

          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "and v25.16b, v21.16b, v28.16b\n"
          "and v29.16b, v22.16b, v28.16b\n"
          "and v30.16b, v23.16b, v28.16b\n"
          "and v31.16b, v24.16b, v28.16b\n"
          "sshr v25.4s, v25.4s, #31\n"
          "sshr v29.4s, v29.4s, #31\n"
          "sshr v30.4s, v30.4s, #31\n"
          "sshr v31.4s, v31.4s, #31\n"
          "sqadd v21.4s, v21.4s, v25.4s\n"
          "sqadd v22.4s, v22.4s, v29.4s\n"
          "dup v29.8h, w2\n"
          "sqadd v23.4s, v23.4s, v30.4s\n"
          "dup v30.16b, w4\n"
          "sqadd v24.4s, v24.4s, v31.4s\n"
          "dup v31.16b, w0\n"
          "srshl v21.4s, v21.4s, v28.4s\n"
          "srshl v22.4s, v22.4s, v28.4s\n"
          "srshl v23.4s, v23.4s, v28.4s\n"
          "srshl v24.4s, v24.4s, v28.4s\n"

          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "sqxtun2 v21.16b, v23.8h\n"
          "umax v21.16b, v21.16b, v30.16b\n"
          "umin v21.16b, v21.16b, v31.16b\n"
          "st1 {v21.8b}, [x6], x3\n"
          "mov v23.d[0], v21.d[1]\n"
          "st1 {v23.8b}, [x7], x3\n"
          "b " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP "f\n"

          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER ":\n"
          "smlal v21.4s, v0.4h, v9.4h\n"
          "smlal2 v22.4s, v0.8h, v9.8h\n"
          "smlal v23.4s, v0.4h, v12.4h\n"
          "smlal2 v24.4s, v0.8h, v12.8h\n"
          "smlal v21.4s, v1.4h, v10.4h\n"
          "smlal2 v22.4s, v1.8h, v10.8h\n"
          "smlal v23.4s, v1.4h, v13.4h\n"
          "smlal2 v24.4s, v1.8h, v13.8h\n"
          "smlal v21.4s, v2.4h, v11.4h\n"
          "smlal2 v22.4s, v2.8h, v11.8h\n"
          "smlal v23.4s, v2.4h, v14.4h\n"
          "smlal2 v24.4s, v2.8h, v14.8h\n"
          "smlal v21.4s, v3.4h, v12.4h\n"
          "smlal2 v22.4s, v3.8h, v12.8h\n"
          "smlal v23.4s, v3.4h, v15.4h\n"
          "smlal2 v24.4s, v3.8h, v15.8h\n"
          "smlal v21.4s, v4.4h, v13.4h\n"
          "smlal2 v22.4s, v4.8h, v13.8h\n"
          "smlal v23.4s, v4.4h, v16.4h\n"
          "smlal2 v24.4s, v4.8h, v16.8h\n"
          "smlal v21.4s, v5.4h, v14.4h\n"
          "smlal2 v22.4s, v5.8h, v14.8h\n"
          "smlal v23.4s, v5.4h, v17.4h\n"
          "smlal2 v24.4s, v5.8h, v17.8h\n"
          "smlal v21.4s, v6.4h, v15.4h\n"
          "smlal2 v22.4s, v6.8h, v15.8h\n"
          "smlal v23.4s, v6.4h, v18.4h\n"
          "smlal2 v24.4s, v6.8h, v18.8h\n"
          "smlal v21.4s, v7.4h, v16.4h\n"
          "smlal2 v22.4s, v7.8h, v16.8h\n"
          "smlal v23.4s, v7.4h, v19.4h\n"
          "smlal2 v24.4s, v7.8h, v19.8h\n"
          "smlal v21.4s, v8.4h, v17.4h\n"
          "smlal2 v22.4s, v8.8h, v17.8h\n"
          "smlal v23.4s, v8.4h, v20.4h\n"
          "smlal2 v24.4s, v8.8h, v20.8h\n"

          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "and v9.16b, v21.16b, v28.16b\n"
          "and v12.16b, v22.16b, v28.16b\n"
          "and v15.16b, v23.16b, v28.16b\n"
          "and v18.16b, v24.16b, v28.16b\n"
          "sshr v9.4s, v9.4s, #31\n"
          "sshr v12.4s, v12.4s, #31\n"
          "sshr v15.4s, v15.4s, #31\n"
          "sshr v18.4s, v18.4s, #31\n"
          "sqadd v21.4s, v21.4s, v9.4s\n"
          "sqadd v22.4s, v22.4s, v12.4s\n"
          "sqadd v23.4s, v23.4s, v15.4s\n"
          "sqadd v24.4s, v24.4s, v18.4s\n"
          "srshl v21.4s, v21.4s, v28.4s\n"
          "srshl v22.4s, v22.4s, v28.4s\n"
          "srshl v23.4s, v23.4s, v28.4s\n"
          "srshl v24.4s, v24.4s, v28.4s\n"
          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "sqxtun2 v21.16b, v23.8h\n"
          "umax v21.16b, v21.16b, v30.16b\n"
          "umin v21.16b, v21.16b, v31.16b\n"
          "st1 {v21.8b}, [x6], x3\n"
          "mov v23.d[0], v21.d[1]\n"
          "st1 {v23.8b}, [x7], x3\n"

          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP ":\n"
          "subs %w[output_window_height], %w[output_window_height], #2\n"
          "add %[input_ptr], %[input_ptr], %[input_height_increment]\n"
          "cmp %w[output_window_height], #2\n"
          "add %[output_ptr], %[output_ptr], %[output_height_increment]\n"
          "bge " DEPTHWISECONV_LABEL_HEIGHT_2_LOOP "b\n"

        DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP ":\n"
        "cmp %w[output_window_height], #1\n"
        "blt " DEPTHWISECONV_LABEL_HEIGHT_1_END "f\n"

        DEPTHWISECONV_LABEL_HEIGHT_1 ":\n"
        "mov x12, %[input_ptr]\n"
        "ld1 {v9.8b}, [x12], %[input_depth]\n"
        "add x13, %[input_ptr], %[input_row_size]\n"
        "ld1 {v10.8b}, [x12], %[input_depth]\n"
        "add x14, x13, %[input_row_size]\n"
        "ld1 {v11.8b}, [x12], %[input_depth]\n"
        "add x15, x14, %[input_row_size]\n"
        "mov w5, %w[output_window_width]\n"
        "ld1 {v13.8b}, [x13], %[input_depth]\n"
        "mov x6, %[output_ptr]\n"
        "ld1 {v14.8b}, [x13], %[input_depth]\n"
        "add x7, %[output_ptr], x1\n"
        "ld1 {v15.8b}, [x13], %[input_depth]\n"
        // The height 1 / width 2 loop loads an extra 1x1 output in anticipation
        // for the next iteration. Make sure |output_window_width| is large
        // enough to handle the additional load, otherwise jump to the
        // appropriate label to handle smaller widths.
        "cmp w5, #2\n"
        "ld1 {v17.8b}, [x14], %[input_depth]\n"
        "ld1 {v18.8b}, [x14], %[input_depth]\n"
        "ld1 {v19.8b}, [x14], %[input_depth]\n"
        "ld1 {v21.4s}, [%[bias_ptr]]\n"
        "ld1 {v22.4s}, [x10]\n"
        "ld1 {v23.4s}, [%[bias_ptr]]\n"
        "ld1 {v24.4s}, [x10]\n"

        "uaddw v9.8h, v26.8h, v9.8b\n"
        "uaddw v10.8h, v26.8h, v10.8b\n"
        "uaddw v11.8h, v26.8h, v11.8b\n"
        "uaddw v13.8h, v26.8h, v13.8b\n"
        "uaddw v14.8h, v26.8h, v14.8b\n"
        "uaddw v15.8h, v26.8h, v15.8b\n"
        "uaddw v17.8h, v26.8h, v17.8b\n"
        "uaddw v18.8h, v26.8h, v18.8b\n"
        "uaddw v19.8h, v26.8h, v19.8b\n"

        "beq " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER "f\n"
        "cmp w5, #1\n"
        "beq " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP ":\n"
          // Load inputs for 3x4 input window which corresponds to a 1x2 output
          // window.
          "smlal v21.4s, v0.4h, v9.4h\n"
          "ld1 {v12.8b}, [x12]\n"
          "smlal2 v22.4s, v0.8h, v9.8h\n"
          "ld1 {v16.8b}, [x13]\n"
          "smlal v23.4s, v0.4h, v10.4h\n"
          "ld1 {v20.8b}, [x14]\n"
          "smlal2 v24.4s, v0.8h, v10.8h\n"
          "subs w5, w5, #2\n"
          "smlal v21.4s, v1.4h, v10.4h\n"
          "cmp w5, #3\n"
          "smlal2 v22.4s, v1.8h, v10.8h\n"
          "add %[input_ptr], %[input_ptr], %[input_width_increment]\n"
          "smlal v23.4s, v1.4h, v11.4h\n"
          "mov x12, %[input_ptr]\n"
          "smlal2 v24.4s, v1.8h, v11.8h\n"
          "ld1 {v9.8b}, [x12], %[input_depth]\n"
          "smlal v21.4s, v2.4h, v11.4h\n"
          "ld1 {v10.8b}, [x12], %[input_depth]\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "smlal2 v22.4s, v2.8h, v11.8h\n"
          "ld1 {v11.8b}, [x12], %[input_depth]\n"
          "add x13, %[input_ptr], %[input_row_size]\n"
          "smlal v23.4s, v2.4h, v12.4h\n"
          "add x14, x13, %[input_row_size]\n"
          "smlal2 v24.4s, v2.8h, v12.8h\n"
          "smlal v21.4s, v3.4h, v13.4h\n"
          "add x15, x14, %[input_row_size]\n"
          "smlal2 v22.4s, v3.8h, v13.8h\n"
          "ld1 {v13.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v3.4h, v14.4h\n"
          "smlal2 v24.4s, v3.8h, v14.8h\n"
          "smlal v21.4s, v4.4h, v14.4h\n"
          "smlal2 v22.4s, v4.8h, v14.8h\n"
          "ld1 {v14.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v4.4h, v15.4h\n"
          "smlal2 v24.4s, v4.8h, v15.8h\n"
          "smlal v21.4s, v5.4h, v15.4h\n"
          "uaddw v16.8h, v26.8h, v16.8b\n"
          "smlal2 v22.4s, v5.8h, v15.8h\n"
          "ld1 {v15.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v5.4h, v16.4h\n"
          "smlal2 v24.4s, v5.8h, v16.8h\n"
          "smlal v21.4s, v6.4h, v17.4h\n"
          "smlal2 v22.4s, v6.8h, v17.8h\n"
          "ld1 {v17.8b}, [x14], %[input_depth]\n"
          "smlal v23.4s, v6.4h, v18.4h\n"
          "smlal2 v24.4s, v6.8h, v18.8h\n"
          "smlal v21.4s, v7.4h, v18.4h\n"
          "smlal2 v22.4s, v7.8h, v18.8h\n"
          "ld1 {v18.8b}, [x14], %[input_depth]\n"
          "smlal v23.4s, v7.4h, v19.4h\n"
          "smlal2 v24.4s, v7.8h, v19.8h\n"
          "smlal v21.4s, v8.4h, v19.4h\n"
          "uaddw v20.8h, v26.8h, v20.8b\n"
          "smlal2 v22.4s, v8.8h, v19.8h\n"
          "ld1 {v19.8b}, [x14], %[input_depth]\n"
          "smlal v23.4s, v8.4h, v20.4h\n"
          "smlal2 v24.4s, v8.8h, v20.8h\n"

          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "and v25.16b, v21.16b, v28.16b\n"
          "and v29.16b, v22.16b, v28.16b\n"
          "and v30.16b, v23.16b, v28.16b\n"
          "and v31.16b, v24.16b, v28.16b\n"
          "sshr v25.4s, v25.4s, #31\n"
          "sshr v29.4s, v29.4s, #31\n"
          "sshr v30.4s, v30.4s, #31\n"
          "sshr v31.4s, v31.4s, #31\n"
          "sqadd v21.4s, v21.4s, v25.4s\n"
          "sqadd v22.4s, v22.4s, v29.4s\n"
          "dup v29.8h, w2\n"
          "sqadd v23.4s, v23.4s, v30.4s\n"
          "dup v30.16b, w4\n"
          "sqadd v24.4s, v24.4s, v31.4s\n"
          "dup v31.16b, w0\n"
          "srshl v21.4s, v21.4s, v28.4s\n"
          "srshl v22.4s, v22.4s, v28.4s\n"
          "srshl v23.4s, v23.4s, v28.4s\n"
          "srshl v24.4s, v24.4s, v28.4s\n"
          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "sqxtun2 v21.16b, v23.8h\n"
          "ld1 {v22.4s}, [x10]\n"
          "umax v21.16b, v21.16b, v30.16b\n"
          "umin v21.16b, v21.16b, v31.16b\n"
          "ld1 {v24.4s}, [x10]\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "st1 {v21.8b}, [%[output_ptr]], x3\n"
          "uaddw v10.8h, v26.8h, v10.8b\n"
          "mov v23.d[0], v21.d[1]\n"
          "st1 {v23.8b}, [%[output_ptr]], x3\n"
          "uaddw v11.8h, v26.8h, v11.8b\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "uaddw v13.8h, v26.8h, v13.8b\n"
          "uaddw v14.8h, v26.8h, v14.8b\n"
          "uaddw v15.8h, v26.8h, v15.8b\n"
          "ld1 {v21.4s}, [%[bias_ptr]]\n"
          "uaddw v16.8h, v26.8h, v16.8b\n"
          "ld1 {v23.4s}, [%[bias_ptr]]\n"
          "uaddw v17.8h, v26.8h, v17.8b\n"
          "uaddw v18.8h, v26.8h, v18.8b\n"
          "uaddw v19.8h, v26.8h, v19.8b\n"
          "uaddw v20.8h, v26.8h, v20.8b\n"

          "bge " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP "b\n"

        // At this point, there will be one of 2 width or 1 width leftover,
        // not both.
        "cmp w5, #2\n"
        "blt " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "f\n"

        // Handle last two horizontal outputs if exists.
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER ":\n"
        "smlal v21.4s, v0.4h, v9.4h\n"
        "ld1 {v12.8b}, [x12], %[input_depth]\n"
        "smlal2 v22.4s, v0.8h, v9.8h\n"
        "ld1 {v16.8b}, [x13], %[input_depth]\n"
        "smlal v23.4s, v0.4h, v10.4h\n"
        "ld1 {v20.8b}, [x14], %[input_depth]\n"
        "smlal2 v24.4s, v0.8h, v10.8h\n"
        "smlal v21.4s, v1.4h, v10.4h\n"
        "smlal2 v22.4s, v1.8h, v10.8h\n"
        "smlal v23.4s, v1.4h, v11.4h\n"
        "smlal2 v24.4s, v1.8h, v11.8h\n"
        "smlal v21.4s, v2.4h, v11.4h\n"
        "uaddw v12.8h, v26.8h, v12.8b\n"
        "smlal2 v22.4s, v2.8h, v11.8h\n"
        "smlal v23.4s, v2.4h, v12.4h\n"
        "smlal2 v24.4s, v2.8h, v12.8h\n"
        "smlal v21.4s, v3.4h, v13.4h\n"
        "smlal2 v22.4s, v3.8h, v13.8h\n"
        "smlal v23.4s, v3.4h, v14.4h\n"
        "smlal2 v24.4s, v3.8h, v14.8h\n"
        "smlal v21.4s, v4.4h, v14.4h\n"
        "smlal2 v22.4s, v4.8h, v14.8h\n"
        "smlal v23.4s, v4.4h, v15.4h\n"
        "smlal2 v24.4s, v4.8h, v15.8h\n"
        "smlal v21.4s, v5.4h, v15.4h\n"
        "uaddw v16.8h, v26.8h, v16.8b\n"
        "smlal2 v22.4s, v5.8h, v15.8h\n"
        "smlal v23.4s, v5.4h, v16.4h\n"
        "smlal2 v24.4s, v5.8h, v16.8h\n"
        "smlal v21.4s, v6.4h, v17.4h\n"
        "smlal2 v22.4s, v6.8h, v17.8h\n"
        "smlal v23.4s, v6.4h, v18.4h\n"
        "smlal2 v24.4s, v6.8h, v18.8h\n"
        "smlal v21.4s, v7.4h, v18.4h\n"
        "smlal2 v22.4s, v7.8h, v18.8h\n"
        "smlal v23.4s, v7.4h, v19.4h\n"
        "smlal2 v24.4s, v7.8h, v19.8h\n"
        "smlal v21.4s, v8.4h, v19.4h\n"
        "uaddw v20.8h, v26.8h, v20.8b\n"
        "smlal2 v22.4s, v8.8h, v19.8h\n"
        "smlal v23.4s, v8.4h, v20.4h\n"
        "smlal2 v24.4s, v8.8h, v20.8h\n"

        "sqrdmulh v21.4s, v21.4s, v27.4s\n"
        "sqrdmulh v22.4s, v22.4s, v27.4s\n"
        "sqrdmulh v23.4s, v23.4s, v27.4s\n"
        "sqrdmulh v24.4s, v24.4s, v27.4s\n"
        "and v25.16b, v21.16b, v28.16b\n"
        "and v29.16b, v22.16b, v28.16b\n"
        "and v30.16b, v23.16b, v28.16b\n"
        "and v31.16b, v24.16b, v28.16b\n"
        "sshr v25.4s, v25.4s, #31\n"
        "sshr v29.4s, v29.4s, #31\n"
        "sshr v30.4s, v30.4s, #31\n"
        "sshr v31.4s, v31.4s, #31\n"
        "sqadd v21.4s, v21.4s, v25.4s\n"
        "sqadd v22.4s, v22.4s, v29.4s\n"
        "dup v29.8h, w2\n"
        "sqadd v23.4s, v23.4s, v30.4s\n"
        "dup v30.16b, w4\n"
        "sqadd v24.4s, v24.4s, v31.4s\n"
        "dup v31.16b, w0\n"
        "srshl v21.4s, v21.4s, v28.4s\n"
        "srshl v22.4s, v22.4s, v28.4s\n"
        "srshl v23.4s, v23.4s, v28.4s\n"
        "srshl v24.4s, v24.4s, v28.4s\n"
        "sqxtn v21.4h, v21.4s\n"
        "sqxtn2 v21.8h, v22.4s\n"
        "sqxtn v23.4h, v23.4s\n"
        "sqxtn2 v23.8h, v24.4s\n"
        "sqadd v21.8h, v21.8h, v29.8h\n"
        "sqadd v23.8h, v23.8h, v29.8h\n"
        "sqxtun v21.8b, v21.8h\n"
        "sqxtun2 v21.16b, v23.8h\n"
        "umax v21.16b, v21.16b, v30.16b\n"
        "umin v21.16b, v21.16b, v31.16b\n"
        "st1 {v21.8b}, [%[output_ptr]], x3\n"
        "mov v23.d[0], v21.d[1]\n"
        "st1 {v23.8b}, [%[output_ptr]], x3\n"
        "b " DEPTHWISECONV_LABEL_HEIGHT_1_END "f\n"

        // Handle bottom right output if exists.
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER ":\n"
        "smlal v21.4s, v0.4h, v9.4h\n"
        "smlal2 v22.4s, v0.8h, v9.8h\n"
        "smlal v21.4s, v1.4h, v10.4h\n"
        "smlal2 v22.4s, v1.8h, v10.8h\n"
        "smlal v21.4s, v2.4h, v11.4h\n"
        "smlal2 v22.4s, v2.8h, v11.8h\n"
        "smlal v21.4s, v3.4h, v13.4h\n"
        "smlal2 v22.4s, v3.8h, v13.8h\n"
        "smlal v21.4s, v4.4h, v14.4h\n"
        "smlal2 v22.4s, v4.8h, v14.8h\n"
        "smlal v21.4s, v5.4h, v15.4h\n"
        "smlal2 v22.4s, v5.8h, v15.8h\n"
        "smlal v21.4s, v6.4h, v17.4h\n"
        "smlal2 v22.4s, v6.8h, v17.8h\n"
        "smlal v21.4s, v7.4h, v18.4h\n"
        "smlal2 v22.4s, v7.8h, v18.8h\n"
        "smlal v21.4s, v8.4h, v19.4h\n"
        "smlal2 v22.4s, v8.8h, v19.8h\n"

        "sqrdmulh v21.4s, v21.4s, v27.4s\n"
        "sqrdmulh v22.4s, v22.4s, v27.4s\n"
        "and v9.16b, v21.16b, v28.16b\n"
        "and v12.16b, v22.16b, v28.16b\n"
        "sshr v9.4s, v9.4s, #31\n"
        "sshr v12.4s, v12.4s, #31\n"
        "sqadd v21.4s, v21.4s, v9.4s\n"
        "sqadd v22.4s, v22.4s, v12.4s\n"
        "srshl v21.4s, v21.4s, v28.4s\n"
        "srshl v22.4s, v22.4s, v28.4s\n"
        "sqxtn v21.4h, v21.4s\n"
        "sqxtn2 v21.8h, v22.4s\n"
        "sqadd v21.8h, v21.8h, v29.8h\n"
        "sqxtun v21.8b, v21.8h\n"
        "umax v21.8b, v21.8b, v30.8b\n"
        "umin v21.8b, v21.8b, v31.8b\n"
        "st1 {v21.8b}, [%[output_ptr]]\n"
        DEPTHWISECONV_LABEL_HEIGHT_1_END ":\n"
    :
    // Outputs.
    [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
    [output_ptr] "+r"(output_ptr),
    [output_window_height] "+r"(output_window_height)
    :
    // Inputs.
    [bias_ptr] "r"(bias_ptr), [input_row_size] "r"(input_row_size),
    [input_depth] "r"(input_depth),
    [output_window_width] "r"(output_window_width),
    [input_width_increment] "r"(input_width_increment),
    [input_height_increment] "r"(input_height_increment),
    [output_height_increment] "r"(output_height_increment),
    [params_ptr] "r"(params_ptr)
    :
    // Clobbers.
    "cc", "memory",
    // We use these NEON registers.
    "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9",
    "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19",
    "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
    "v30", "v31",
    // We use these general-purpose registers.
    "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
    "x9", "x10", "x11", "x12", "x13", "x14", "x15");
#undef DEPTHWISECONV_LABEL_HEIGHT_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_1
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_1_END
  }
};

template <>
struct DepthwiseConvWindow<DepthwiseConvOutputRounding::kUpward, 8, 1, 1> {
 public:
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         int64_t input_depth, int64_t input_row_size,
                         int32 output_window_height, int32 output_window_width,
                         const DepthwiseConvParams* params_ptr) {
    const int64_t input_width_increment = 2 * input_depth;
    const int64_t input_height_increment = 2 * input_row_size;
    const int64_t output_height_increment = 2 * params_ptr->output_row_size;

#define DEPTHWISECONV_LABEL_HEIGHT_2_LOOP "1"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP "2"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "3"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER "4"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP "5"
#define DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP "6"
#define DEPTHWISECONV_LABEL_HEIGHT_1 "7"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP "8"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "9"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER "10"
#define DEPTHWISECONV_LABEL_HEIGHT_1_END "11"

    asm volatile(
        // Performs depthwise convolutions for a window specified by
        // |output_window_height| and |output_window_width|. The inner-most loop
        // processes 2x2 outputs, and any leftovers at the end.
        //
        // Algorithm works as follows:
        //
        //   1. Load filters of 8 depth (8x3x3). Registers v0--v8 hold filter
        //      values.
        //   2. For 2 output heights at a time:
        //        i.  For 2 output widths at a time, load inputs for a 2x1 (2
        //            height, 1 width) output window (4x3 input window).
        //            Registers v9--v20 hold input values. Mul-add with
        //            accumulators v21--v24. Then run activation, downquantize
        //            and store. Repeat for the next 2x1 output window,
        //            leveraging overlapping inputs.
        //        ii. Handle single leftover width if exists.
        //   3. Handle single leftover height if exists.
        //        i.  For 2 output widths at a time, load inputs for a 1x2 (1
        //            height, 2 width) output window (3x4 input window).
        //            Registers v9--v20 hold input values. Mul-add with
        //            accumulators v21--v24. Then run activation, downquantize
        //            and store. Repeat for the next 1x2 output window,
        //            leveraging overlapping inputs.
        //        ii. Handle single leftover width if exists.
        //
        // Loads are placed as soon as the register is no longer needed and
        // interleaved with arithmetic operations to take advantage of
        // dual-issue pipelines. We also add input offsets as far from the loads
        // as possible to give loads enough cycles to fetch data from memory.

        // Set "constant" registers. These registers may be replaced with temp
        // values from time to time when there are not enough NEON registers.
        // We use x9--x15 general purpose registers as they are caller-saved
        // temporary registers (see
        // http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf).  // NOLINT
        "ldr w9, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "ldr x3, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"
        "cmp %w[output_window_height], #2\n"
        "dup v26.8h, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "ldr w2, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "dup v29.8h, w2\n"
        "ldr w4, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v30.16b, w4\n"
        "ldr w0, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v31.16b, w0\n"
        "dup v28.4s, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"
        "add x10, %[bias_ptr], #16\n"
        "ldr x1, [%[params_ptr], #" STR(OFFSET_OUTPUT_ROW_SIZE) "]\n"
        "dup v9.8h, w9\n"

        // Load filters and add offsets.
        "ld1 {v0.8b}, [%[filter_ptr]], x3\n"
        "ld1 {v1.8b}, [%[filter_ptr]], x3\n"
        "uaddw v0.8h, v9.8h, v0.8b\n"
        "ld1 {v2.8b}, [%[filter_ptr]], x3\n"
        "uaddw v1.8h, v9.8h, v1.8b\n"
        "ld1 {v3.8b}, [%[filter_ptr]], x3\n"
        "uaddw v2.8h, v9.8h, v2.8b\n"
        "ld1 {v4.8b}, [%[filter_ptr]], x3\n"
        "uaddw v3.8h, v9.8h, v3.8b\n"
        "ld1 {v5.8b}, [%[filter_ptr]], x3\n"
        "uaddw v4.8h, v9.8h, v4.8b\n"
        "ld1 {v6.8b}, [%[filter_ptr]], x3\n"
        "uaddw v5.8h, v9.8h, v5.8b\n"
        "ld1 {v7.8b}, [%[filter_ptr]], x3\n"
        "uaddw v6.8h, v9.8h, v6.8b\n"
        "ld1 {v8.8b}, [%[filter_ptr]], x3\n"
        "uaddw v7.8h, v9.8h, v7.8b\n"
        "uaddw v8.8h, v9.8h, v8.8b\n"

        "blt " DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_HEIGHT_2_LOOP ":\n"
          // This loop processes 2x2 outputs. To avoid register exhaustion,
          // inputs for the left 2 outputs are loaded first, then the right
          // two outputs.
          "mov x11, %[input_ptr]\n"
          "mov x12, x11\n"
          "ld1 {v9.8b}, [x12], %[input_depth]\n"
          "add x13, x11, %[input_row_size]\n"
          "ld1 {v10.8b}, [x12], %[input_depth]\n"
          "add x14, x13, %[input_row_size]\n"
          "ld1 {v11.8b}, [x12], %[input_depth]\n"
          "add x15, x14, %[input_row_size]\n"
          "ld1 {v12.8b}, [x13], %[input_depth]\n"
          "mov w5, %w[output_window_width]\n"
          "ld1 {v13.8b}, [x13], %[input_depth]\n"
          "mov x6, %[output_ptr]\n"
          "ld1 {v14.8b}, [x13], %[input_depth]\n"
          "add x7, %[output_ptr], x1\n"
          "ld1 {v15.8b}, [x14], %[input_depth]\n"
          // The height 2 / width 2 loop loads an extra 2x1 outputs (2 height,
          // 1 width) in anticipation for the next iteration. Make sure
          // |output_window_width| is large enough to handle the additional
          // loads, otherwise jump to specific the appropriate label to handle
          // smaller widths.
          "cmp w5, #2\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "ld1 {v16.8b}, [x14], %[input_depth]\n"
          "uaddw v10.8h, v26.8h, v10.8b\n"
          "ld1 {v17.8b}, [x14], %[input_depth]\n"
          "uaddw v11.8h, v26.8h, v11.8b\n"
          "ld1 {v18.8b}, [x15], %[input_depth]\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "ld1 {v19.8b}, [x15], %[input_depth]\n"
          "uaddw v13.8h, v26.8h, v13.8b\n"
          "ld1 {v20.8b}, [x15], %[input_depth]\n"
          "uaddw v14.8h, v26.8h, v14.8b\n"
          "ld1 {v21.4s}, [%[bias_ptr]]\n"
          "uaddw v15.8h, v26.8h, v15.8b\n"
          "ld1 {v22.4s}, [x10]\n"
          "uaddw v16.8h, v26.8h, v16.8b\n"
          "ld1 {v23.4s}, [%[bias_ptr]]\n"
          "uaddw v17.8h, v26.8h, v17.8b\n"
          "ld1 {v24.4s}, [x10]\n"
          "uaddw v18.8h, v26.8h, v18.8b\n"
          "uaddw v19.8h, v26.8h, v19.8b\n"
          "uaddw v20.8h, v26.8h, v20.8b\n"

          "beq " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER "f\n"
          "cmp w5, #1\n"
          "beq " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "f\n"

          //"loop_%=:\n"
          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP ":\n"
            // Mul-add left outputs.
            "smlal v21.4s, v0.4h, v9.4h\n"
            "subs w5, w5, #2\n"
            "smlal2 v22.4s, v0.8h, v9.8h\n"
            "cmp w5, #3\n"
            "smlal v23.4s, v0.4h, v12.4h\n"
            "ld1 {v9.8b}, [x12]\n"
            "smlal2 v24.4s, v0.8h, v12.8h\n"
            "smlal v21.4s, v1.4h, v10.4h\n"
            "smlal2 v22.4s, v1.8h, v10.8h\n"
            "smlal v23.4s, v1.4h, v13.4h\n"
            "smlal2 v24.4s, v1.8h, v13.8h\n"
            "smlal v21.4s, v2.4h, v11.4h\n"
            "smlal2 v22.4s, v2.8h, v11.8h\n"
            "smlal v23.4s, v2.4h, v14.4h\n"
            "smlal2 v24.4s, v2.8h, v14.8h\n"
            "smlal v21.4s, v3.4h, v12.4h\n"
            "smlal2 v22.4s, v3.8h, v12.8h\n"
            "ld1 {v12.8b}, [x13]\n"
            "smlal v23.4s, v3.4h, v15.4h\n"
            "smlal2 v24.4s, v3.8h, v15.8h\n"
            "smlal v21.4s, v4.4h, v13.4h\n"
            "smlal2 v22.4s, v4.8h, v13.8h\n"
            "smlal v23.4s, v4.4h, v16.4h\n"
            "smlal2 v24.4s, v4.8h, v16.8h\n"
            "smlal v21.4s, v5.4h, v14.4h\n"
            "smlal2 v22.4s, v5.8h, v14.8h\n"
            "smlal v23.4s, v5.4h, v17.4h\n"
            "smlal2 v24.4s, v5.8h, v17.8h\n"
            "smlal v21.4s, v6.4h, v15.4h\n"
            "smlal2 v22.4s, v6.8h, v15.8h\n"
            "ld1 {v15.8b}, [x14]\n"
            "smlal v23.4s, v6.4h, v18.4h\n"
            "smlal2 v24.4s, v6.8h, v18.8h\n"
            "ld1 {v18.8b}, [x15]\n"
            "smlal v21.4s, v7.4h, v16.4h\n"
            "smlal2 v22.4s, v7.8h, v16.8h\n"
            "smlal v23.4s, v7.4h, v19.4h\n"
            "smlal2 v24.4s, v7.8h, v19.8h\n"
            "smlal v21.4s, v8.4h, v17.4h\n"
            "smlal2 v22.4s, v8.8h, v17.8h\n"
            "smlal v23.4s, v8.4h, v20.4h\n"
            "smlal2 v24.4s, v8.8h, v20.8h\n"

            "sqrdmulh v21.4s, v21.4s, v27.4s\n"
            "sqrdmulh v22.4s, v22.4s, v27.4s\n"
            "sqrdmulh v23.4s, v23.4s, v27.4s\n"
            "sqrdmulh v24.4s, v24.4s, v27.4s\n"
            "sqrshl v21.4s, v21.4s, v28.4s\n"
            "sqrshl v22.4s, v22.4s, v28.4s\n"
            "sqrshl v23.4s, v23.4s, v28.4s\n"
            "sqrshl v24.4s, v24.4s, v28.4s\n"
            "sqxtn v21.4h, v21.4s\n"
            "sqxtn2 v21.8h, v22.4s\n"
            "sqxtn v23.4h, v23.4s\n"
            "sqxtn2 v23.8h, v24.4s\n"
            "sqadd v21.8h, v21.8h, v29.8h\n"
            "sqadd v23.8h, v23.8h, v29.8h\n"
            "sqxtun v21.8b, v21.8h\n"
            "sqxtun2 v21.16b, v23.8h\n"
            "ld1 {v22.4s}, [x10]\n"
            "umax v21.16b, v21.16b, v30.16b\n"
            "umin v21.16b, v21.16b, v31.16b\n"
            "ld1 {v24.4s}, [x10]\n"
            "uaddw v9.8h, v26.8h, v9.8b\n"
            "st1 {v21.8b}, [x6], x3\n"
            "uaddw v12.8h, v26.8h, v12.8b\n"
            "mov v23.d[0], v21.d[1]\n"
            "st1 {v23.8b}, [x7], x3\n"
            "uaddw v15.8h, v26.8h, v15.8b\n"
            "ld1 {v21.4s}, [%[bias_ptr]]\n"
            "uaddw v18.8h, v26.8h, v18.8b\n"
            "ld1 {v23.4s}, [%[bias_ptr]]\n"

            // Mul-add right outputs.
            "smlal v21.4s, v0.4h, v10.4h\n"
            "add x11, x11, %[input_width_increment]\n"
            "smlal2 v22.4s, v0.8h, v10.8h\n"
            "mov x12, x11\n"
            "smlal v23.4s, v0.4h, v13.4h\n"
            "add x13, x11, %[input_row_size]\n"
            "smlal2 v24.4s, v0.8h, v13.8h\n"
            "add x14, x13, %[input_row_size]\n"
            "smlal v21.4s, v1.4h, v11.4h\n"
            "add x15, x14, %[input_row_size]\n"
            "smlal2 v22.4s, v1.8h, v11.8h\n"
            "smlal v23.4s, v1.4h, v14.4h\n"
            "smlal2 v24.4s, v1.8h, v14.8h\n"
            "smlal v21.4s, v2.4h, v9.4h\n"
            "smlal2 v22.4s, v2.8h, v9.8h\n"
            "ld1 {v9.8b}, [x12], %[input_depth]\n"
            "smlal v23.4s, v2.4h, v12.4h\n"
            "ld1 {v10.8b}, [x12], %[input_depth]\n"
            "smlal2 v24.4s, v2.8h, v12.8h\n"
            "ld1 {v11.8b}, [x12], %[input_depth]\n"
            "smlal v21.4s, v3.4h, v13.4h\n"
            "smlal2 v22.4s, v3.8h, v13.8h\n"
            "smlal v23.4s, v3.4h, v16.4h\n"
            "smlal2 v24.4s, v3.8h, v16.8h\n"
            "smlal v21.4s, v4.4h, v14.4h\n"
            "smlal2 v22.4s, v4.8h, v14.8h\n"
            "smlal v23.4s, v4.4h, v17.4h\n"
            "smlal2 v24.4s, v4.8h, v17.8h\n"
            "smlal v21.4s, v5.4h, v12.4h\n"
            "smlal2 v22.4s, v5.8h, v12.8h\n"
            "ld1 {v12.8b}, [x13], %[input_depth]\n"
            "smlal v23.4s, v5.4h, v15.4h\n"
            "ld1 {v13.8b}, [x13], %[input_depth]\n"
            "smlal2 v24.4s, v5.8h, v15.8h\n"
            "ld1 {v14.8b}, [x13], %[input_depth]\n"
            "smlal v21.4s, v6.4h, v16.4h\n"
            "smlal2 v22.4s, v6.8h, v16.8h\n"
            "smlal v23.4s, v6.4h, v19.4h\n"
            "smlal2 v24.4s, v6.8h, v19.8h\n"
            "smlal v21.4s, v7.4h, v17.4h\n"
            "smlal2 v22.4s, v7.8h, v17.8h\n"
            "smlal v23.4s, v7.4h, v20.4h\n"
            "smlal2 v24.4s, v7.8h, v20.8h\n"
            "smlal v21.4s, v8.4h, v15.4h\n"
            "smlal2 v22.4s, v8.8h, v15.8h\n"
            "ld1 {v15.8b}, [x14], %[input_depth]\n"
            "smlal v23.4s, v8.4h, v18.4h\n"
            "ld1 {v16.8b}, [x14], %[input_depth]\n"
            "smlal2 v24.4s, v8.8h, v18.8h\n"
            "ld1 {v17.8b}, [x14], %[input_depth]\n"

            "sqrdmulh v21.4s, v21.4s, v27.4s\n"
            "ld1 {v18.8b}, [x15], %[input_depth]\n"
            "sqrdmulh v22.4s, v22.4s, v27.4s\n"
            "ld1 {v19.8b}, [x15], %[input_depth]\n"
            "sqrdmulh v23.4s, v23.4s, v27.4s\n"
            "ld1 {v20.8b}, [x15], %[input_depth]\n"
            "sqrdmulh v24.4s, v24.4s, v27.4s\n"
            "sqrshl v21.4s, v21.4s, v28.4s\n"
            "sqrshl v22.4s, v22.4s, v28.4s\n"
            "sqrshl v23.4s, v23.4s, v28.4s\n"
            "sqrshl v24.4s, v24.4s, v28.4s\n"
            "sqxtn v21.4h, v21.4s\n"
            "sqxtn2 v21.8h, v22.4s\n"
            "sqxtn v23.4h, v23.4s\n"
            "sqxtn2 v23.8h, v24.4s\n"
            "sqadd v21.8h, v21.8h, v29.8h\n"
            "sqadd v23.8h, v23.8h, v29.8h\n"
            "sqxtun v21.8b, v21.8h\n"
            "sqxtun2 v21.16b, v23.8h\n"
            "ld1 {v22.4s}, [x10]\n"
            "umax v21.16b, v21.16b, v30.16b\n"
            "umin v21.16b, v21.16b, v31.16b\n"
            "ld1 {v24.4s}, [x10]\n"
            "uaddw v9.8h, v26.8h, v9.8b\n"
            "st1 {v21.8b}, [x6], x3\n"
            "uaddw v10.8h, v26.8h, v10.8b\n"
            "mov v23.d[0], v21.d[1]\n"
            "st1 {v23.8b}, [x7], x3\n"
            "uaddw v11.8h, v26.8h, v11.8b\n"
            "uaddw v12.8h, v26.8h, v12.8b\n"
            "uaddw v13.8h, v26.8h, v13.8b\n"
            "uaddw v14.8h, v26.8h, v14.8b\n"
            "uaddw v15.8h, v26.8h, v15.8b\n"
            "ld1 {v21.4s}, [%[bias_ptr]]\n"
            "uaddw v16.8h, v26.8h, v16.8b\n"
            "ld1 {v23.4s}, [%[bias_ptr]]\n"
            "uaddw v17.8h, v26.8h, v17.8b\n"
            "uaddw v18.8h, v26.8h, v18.8b\n"
            "uaddw v19.8h, v26.8h, v19.8b\n"
            "uaddw v20.8h, v26.8h, v20.8b\n"

            "bge " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP "b\n"

          // At this point, there will be one of 2 width or 1 width leftover,
          // not both.
          "cmp w5, #2\n"
          "blt " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "f\n"

          // Handle last 2 columns if exists.
          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER ":\n"
          // Mul-add left outputs.
          "smlal v21.4s, v0.4h, v9.4h\n"
          "smlal2 v22.4s, v0.8h, v9.8h\n"
          "smlal v23.4s, v0.4h, v12.4h\n"
          "ld1 {v9.8b}, [x12]\n"
          "smlal2 v24.4s, v0.8h, v12.8h\n"
          "smlal v21.4s, v1.4h, v10.4h\n"
          "smlal2 v22.4s, v1.8h, v10.8h\n"
          "smlal v23.4s, v1.4h, v13.4h\n"
          "smlal2 v24.4s, v1.8h, v13.8h\n"
          "smlal v21.4s, v2.4h, v11.4h\n"
          "smlal2 v22.4s, v2.8h, v11.8h\n"
          "smlal v23.4s, v2.4h, v14.4h\n"
          "smlal2 v24.4s, v2.8h, v14.8h\n"
          "smlal v21.4s, v3.4h, v12.4h\n"
          "smlal2 v22.4s, v3.8h, v12.8h\n"
          "ld1 {v12.8b}, [x13]\n"
          "smlal v23.4s, v3.4h, v15.4h\n"
          "smlal2 v24.4s, v3.8h, v15.8h\n"
          "smlal v21.4s, v4.4h, v13.4h\n"
          "smlal2 v22.4s, v4.8h, v13.8h\n"
          "smlal v23.4s, v4.4h, v16.4h\n"
          "smlal2 v24.4s, v4.8h, v16.8h\n"
          "smlal v21.4s, v5.4h, v14.4h\n"
          "smlal2 v22.4s, v5.8h, v14.8h\n"
          "smlal v23.4s, v5.4h, v17.4h\n"
          "smlal2 v24.4s, v5.8h, v17.8h\n"
          "smlal v21.4s, v6.4h, v15.4h\n"
          "smlal2 v22.4s, v6.8h, v15.8h\n"
          "ld1 {v15.8b}, [x14]\n"
          "smlal v23.4s, v6.4h, v18.4h\n"
          "smlal2 v24.4s, v6.8h, v18.8h\n"
          "ld1 {v18.8b}, [x15]\n"
          "smlal v21.4s, v7.4h, v16.4h\n"
          "smlal2 v22.4s, v7.8h, v16.8h\n"
          "smlal v23.4s, v7.4h, v19.4h\n"
          "smlal2 v24.4s, v7.8h, v19.8h\n"
          "smlal v21.4s, v8.4h, v17.4h\n"
          "smlal2 v22.4s, v8.8h, v17.8h\n"
          "smlal v23.4s, v8.4h, v20.4h\n"
          "smlal2 v24.4s, v8.8h, v20.8h\n"

          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "sqrshl v21.4s, v21.4s, v28.4s\n"
          "sqrshl v22.4s, v22.4s, v28.4s\n"
          "sqrshl v23.4s, v23.4s, v28.4s\n"
          "sqrshl v24.4s, v24.4s, v28.4s\n"
          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "sqxtun2 v21.16b, v23.8h\n"
          "ld1 {v22.4s}, [x10]\n"
          "umax v21.16b, v21.16b, v30.16b\n"
          "umin v21.16b, v21.16b, v31.16b\n"
          "ld1 {v24.4s}, [x10]\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "st1 {v21.8b}, [x6], x3\n"
          "mov v23.d[0], v21.d[1]\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "st1 {v23.8b}, [x7], x3\n"
          "uaddw v15.8h, v26.8h, v15.8b\n"
          "ld1 {v21.4s}, [%[bias_ptr]]\n"
          "uaddw v18.8h, v26.8h, v18.8b\n"
          "ld1 {v23.4s}, [%[bias_ptr]]\n"

          // Mul-add right outputs.
          "smlal v21.4s, v0.4h, v10.4h\n"
          "smlal2 v22.4s, v0.8h, v10.8h\n"
          "smlal v23.4s, v0.4h, v13.4h\n"
          "smlal2 v24.4s, v0.8h, v13.8h\n"
          "smlal v21.4s, v1.4h, v11.4h\n"
          "smlal2 v22.4s, v1.8h, v11.8h\n"
          "smlal v23.4s, v1.4h, v14.4h\n"
          "smlal2 v24.4s, v1.8h, v14.8h\n"
          "smlal v21.4s, v2.4h, v9.4h\n"
          "smlal2 v22.4s, v2.8h, v9.8h\n"
          "smlal v23.4s, v2.4h, v12.4h\n"
          "smlal2 v24.4s, v2.8h, v12.8h\n"
          "smlal v21.4s, v3.4h, v13.4h\n"
          "smlal2 v22.4s, v3.8h, v13.8h\n"
          "smlal v23.4s, v3.4h, v16.4h\n"
          "smlal2 v24.4s, v3.8h, v16.8h\n"
          "smlal v21.4s, v4.4h, v14.4h\n"
          "smlal2 v22.4s, v4.8h, v14.8h\n"
          "smlal v23.4s, v4.4h, v17.4h\n"
          "smlal2 v24.4s, v4.8h, v17.8h\n"
          "smlal v21.4s, v5.4h, v12.4h\n"
          "smlal2 v22.4s, v5.8h, v12.8h\n"
          "smlal v23.4s, v5.4h, v15.4h\n"
          "smlal2 v24.4s, v5.8h, v15.8h\n"
          "smlal v21.4s, v6.4h, v16.4h\n"
          "smlal2 v22.4s, v6.8h, v16.8h\n"
          "smlal v23.4s, v6.4h, v19.4h\n"
          "smlal2 v24.4s, v6.8h, v19.8h\n"
          "smlal v21.4s, v7.4h, v17.4h\n"
          "smlal2 v22.4s, v7.8h, v17.8h\n"
          "smlal v23.4s, v7.4h, v20.4h\n"
          "smlal2 v24.4s, v7.8h, v20.8h\n"
          "smlal v21.4s, v8.4h, v15.4h\n"
          "smlal2 v22.4s, v8.8h, v15.8h\n"
          "smlal v23.4s, v8.4h, v18.4h\n"
          "smlal2 v24.4s, v8.8h, v18.8h\n"

          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "sqrshl v21.4s, v21.4s, v28.4s\n"
          "sqrshl v22.4s, v22.4s, v28.4s\n"
          "sqrshl v23.4s, v23.4s, v28.4s\n"
          "sqrshl v24.4s, v24.4s, v28.4s\n"

          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "sqxtun2 v21.16b, v23.8h\n"
          "umax v21.16b, v21.16b, v30.16b\n"
          "umin v21.16b, v21.16b, v31.16b\n"
          "st1 {v21.8b}, [x6], x3\n"
          "mov v23.d[0], v21.d[1]\n"
          "st1 {v23.8b}, [x7], x3\n"
          "b " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP "f\n"

          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER ":\n"
          "smlal v21.4s, v0.4h, v9.4h\n"
          "smlal2 v22.4s, v0.8h, v9.8h\n"
          "smlal v23.4s, v0.4h, v12.4h\n"
          "smlal2 v24.4s, v0.8h, v12.8h\n"
          "smlal v21.4s, v1.4h, v10.4h\n"
          "smlal2 v22.4s, v1.8h, v10.8h\n"
          "smlal v23.4s, v1.4h, v13.4h\n"
          "smlal2 v24.4s, v1.8h, v13.8h\n"
          "smlal v21.4s, v2.4h, v11.4h\n"
          "smlal2 v22.4s, v2.8h, v11.8h\n"
          "smlal v23.4s, v2.4h, v14.4h\n"
          "smlal2 v24.4s, v2.8h, v14.8h\n"
          "smlal v21.4s, v3.4h, v12.4h\n"
          "smlal2 v22.4s, v3.8h, v12.8h\n"
          "smlal v23.4s, v3.4h, v15.4h\n"
          "smlal2 v24.4s, v3.8h, v15.8h\n"
          "smlal v21.4s, v4.4h, v13.4h\n"
          "smlal2 v22.4s, v4.8h, v13.8h\n"
          "smlal v23.4s, v4.4h, v16.4h\n"
          "smlal2 v24.4s, v4.8h, v16.8h\n"
          "smlal v21.4s, v5.4h, v14.4h\n"
          "smlal2 v22.4s, v5.8h, v14.8h\n"
          "smlal v23.4s, v5.4h, v17.4h\n"
          "smlal2 v24.4s, v5.8h, v17.8h\n"
          "smlal v21.4s, v6.4h, v15.4h\n"
          "smlal2 v22.4s, v6.8h, v15.8h\n"
          "smlal v23.4s, v6.4h, v18.4h\n"
          "smlal2 v24.4s, v6.8h, v18.8h\n"
          "smlal v21.4s, v7.4h, v16.4h\n"
          "smlal2 v22.4s, v7.8h, v16.8h\n"
          "smlal v23.4s, v7.4h, v19.4h\n"
          "smlal2 v24.4s, v7.8h, v19.8h\n"
          "smlal v21.4s, v8.4h, v17.4h\n"
          "smlal2 v22.4s, v8.8h, v17.8h\n"
          "smlal v23.4s, v8.4h, v20.4h\n"
          "smlal2 v24.4s, v8.8h, v20.8h\n"

          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "sqrshl v21.4s, v21.4s, v28.4s\n"
          "sqrshl v22.4s, v22.4s, v28.4s\n"
          "sqrshl v23.4s, v23.4s, v28.4s\n"
          "sqrshl v24.4s, v24.4s, v28.4s\n"
          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "sqxtun2 v21.16b, v23.8h\n"
          "umax v21.16b, v21.16b, v30.16b\n"
          "umin v21.16b, v21.16b, v31.16b\n"
          "st1 {v21.8b}, [x6], x3\n"
          "mov v23.d[0], v21.d[1]\n"
          "st1 {v23.8b}, [x7], x3\n"

          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP ":\n"
          "subs %w[output_window_height], %w[output_window_height], #2\n"
          "add %[input_ptr], %[input_ptr], %[input_height_increment]\n"
          "cmp %w[output_window_height], #2\n"
          "add %[output_ptr], %[output_ptr], %[output_height_increment]\n"
          "bge " DEPTHWISECONV_LABEL_HEIGHT_2_LOOP "b\n"

        DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP ":\n"
        "cmp %w[output_window_height], #1\n"
        "blt " DEPTHWISECONV_LABEL_HEIGHT_1_END "f\n"

        DEPTHWISECONV_LABEL_HEIGHT_1 ":\n"
        "mov x12, %[input_ptr]\n"
        "ld1 {v9.8b}, [x12], %[input_depth]\n"
        "add x13, %[input_ptr], %[input_row_size]\n"
        "ld1 {v10.8b}, [x12], %[input_depth]\n"
        "add x14, x13, %[input_row_size]\n"
        "ld1 {v11.8b}, [x12], %[input_depth]\n"
        "add x15, x14, %[input_row_size]\n"
        "mov w5, %w[output_window_width]\n"
        "ld1 {v13.8b}, [x13], %[input_depth]\n"
        "mov x6, %[output_ptr]\n"
        "ld1 {v14.8b}, [x13], %[input_depth]\n"
        "add x7, %[output_ptr], x1\n"
        "ld1 {v15.8b}, [x13], %[input_depth]\n"
        // The height 1 / width 2 loop loads an extra 1x1 output in anticipation
        // for the next iteration. Make sure |output_window_width| is large
        // enough to handle the additional load, otherwise jump to the
        // appropriate label to handle smaller widths.
        "cmp w5, #2\n"
        "ld1 {v17.8b}, [x14], %[input_depth]\n"
        "ld1 {v18.8b}, [x14], %[input_depth]\n"
        "ld1 {v19.8b}, [x14], %[input_depth]\n"
        "ld1 {v21.4s}, [%[bias_ptr]]\n"
        "ld1 {v22.4s}, [x10]\n"
        "ld1 {v23.4s}, [%[bias_ptr]]\n"
        "ld1 {v24.4s}, [x10]\n"

        "uaddw v9.8h, v26.8h, v9.8b\n"
        "uaddw v10.8h, v26.8h, v10.8b\n"
        "uaddw v11.8h, v26.8h, v11.8b\n"
        "uaddw v13.8h, v26.8h, v13.8b\n"
        "uaddw v14.8h, v26.8h, v14.8b\n"
        "uaddw v15.8h, v26.8h, v15.8b\n"
        "uaddw v17.8h, v26.8h, v17.8b\n"
        "uaddw v18.8h, v26.8h, v18.8b\n"
        "uaddw v19.8h, v26.8h, v19.8b\n"

        "beq " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER "f\n"
        "cmp w5, #1\n"
        "beq " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP ":\n"
          // Load inputs for 3x4 input window which corresponds to a 1x2 output
          // window.
          "smlal v21.4s, v0.4h, v9.4h\n"
          "ld1 {v12.8b}, [x12]\n"
          "smlal2 v22.4s, v0.8h, v9.8h\n"
          "ld1 {v16.8b}, [x13]\n"
          "smlal v23.4s, v0.4h, v10.4h\n"
          "ld1 {v20.8b}, [x14]\n"
          "smlal2 v24.4s, v0.8h, v10.8h\n"
          "subs w5, w5, #2\n"
          "smlal v21.4s, v1.4h, v10.4h\n"
          "cmp w5, #3\n"
          "smlal2 v22.4s, v1.8h, v10.8h\n"
          "add %[input_ptr], %[input_ptr], %[input_width_increment]\n"
          "smlal v23.4s, v1.4h, v11.4h\n"
          "mov x12, %[input_ptr]\n"
          "smlal2 v24.4s, v1.8h, v11.8h\n"
          "ld1 {v9.8b}, [x12], %[input_depth]\n"
          "smlal v21.4s, v2.4h, v11.4h\n"
          "ld1 {v10.8b}, [x12], %[input_depth]\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "smlal2 v22.4s, v2.8h, v11.8h\n"
          "ld1 {v11.8b}, [x12], %[input_depth]\n"
          "add x13, %[input_ptr], %[input_row_size]\n"
          "smlal v23.4s, v2.4h, v12.4h\n"
          "add x14, x13, %[input_row_size]\n"
          "smlal2 v24.4s, v2.8h, v12.8h\n"
          "smlal v21.4s, v3.4h, v13.4h\n"
          "add x15, x14, %[input_row_size]\n"
          "smlal2 v22.4s, v3.8h, v13.8h\n"
          "ld1 {v13.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v3.4h, v14.4h\n"
          "smlal2 v24.4s, v3.8h, v14.8h\n"
          "smlal v21.4s, v4.4h, v14.4h\n"
          "smlal2 v22.4s, v4.8h, v14.8h\n"
          "ld1 {v14.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v4.4h, v15.4h\n"
          "smlal2 v24.4s, v4.8h, v15.8h\n"
          "smlal v21.4s, v5.4h, v15.4h\n"
          "uaddw v16.8h, v26.8h, v16.8b\n"
          "smlal2 v22.4s, v5.8h, v15.8h\n"
          "ld1 {v15.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v5.4h, v16.4h\n"
          "smlal2 v24.4s, v5.8h, v16.8h\n"
          "smlal v21.4s, v6.4h, v17.4h\n"
          "smlal2 v22.4s, v6.8h, v17.8h\n"
          "ld1 {v17.8b}, [x14], %[input_depth]\n"
          "smlal v23.4s, v6.4h, v18.4h\n"
          "smlal2 v24.4s, v6.8h, v18.8h\n"
          "smlal v21.4s, v7.4h, v18.4h\n"
          "smlal2 v22.4s, v7.8h, v18.8h\n"
          "ld1 {v18.8b}, [x14], %[input_depth]\n"
          "smlal v23.4s, v7.4h, v19.4h\n"
          "smlal2 v24.4s, v7.8h, v19.8h\n"
          "smlal v21.4s, v8.4h, v19.4h\n"
          "uaddw v20.8h, v26.8h, v20.8b\n"
          "smlal2 v22.4s, v8.8h, v19.8h\n"
          "ld1 {v19.8b}, [x14], %[input_depth]\n"
          "smlal v23.4s, v8.4h, v20.4h\n"
          "smlal2 v24.4s, v8.8h, v20.8h\n"

          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "sqrshl v21.4s, v21.4s, v28.4s\n"
          "sqrshl v22.4s, v22.4s, v28.4s\n"
          "sqrshl v23.4s, v23.4s, v28.4s\n"
          "sqrshl v24.4s, v24.4s, v28.4s\n"
          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "sqxtun2 v21.16b, v23.8h\n"
          "ld1 {v22.4s}, [x10]\n"
          "umax v21.16b, v21.16b, v30.16b\n"
          "umin v21.16b, v21.16b, v31.16b\n"
          "ld1 {v24.4s}, [x10]\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "st1 {v21.8b}, [%[output_ptr]], x3\n"
          "uaddw v10.8h, v26.8h, v10.8b\n"
          "mov v23.d[0], v21.d[1]\n"
          "st1 {v23.8b}, [%[output_ptr]], x3\n"
          "uaddw v11.8h, v26.8h, v11.8b\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "uaddw v13.8h, v26.8h, v13.8b\n"
          "uaddw v14.8h, v26.8h, v14.8b\n"
          "uaddw v15.8h, v26.8h, v15.8b\n"
          "ld1 {v21.4s}, [%[bias_ptr]]\n"
          "uaddw v16.8h, v26.8h, v16.8b\n"
          "ld1 {v23.4s}, [%[bias_ptr]]\n"
          "uaddw v17.8h, v26.8h, v17.8b\n"
          "uaddw v18.8h, v26.8h, v18.8b\n"
          "uaddw v19.8h, v26.8h, v19.8b\n"
          "uaddw v20.8h, v26.8h, v20.8b\n"

          "bge " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP "b\n"

        // At this point, there will be one of 2 width or 1 width leftover,
        // not both.
        "cmp w5, #2\n"
        "blt " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "f\n"

        // Handle last two horizontal outputs if exists.
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER ":\n"
        "smlal v21.4s, v0.4h, v9.4h\n"
        "ld1 {v12.8b}, [x12], %[input_depth]\n"
        "smlal2 v22.4s, v0.8h, v9.8h\n"
        "ld1 {v16.8b}, [x13], %[input_depth]\n"
        "smlal v23.4s, v0.4h, v10.4h\n"
        "ld1 {v20.8b}, [x14], %[input_depth]\n"
        "smlal2 v24.4s, v0.8h, v10.8h\n"
        "smlal v21.4s, v1.4h, v10.4h\n"
        "smlal2 v22.4s, v1.8h, v10.8h\n"
        "smlal v23.4s, v1.4h, v11.4h\n"
        "smlal2 v24.4s, v1.8h, v11.8h\n"
        "smlal v21.4s, v2.4h, v11.4h\n"
        "uaddw v12.8h, v26.8h, v12.8b\n"
        "smlal2 v22.4s, v2.8h, v11.8h\n"
        "smlal v23.4s, v2.4h, v12.4h\n"
        "smlal2 v24.4s, v2.8h, v12.8h\n"
        "smlal v21.4s, v3.4h, v13.4h\n"
        "smlal2 v22.4s, v3.8h, v13.8h\n"
        "smlal v23.4s, v3.4h, v14.4h\n"
        "smlal2 v24.4s, v3.8h, v14.8h\n"
        "smlal v21.4s, v4.4h, v14.4h\n"
        "smlal2 v22.4s, v4.8h, v14.8h\n"
        "smlal v23.4s, v4.4h, v15.4h\n"
        "smlal2 v24.4s, v4.8h, v15.8h\n"
        "smlal v21.4s, v5.4h, v15.4h\n"
        "uaddw v16.8h, v26.8h, v16.8b\n"
        "smlal2 v22.4s, v5.8h, v15.8h\n"
        "smlal v23.4s, v5.4h, v16.4h\n"
        "smlal2 v24.4s, v5.8h, v16.8h\n"
        "smlal v21.4s, v6.4h, v17.4h\n"
        "smlal2 v22.4s, v6.8h, v17.8h\n"
        "smlal v23.4s, v6.4h, v18.4h\n"
        "smlal2 v24.4s, v6.8h, v18.8h\n"
        "smlal v21.4s, v7.4h, v18.4h\n"
        "smlal2 v22.4s, v7.8h, v18.8h\n"
        "smlal v23.4s, v7.4h, v19.4h\n"
        "smlal2 v24.4s, v7.8h, v19.8h\n"
        "smlal v21.4s, v8.4h, v19.4h\n"
        "uaddw v20.8h, v26.8h, v20.8b\n"
        "smlal2 v22.4s, v8.8h, v19.8h\n"
        "smlal v23.4s, v8.4h, v20.4h\n"
        "smlal2 v24.4s, v8.8h, v20.8h\n"

        "sqrdmulh v21.4s, v21.4s, v27.4s\n"
        "sqrdmulh v22.4s, v22.4s, v27.4s\n"
        "sqrdmulh v23.4s, v23.4s, v27.4s\n"
        "sqrdmulh v24.4s, v24.4s, v27.4s\n"
        "sqrshl v21.4s, v21.4s, v28.4s\n"
        "sqrshl v22.4s, v22.4s, v28.4s\n"
        "sqrshl v23.4s, v23.4s, v28.4s\n"
        "sqrshl v24.4s, v24.4s, v28.4s\n"
        "sqxtn v21.4h, v21.4s\n"
        "sqxtn2 v21.8h, v22.4s\n"
        "sqxtn v23.4h, v23.4s\n"
        "sqxtn2 v23.8h, v24.4s\n"
        "sqadd v21.8h, v21.8h, v29.8h\n"
        "sqadd v23.8h, v23.8h, v29.8h\n"
        "sqxtun v21.8b, v21.8h\n"
        "sqxtun2 v21.16b, v23.8h\n"
        "umax v21.16b, v21.16b, v30.16b\n"
        "umin v21.16b, v21.16b, v31.16b\n"
        "st1 {v21.8b}, [%[output_ptr]], x3\n"
        "mov v23.d[0], v21.d[1]\n"
        "st1 {v23.8b}, [%[output_ptr]], x3\n"
        "b " DEPTHWISECONV_LABEL_HEIGHT_1_END "f\n"

        // Handle bottom right output if exists.
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER ":\n"
        "smlal v21.4s, v0.4h, v9.4h\n"
        "smlal2 v22.4s, v0.8h, v9.8h\n"
        "smlal v21.4s, v1.4h, v10.4h\n"
        "smlal2 v22.4s, v1.8h, v10.8h\n"
        "smlal v21.4s, v2.4h, v11.4h\n"
        "smlal2 v22.4s, v2.8h, v11.8h\n"
        "smlal v21.4s, v3.4h, v13.4h\n"
        "smlal2 v22.4s, v3.8h, v13.8h\n"
        "smlal v21.4s, v4.4h, v14.4h\n"
        "smlal2 v22.4s, v4.8h, v14.8h\n"
        "smlal v21.4s, v5.4h, v15.4h\n"
        "smlal2 v22.4s, v5.8h, v15.8h\n"
        "smlal v21.4s, v6.4h, v17.4h\n"
        "smlal2 v22.4s, v6.8h, v17.8h\n"
        "smlal v21.4s, v7.4h, v18.4h\n"
        "smlal2 v22.4s, v7.8h, v18.8h\n"
        "smlal v21.4s, v8.4h, v19.4h\n"
        "smlal2 v22.4s, v8.8h, v19.8h\n"

        "sqrdmulh v21.4s, v21.4s, v27.4s\n"
        "sqrdmulh v22.4s, v22.4s, v27.4s\n"
        "sqrshl v21.4s, v21.4s, v28.4s\n"
        "sqrshl v22.4s, v22.4s, v28.4s\n"
        "sqxtn v21.4h, v21.4s\n"
        "sqxtn2 v21.8h, v22.4s\n"
        "sqadd v21.8h, v21.8h, v29.8h\n"
        "sqxtun v21.8b, v21.8h\n"
        "umax v21.8b, v21.8b, v30.8b\n"
        "umin v21.8b, v21.8b, v31.8b\n"
        "st1 {v21.8b}, [%[output_ptr]]\n"
        DEPTHWISECONV_LABEL_HEIGHT_1_END ":\n"
    :
    // Outputs.
    [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
    [output_ptr] "+r"(output_ptr),
    [output_window_height] "+r"(output_window_height)
    :
    // Inputs.
    [bias_ptr] "r"(bias_ptr), [input_row_size] "r"(input_row_size),
    [input_depth] "r"(input_depth),
    [output_window_width] "r"(output_window_width),
    [input_width_increment] "r"(input_width_increment),
    [input_height_increment] "r"(input_height_increment),
    [output_height_increment] "r"(output_height_increment),
    [params_ptr] "r"(params_ptr)
    :
    // Clobbers.
    "cc", "memory",
    // We use these NEON registers.
    "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9",
    "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19",
    "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
    "v30", "v31",
    // We use these general-purpose registers.
    "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
    "x9", "x10", "x11", "x12", "x13", "x14", "x15");
#undef DEPTHWISECONV_LABEL_HEIGHT_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_1
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_1_END
  }
};

template <>
struct DepthwiseConvWindow<DepthwiseConvOutputRounding::kAwayFromZero, 8, 2,
                           2> {
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         int64_t input_depth, int64_t input_row_size,
                         int32 output_window_height, int32 output_window_width,
                         const DepthwiseConvParams* params_ptr) {
    const int64_t input_width_increment = 4 * input_depth;
    const int64_t input_height_increment = 4 * input_row_size;
    const int64_t output_height_increment = 2 * params_ptr->output_row_size;

#define DEPTHWISECONV_LABEL_HEIGHT_2_LOOP "1"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP "2"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "3"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER "4"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP "5"
#define DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP "6"
#define DEPTHWISECONV_LABEL_HEIGHT_1 "7"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP "8"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "9"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER "10"
#define DEPTHWISECONV_LABEL_HEIGHT_1_END "11"

    asm volatile(
        // Performs depthwise convolutions for a window specified by
        // |output_window_height| and |output_window_width|. The inner-most loop
        // processes 2x2 outputs, and any leftovers at the end.
        //
        // Algorithm works as follows:
        //
        //   1. Load filters of 8 depth (8x3x3). Registers v0--v8 hold filter
        //      values.
        //   2. For 2 output heights at a time:
        //        i.  For 2 output widths at a time at stride 2, a 5x5 input
        //            window is required. To avoid register exhaustion, we load
        //            the first 2 rows of the 5x5 input window into registers
        //            v9--v18, and use the same registers to load the next 2
        //            rows, and finally v9--v13 to load the last row.
        //            Accumulators for all 2x2 outputs are reserved by registers
        //            v21-v22 (top left output), v23-v24 (top right output),
        //            v19-v20 (bottom left output), v25-v26 (bottom right
        //            output).
        //        ii. Handle single leftover width if exists.
        //   3. Handle single leftover height if exists.
        //        i.  For 2 output widths at a time at stride 2, load inputs for
        //            a 1x2 (1 height, 2 width) output window (3x5 input
        //            window). Registers v9--v24 hold input values. Mul-add with
        //            accumulators v24--v27.
        //        ii. Handle single leftover width if exists.
        //
        // Loads are placed as soon as the register is no longer needed and
        // interleaved with arithmetic operations to take advantage of
        // dual-issue pipelines. We also add input offsets as far from the loads
        // as possible to give loads enough cycles to fetch data from memory.

        // Set "constant" registers. These registers may be replaced with temp
        // values from time to time when there are not enough NEON registers.
        // We use x9--x15 general purpose registers as they are caller-saved
        // temporary registers (see http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf).  // NOLINT
        "ldr w9, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "ldr w0, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "cmp %w[output_window_height], #2\n"
        "dup v28.8h, w0\n"
        "ldr w1, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "dup v26.4s, w9\n"
        "ldr w2, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w1\n"
        "ldr w3, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v29.8h, w2\n"
        "ldr w4, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v30.16b, w3\n"
        "ldr x5, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"
        "dup v31.16b, w4\n"
        "ldr x19, [%[params_ptr], #" STR(OFFSET_OUTPUT_ROW_SIZE) "]\n"
        "ldr w20, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"

        // Load filters and add offsets.
        "add x10, %[bias_ptr], #16\n"
        "ld1 {v0.8b}, [%[filter_ptr]], x5\n"
        "dup v9.8h, w20\n"
        "ld1 {v1.8b}, [%[filter_ptr]], x5\n"
        "uaddw v0.8h, v9.8h, v0.8b\n"
        "ld1 {v2.8b}, [%[filter_ptr]], x5\n"
        "uaddw v1.8h, v9.8h, v1.8b\n"
        "ld1 {v3.8b}, [%[filter_ptr]], x5\n"
        "uaddw v2.8h, v9.8h, v2.8b\n"
        "ld1 {v4.8b}, [%[filter_ptr]], x5\n"
        "uaddw v3.8h, v9.8h, v3.8b\n"
        "ld1 {v5.8b}, [%[filter_ptr]], x5\n"
        "uaddw v4.8h, v9.8h, v4.8b\n"
        "ld1 {v6.8b}, [%[filter_ptr]], x5\n"
        "uaddw v5.8h, v9.8h, v5.8b\n"
        "ld1 {v7.8b}, [%[filter_ptr]], x5\n"
        "uaddw v6.8h, v9.8h, v6.8b\n"
        "ld1 {v8.8b}, [%[filter_ptr]]\n"
        "uaddw v7.8h, v9.8h, v7.8b\n"
        "uaddw v8.8h, v9.8h, v8.8b\n"

        "blt " DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_HEIGHT_2_LOOP ":\n"
          // Load the first two rows of the 5x5 input window, then reuse the
          // same registers to load subsequent rows as they become available.
          "mov x11, %[input_ptr]\n"
          "mov x12, x11\n"
          "add x13, x12, %[input_row_size]\n"
          "ld1 {v9.8b}, [x12], %[input_depth]\n"
          "mov w14, %w[output_window_width]\n"
          "ld1 {v10.8b}, [x12], %[input_depth]\n"
          // The height 2 / width 2 loop loads an extra 1 output horizontally in
          // anticipation for the next iteration. Make sure
          // |output_window_width| is large enough to handle the additional
          // load, otherwise jump to the appropriate label to handle smaller
          // widths.
          "cmp w14, #2\n"
          "ld1 {v11.8b}, [x12], %[input_depth]\n"
          "add x15, x13, %[input_row_size]\n"
          "ld1 {v14.8b}, [x13], %[input_depth]\n"
          "mov x6, %[output_ptr]\n"
          "ld1 {v15.8b}, [x13], %[input_depth]\n"
          "add x7, %[output_ptr], x19\n"
          "ld1 {v16.8b}, [x13], %[input_depth]\n"
          "ld1 {v21.4s}, [%[bias_ptr]]\n"
          "ld1 {v22.4s}, [x10]\n"
          "ld1 {v23.4s}, [%[bias_ptr]]\n"
          "uaddw v9.8h, v28.8h, v9.8b\n"
          "ld1 {v24.4s}, [x10]\n"
          "uaddw v10.8h, v28.8h, v10.8b\n"
          "ld1 {v19.4s}, [%[bias_ptr]]\n"
          "uaddw v11.8h, v28.8h, v11.8b\n"
          "ld1 {v20.4s}, [x10]\n"
          "uaddw v14.8h, v28.8h, v14.8b\n"
          "ld1 {v25.4s}, [%[bias_ptr]]\n"
          "uaddw v15.8h, v28.8h, v15.8b\n"
          "ld1 {v26.4s}, [x10]\n"
          "uaddw v16.8h, v28.8h, v16.8b\n"

          "beq " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER "f\n"
          "cmp w14, #1\n"
          "beq " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "f\n"

          //"loop_%=:\n"
          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP ":\n"
            "smlal v21.4s, v0.4h, v9.4h\n"
            "ld1 {v12.8b}, [x12], %[input_depth]\n"
            "smlal2 v22.4s, v0.8h, v9.8h\n"
            "ld1 {v13.8b}, [x12]\n"
            "add x12, x15, %[input_row_size]\n"
            "smlal v23.4s, v0.4h, v11.4h\n"
            "ld1 {v17.8b}, [x13], %[input_depth]\n"
            "smlal2 v24.4s, v0.8h, v11.8h\n"
            "ld1 {v18.8b}, [x13]\n"
            "add x13, x12, %[input_row_size]\n"
            "smlal v21.4s, v1.4h, v10.4h\n"
            "ld1 {v9.8b}, [x15], %[input_depth]\n"
            "smlal2 v22.4s, v1.8h, v10.8h\n"
            "ld1 {v10.8b}, [x15], %[input_depth]\n"
            "smlal v21.4s, v2.4h, v11.4h\n"
            "smlal2 v22.4s, v2.8h, v11.8h\n"
            "ld1 {v11.8b}, [x15], %[input_depth]\n"
            "smlal v21.4s, v3.4h, v14.4h\n"
            "smlal2 v22.4s, v3.8h, v14.8h\n"
            "ld1 {v14.8b}, [x12], %[input_depth]\n"
            "smlal v23.4s, v3.4h, v16.4h\n"
            "subs w14, w14, #2\n"
            "smlal2 v24.4s, v3.8h, v16.8h\n"
            "cmp w14, #3\n"
            "smlal v21.4s, v4.4h, v15.4h\n"
            "uaddw v12.8h, v28.8h, v12.8b\n"
            "smlal2 v22.4s, v4.8h, v15.8h\n"
            "ld1 {v15.8b}, [x12], %[input_depth]\n"
            "smlal v21.4s, v5.4h, v16.4h\n"
            "uaddw v13.8h, v28.8h, v13.8b\n"
            "smlal2 v22.4s, v5.8h, v16.8h\n"
            "ld1 {v16.8b}, [x12], %[input_depth]\n"
            "smlal v23.4s, v1.4h, v12.4h\n"
            "uaddw v17.8h, v28.8h, v17.8b\n"
            "smlal2 v24.4s, v1.8h, v12.8h\n"
            "ld1 {v12.8b}, [x15], %[input_depth]\n"
            "smlal v23.4s, v2.4h, v13.4h\n"
            "uaddw v18.8h, v28.8h, v18.8b\n"
            "smlal2 v24.4s, v2.8h, v13.8h\n"
            "ld1 {v13.8b}, [x15]\n"
            "smlal v23.4s, v4.4h, v17.4h\n"
            "uaddw v9.8h, v28.8h, v9.8b\n"
            "smlal2 v24.4s, v4.8h, v17.8h\n"
            "ld1 {v17.8b}, [x12], %[input_depth]\n"
            "smlal v23.4s, v5.4h, v18.4h\n"
            "uaddw v10.8h, v28.8h, v10.8b\n"
            "smlal2 v24.4s, v5.8h, v18.8h\n"
            "ld1 {v18.8b}, [x12]\n"

            "smlal v21.4s, v6.4h, v9.4h\n"
            "smlal2 v22.4s, v6.8h, v9.8h\n"
            "smlal v19.4s, v0.4h, v9.4h\n"
            "uaddw v11.8h, v28.8h, v11.8b\n"
            "smlal2 v20.4s, v0.8h, v9.8h\n"
            "ld1 {v9.8b}, [x13], %[input_depth]\n"
            "smlal v23.4s, v6.4h, v11.4h\n"
            "smlal2 v24.4s, v6.8h, v11.8h\n"
            "smlal v21.4s, v7.4h, v10.4h\n"
            "smlal2 v22.4s, v7.8h, v10.8h\n"
            "uaddw v12.8h, v28.8h, v12.8b\n"
            "smlal v19.4s, v1.4h, v10.4h\n"
            "smlal2 v20.4s, v1.8h, v10.8h\n"
            "ld1 {v10.8b}, [x13], %[input_depth]\n"
            "smlal v23.4s, v7.4h, v12.4h\n"
            "smlal2 v24.4s, v7.8h, v12.8h\n"
            "smlal v25.4s, v1.4h, v12.4h\n"
            "smlal2 v26.4s, v1.8h, v12.8h\n"
            "smlal v21.4s, v8.4h, v11.4h\n"
            "smlal2 v22.4s, v8.8h, v11.8h\n"
            "add x11, x11, %[input_width_increment]\n"
            "smlal v19.4s, v2.4h, v11.4h\n"
            "mov x12, x11\n"
            "smlal2 v20.4s, v2.8h, v11.8h\n"
            "uaddw v13.8h, v28.8h, v13.8b\n"
            "smlal v25.4s, v0.4h, v11.4h\n"
            "smlal2 v26.4s, v0.8h, v11.8h\n"
            "ld1 {v11.8b}, [x13], %[input_depth]\n"
            "smlal v23.4s, v8.4h, v13.4h\n"
            "ld1 {v12.8b}, [x13], %[input_depth]\n"
            "smlal2 v24.4s, v8.8h, v13.8h\n"
            "smlal v25.4s, v2.4h, v13.4h\n"
            "smlal2 v26.4s, v2.8h, v13.8h\n"
            "ld1 {v13.8b}, [x13]\n"
            "add x13, x12, %[input_row_size]\n"
            "add x15, x13, %[input_row_size]\n"

            "dup v28.4s, w9\n"
            "sqrdmulh v21.4s, v21.4s, v27.4s\n"
            "sqrdmulh v22.4s, v22.4s, v27.4s\n"
            "sqrdmulh v23.4s, v23.4s, v27.4s\n"
            "sqrdmulh v24.4s, v24.4s, v27.4s\n"
            "and v27.16b, v21.16b, v28.16b\n"
            "and v29.16b, v22.16b, v28.16b\n"
            "and v30.16b, v23.16b, v28.16b\n"
            "and v31.16b, v24.16b, v28.16b\n"
            "sshr v27.4s, v27.4s, #31\n"
            "sshr v29.4s, v29.4s, #31\n"
            "sshr v30.4s, v30.4s, #31\n"
            "sshr v31.4s, v31.4s, #31\n"
            "sqadd v21.4s, v21.4s, v27.4s\n"
            "dup v27.4s, w1\n"
            "sqadd v22.4s, v22.4s, v29.4s\n"
            "dup v29.8h, w2\n"
            "sqadd v23.4s, v23.4s, v30.4s\n"
            "dup v30.16b, w3\n"
            "sqadd v24.4s, v24.4s, v31.4s\n"
            "dup v31.16b, w4\n"
            "srshl v21.4s, v21.4s, v28.4s\n"
            "srshl v22.4s, v22.4s, v28.4s\n"
            "srshl v23.4s, v23.4s, v28.4s\n"
            "srshl v24.4s, v24.4s, v28.4s\n"
            "dup v28.8h, w0\n"
            "sqxtn v21.4h, v21.4s\n"
            "sqxtn2 v21.8h, v22.4s\n"
            "sqxtn v23.4h, v23.4s\n"
            "sqxtn2 v23.8h, v24.4s\n"
            "sqadd v21.8h, v21.8h, v29.8h\n"
            "sqadd v23.8h, v23.8h, v29.8h\n"
            "sqxtun v21.8b, v21.8h\n"
            "sqxtun2 v21.16b, v23.8h\n"
            "ld1 {v22.4s}, [x10]\n"
            "umax v21.16b, v21.16b, v30.16b\n"
            "umin v21.16b, v21.16b, v31.16b\n"
            "ld1 {v24.4s}, [x10]\n"
            "uaddw v9.8h, v28.8h, v9.8b\n"
            "st1 {v21.8b}, [x6], x5\n"
            "uaddw v10.8h, v28.8h, v10.8b\n"
            "mov v23.d[0], v21.d[1]\n"
            "st1 {v23.8b}, [x6], x5\n"
            "uaddw v11.8h, v28.8h, v11.8b\n"

            "smlal v19.4s, v6.4h, v9.4h\n"
            "smlal2 v20.4s, v6.8h, v9.8h\n"
            "ld1 {v9.8b}, [x12], %[input_depth]\n"
            "smlal v25.4s, v6.4h, v11.4h\n"
            "smlal2 v26.4s, v6.8h, v11.8h\n"
            "smlal v19.4s, v7.4h, v10.4h\n"
            "uaddw v12.8h, v28.8h, v12.8b\n"
            "smlal2 v20.4s, v7.8h, v10.8h\n"
            "ld1 {v10.8b}, [x12], %[input_depth]\n"
            "smlal v25.4s, v7.4h, v12.4h\n"
            "smlal2 v26.4s, v7.8h, v12.8h\n"
            "smlal v19.4s, v8.4h, v11.4h\n"
            "uaddw v13.8h, v28.8h, v13.8b\n"
            "smlal2 v20.4s, v8.8h, v11.8h\n"
            "ld1 {v11.8b}, [x12], %[input_depth]\n"
            "smlal v25.4s, v8.4h, v13.4h\n"
            "uaddw v14.8h, v28.8h, v14.8b\n"
            "smlal2 v26.4s, v8.8h, v13.8h\n"
            "uaddw v16.8h, v28.8h, v16.8b\n"
            "smlal v19.4s, v3.4h, v14.4h\n"
            "uaddw v15.8h, v28.8h, v15.8b\n"
            "smlal2 v20.4s, v3.8h, v14.8h\n"
            "ld1 {v14.8b}, [x13], %[input_depth]\n"
            "smlal v25.4s, v3.4h, v16.4h\n"
            "ld1 {v21.4s}, [%[bias_ptr]]\n"
            "smlal2 v26.4s, v3.8h, v16.8h\n"
            "ld1 {v23.4s}, [%[bias_ptr]]\n"
            "smlal v19.4s, v4.4h, v15.4h\n"
            "uaddw v17.8h, v28.8h, v17.8b\n"
            "smlal2 v20.4s, v4.8h, v15.8h\n"
            "ld1 {v15.8b}, [x13], %[input_depth]\n"
            "smlal v25.4s, v4.4h, v17.4h\n"
            "smlal2 v26.4s, v4.8h, v17.8h\n"
            "smlal v19.4s, v5.4h, v16.4h\n"
            "uaddw v18.8h, v28.8h, v18.8b\n"
            "smlal2 v20.4s, v5.8h, v16.8h\n"
            "ld1 {v16.8b}, [x13], %[input_depth]\n"
            "smlal v25.4s, v5.4h, v18.4h\n"
            "smlal2 v26.4s, v5.8h, v18.8h\n"

            "dup v28.4s, w9\n"
            "sqrdmulh v19.4s, v19.4s, v27.4s\n"
            "sqrdmulh v20.4s, v20.4s, v27.4s\n"
            "sqrdmulh v25.4s, v25.4s, v27.4s\n"
            "sqrdmulh v26.4s, v26.4s, v27.4s\n"
            "and v27.16b, v19.16b, v28.16b\n"
            "and v29.16b, v20.16b, v28.16b\n"
            "and v30.16b, v25.16b, v28.16b\n"
            "and v31.16b, v26.16b, v28.16b\n"
            "sshr v27.4s, v27.4s, #31\n"
            "sshr v29.4s, v29.4s, #31\n"
            "sshr v30.4s, v30.4s, #31\n"
            "sshr v31.4s, v31.4s, #31\n"
            "sqadd v19.4s, v19.4s, v27.4s\n"
            "dup v27.4s, w1\n"
            "sqadd v20.4s, v20.4s, v29.4s\n"
            "dup v29.8h, w2\n"
            "sqadd v25.4s, v25.4s, v30.4s\n"
            "dup v30.16b, w3\n"
            "sqadd v26.4s, v26.4s, v31.4s\n"
            "dup v31.16b, w4\n"
            "srshl v19.4s, v19.4s, v28.4s\n"
            "srshl v20.4s, v20.4s, v28.4s\n"
            "srshl v25.4s, v25.4s, v28.4s\n"
            "srshl v26.4s, v26.4s, v28.4s\n"
            "dup v28.8h, w0\n"
            "sqxtn v19.4h, v19.4s\n"
            "sqxtn2 v19.8h, v20.4s\n"
            "sqxtn v25.4h, v25.4s\n"
            "sqxtn2 v25.8h, v26.4s\n"
            "sqadd v19.8h, v19.8h, v29.8h\n"
            "sqadd v25.8h, v25.8h, v29.8h\n"
            "sqxtun v19.8b, v19.8h\n"
            "sqxtun2 v19.16b, v25.8h\n"
            "ld1 {v20.4s}, [x10]\n"
            "umax v19.16b, v19.16b, v30.16b\n"
            "umin v19.16b, v19.16b, v31.16b\n"
            "ld1 {v26.4s}, [x10]\n"
            "uaddw v9.8h, v28.8h, v9.8b\n"
            "st1 {v19.8b}, [x7], x5\n"
            "uaddw v10.8h, v28.8h, v10.8b\n"
            "mov v25.d[0], v19.d[1]\n"
            "st1 {v25.8b}, [x7], x5\n"
            "uaddw v11.8h, v28.8h, v11.8b\n"
            "ld1 {v19.4s}, [%[bias_ptr]]\n"
            "uaddw v14.8h, v28.8h, v14.8b\n"
            "ld1 {v25.4s}, [%[bias_ptr]]\n"
            "uaddw v15.8h, v28.8h, v15.8b\n"
            "uaddw v16.8h, v28.8h, v16.8b\n"

            "bge " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP "b\n"

          // At this point, there will be one of 2 width or 1 width leftover,
          // not both.
          "cmp w14, #2\n"
          "blt " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "f\n"

          // Handle last 2 columns if exists.
          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER ":\n"
          "smlal v21.4s, v0.4h, v9.4h\n"
          "ld1 {v12.8b}, [x12], %[input_depth]\n"
          "smlal2 v22.4s, v0.8h, v9.8h\n"
          "ld1 {v13.8b}, [x12]\n"
          "add x12, x15, %[input_row_size]\n"
          "smlal v23.4s, v0.4h, v11.4h\n"
          "ld1 {v17.8b}, [x13], %[input_depth]\n"
          "smlal2 v24.4s, v0.8h, v11.8h\n"
          "ld1 {v18.8b}, [x13]\n"
          "add x13, x12, %[input_row_size]\n"
          "smlal v21.4s, v1.4h, v10.4h\n"
          "ld1 {v9.8b}, [x15], %[input_depth]\n"
          "smlal2 v22.4s, v1.8h, v10.8h\n"
          "ld1 {v10.8b}, [x15], %[input_depth]\n"
          "smlal v21.4s, v2.4h, v11.4h\n"
          "smlal2 v22.4s, v2.8h, v11.8h\n"
          "ld1 {v11.8b}, [x15], %[input_depth]\n"
          "smlal v21.4s, v3.4h, v14.4h\n"
          "smlal2 v22.4s, v3.8h, v14.8h\n"
          "ld1 {v14.8b}, [x12], %[input_depth]\n"
          "smlal v23.4s, v3.4h, v16.4h\n"
          "smlal2 v24.4s, v3.8h, v16.8h\n"
          "smlal v21.4s, v4.4h, v15.4h\n"
          "uaddw v12.8h, v28.8h, v12.8b\n"
          "smlal2 v22.4s, v4.8h, v15.8h\n"
          "ld1 {v15.8b}, [x12], %[input_depth]\n"
          "smlal v21.4s, v5.4h, v16.4h\n"
          "uaddw v13.8h, v28.8h, v13.8b\n"
          "smlal2 v22.4s, v5.8h, v16.8h\n"
          "ld1 {v16.8b}, [x12], %[input_depth]\n"
          "smlal v23.4s, v1.4h, v12.4h\n"
          "uaddw v17.8h, v28.8h, v17.8b\n"
          "smlal2 v24.4s, v1.8h, v12.8h\n"
          "ld1 {v12.8b}, [x15], %[input_depth]\n"
          "smlal v23.4s, v2.4h, v13.4h\n"
          "uaddw v18.8h, v28.8h, v18.8b\n"
          "smlal2 v24.4s, v2.8h, v13.8h\n"
          "ld1 {v13.8b}, [x15]\n"
          "smlal v23.4s, v4.4h, v17.4h\n"
          "uaddw v9.8h, v28.8h, v9.8b\n"
          "smlal2 v24.4s, v4.8h, v17.8h\n"
          "ld1 {v17.8b}, [x12], %[input_depth]\n"
          "smlal v23.4s, v5.4h, v18.4h\n"
          "uaddw v10.8h, v28.8h, v10.8b\n"
          "smlal2 v24.4s, v5.8h, v18.8h\n"
          "ld1 {v18.8b}, [x12]\n"

          "smlal v21.4s, v6.4h, v9.4h\n"
          "smlal2 v22.4s, v6.8h, v9.8h\n"
          "smlal v19.4s, v0.4h, v9.4h\n"
          "uaddw v11.8h, v28.8h, v11.8b\n"
          "smlal2 v20.4s, v0.8h, v9.8h\n"
          "ld1 {v9.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v6.4h, v11.4h\n"
          "smlal2 v24.4s, v6.8h, v11.8h\n"
          "smlal v21.4s, v7.4h, v10.4h\n"
          "smlal2 v22.4s, v7.8h, v10.8h\n"
          "uaddw v12.8h, v28.8h, v12.8b\n"
          "smlal v19.4s, v1.4h, v10.4h\n"
          "smlal2 v20.4s, v1.8h, v10.8h\n"
          "ld1 {v10.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v7.4h, v12.4h\n"
          "smlal2 v24.4s, v7.8h, v12.8h\n"
          "smlal v25.4s, v1.4h, v12.4h\n"
          "smlal2 v26.4s, v1.8h, v12.8h\n"
          "smlal v21.4s, v8.4h, v11.4h\n"
          "smlal2 v22.4s, v8.8h, v11.8h\n"
          "smlal v19.4s, v2.4h, v11.4h\n"
          "smlal2 v20.4s, v2.8h, v11.8h\n"
          "uaddw v13.8h, v28.8h, v13.8b\n"
          "smlal v25.4s, v0.4h, v11.4h\n"
          "smlal2 v26.4s, v0.8h, v11.8h\n"
          "ld1 {v11.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v8.4h, v13.4h\n"
          "ld1 {v12.8b}, [x13], %[input_depth]\n"
          "smlal2 v24.4s, v8.8h, v13.8h\n"
          "smlal v25.4s, v2.4h, v13.4h\n"
          "smlal2 v26.4s, v2.8h, v13.8h\n"
          "ld1 {v13.8b}, [x13]\n"

          "dup v28.4s, w9\n"
          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "and v27.16b, v21.16b, v28.16b\n"
          "and v29.16b, v22.16b, v28.16b\n"
          "and v30.16b, v23.16b, v28.16b\n"
          "and v31.16b, v24.16b, v28.16b\n"
          "sshr v27.4s, v27.4s, #31\n"
          "sshr v29.4s, v29.4s, #31\n"
          "sshr v30.4s, v30.4s, #31\n"
          "sshr v31.4s, v31.4s, #31\n"
          "sqadd v21.4s, v21.4s, v27.4s\n"
          "dup v27.4s, w1\n"
          "sqadd v22.4s, v22.4s, v29.4s\n"
          "dup v29.8h, w2\n"
          "sqadd v23.4s, v23.4s, v30.4s\n"
          "dup v30.16b, w3\n"
          "sqadd v24.4s, v24.4s, v31.4s\n"
          "dup v31.16b, w4\n"
          "srshl v21.4s, v21.4s, v28.4s\n"
          "srshl v22.4s, v22.4s, v28.4s\n"
          "srshl v23.4s, v23.4s, v28.4s\n"
          "srshl v24.4s, v24.4s, v28.4s\n"
          "dup v28.8h, w0\n"
          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "sqxtun2 v21.16b, v23.8h\n"
          "ld1 {v22.4s}, [x10]\n"
          "umax v21.16b, v21.16b, v30.16b\n"
          "umin v21.16b, v21.16b, v31.16b\n"
          "ld1 {v24.4s}, [x10]\n"
          "uaddw v9.8h, v28.8h, v9.8b\n"
          "st1 {v21.8b}, [x6], x5\n"
          "uaddw v10.8h, v28.8h, v10.8b\n"
          "mov v23.d[0], v21.d[1]\n"
          "st1 {v23.8b}, [x6]\n"
          "uaddw v11.8h, v28.8h, v11.8b\n"

          "smlal v19.4s, v6.4h, v9.4h\n"
          "smlal2 v20.4s, v6.8h, v9.8h\n"
          "smlal v25.4s, v6.4h, v11.4h\n"
          "smlal2 v26.4s, v6.8h, v11.8h\n"
          "smlal v19.4s, v7.4h, v10.4h\n"
          "uaddw v12.8h, v28.8h, v12.8b\n"
          "smlal2 v20.4s, v7.8h, v10.8h\n"
          "smlal v25.4s, v7.4h, v12.4h\n"
          "smlal2 v26.4s, v7.8h, v12.8h\n"
          "smlal v19.4s, v8.4h, v11.4h\n"
          "uaddw v13.8h, v28.8h, v13.8b\n"
          "smlal2 v20.4s, v8.8h, v11.8h\n"
          "smlal v25.4s, v8.4h, v13.4h\n"
          "uaddw v14.8h, v28.8h, v14.8b\n"
          "smlal2 v26.4s, v8.8h, v13.8h\n"
          "uaddw v16.8h, v28.8h, v16.8b\n"
          "smlal v19.4s, v3.4h, v14.4h\n"
          "uaddw v15.8h, v28.8h, v15.8b\n"
          "smlal2 v20.4s, v3.8h, v14.8h\n"
          "smlal v25.4s, v3.4h, v16.4h\n"
          "smlal2 v26.4s, v3.8h, v16.8h\n"
          "smlal v19.4s, v4.4h, v15.4h\n"
          "uaddw v17.8h, v28.8h, v17.8b\n"
          "smlal2 v20.4s, v4.8h, v15.8h\n"
          "smlal v25.4s, v4.4h, v17.4h\n"
          "smlal2 v26.4s, v4.8h, v17.8h\n"
          "smlal v19.4s, v5.4h, v16.4h\n"
          "uaddw v18.8h, v28.8h, v18.8b\n"
          "smlal2 v20.4s, v5.8h, v16.8h\n"
          "smlal v25.4s, v5.4h, v18.4h\n"
          "smlal2 v26.4s, v5.8h, v18.8h\n"

          "dup v28.4s, w9\n"
          "sqrdmulh v19.4s, v19.4s, v27.4s\n"
          "sqrdmulh v20.4s, v20.4s, v27.4s\n"
          "sqrdmulh v25.4s, v25.4s, v27.4s\n"
          "sqrdmulh v26.4s, v26.4s, v27.4s\n"
          "and v27.16b, v19.16b, v28.16b\n"
          "and v29.16b, v20.16b, v28.16b\n"
          "and v30.16b, v25.16b, v28.16b\n"
          "and v31.16b, v26.16b, v28.16b\n"
          "sshr v27.4s, v27.4s, #31\n"
          "sshr v29.4s, v29.4s, #31\n"
          "sshr v30.4s, v30.4s, #31\n"
          "sshr v31.4s, v31.4s, #31\n"
          "sqadd v19.4s, v19.4s, v27.4s\n"
          "dup v27.4s, w1\n"
          "sqadd v20.4s, v20.4s, v29.4s\n"
          "dup v29.8h, w2\n"
          "sqadd v25.4s, v25.4s, v30.4s\n"
          "dup v30.16b, w3\n"
          "sqadd v26.4s, v26.4s, v31.4s\n"
          "dup v31.16b, w4\n"
          "srshl v19.4s, v19.4s, v28.4s\n"
          "srshl v20.4s, v20.4s, v28.4s\n"
          "srshl v25.4s, v25.4s, v28.4s\n"
          "srshl v26.4s, v26.4s, v28.4s\n"
          "dup v28.8h, w0\n"
          "sqxtn v19.4h, v19.4s\n"
          "sqxtn2 v19.8h, v20.4s\n"
          "sqxtn v25.4h, v25.4s\n"
          "sqxtn2 v25.8h, v26.4s\n"
          "sqadd v19.8h, v19.8h, v29.8h\n"
          "sqadd v25.8h, v25.8h, v29.8h\n"
          "sqxtun v19.8b, v19.8h\n"
          "sqxtun2 v19.16b, v25.8h\n"
          "umax v19.16b, v19.16b, v30.16b\n"
          "umin v19.16b, v19.16b, v31.16b\n"
          "st1 {v19.8b}, [x7], x5\n"
          "mov v25.d[0], v19.d[1]\n"
          "st1 {v25.8b}, [x7]\n"
          "b " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP "f\n"

          // Handle last column if exists.
          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER ":\n"
          // Registers v9, v10, v11, v14, v15, and v16 have already been loaded
          // with the correct values at this point. This corresponds to the
          // first two input rows of the top left output. Now load the last
          // input row for this output. Once these inputs are no longer needed,
          // load the input rows for the bottom left output.
          "add x12, x15, %[input_row_size]\n"
          "add x13, x12, %[input_row_size]\n"

          "ld1 {v12.8b}, [x15], %[input_depth]\n"
          "smlal v21.4s, v0.4h, v9.4h\n"
          "ld1 {v13.8b}, [x15], %[input_depth]\n"
          "smlal2 v22.4s, v0.8h, v9.8h\n"
          "ld1 {v17.8b}, [x15]\n"
          "smlal v21.4s, v1.4h, v10.4h\n"
          "ld1 {v9.8b}, [x12], %[input_depth]\n"
          "smlal2 v22.4s, v1.8h, v10.8h\n"
          "ld1 {v10.8b}, [x12], %[input_depth]\n"
          "smlal v21.4s, v2.4h, v11.4h\n"
          "smlal2 v22.4s, v2.8h, v11.8h\n"
          "ld1 {v11.8b}, [x12]\n"
          "smlal v21.4s, v3.4h, v14.4h\n"
          "smlal2 v22.4s, v3.8h, v14.8h\n"
          "ld1 {v14.8b}, [x13], %[input_depth]\n"
          "smlal v21.4s, v4.4h, v15.4h\n"
          "smlal2 v22.4s, v4.8h, v15.8h\n"
          "ld1 {v15.8b}, [x13], %[input_depth]\n"
          "smlal v21.4s, v5.4h, v16.4h\n"
          "uaddw v12.8h, v28.8h, v12.8b\n"
          "smlal2 v22.4s, v5.8h, v16.8h\n"
          "uaddw v13.8h, v28.8h, v13.8b\n"
          "ld1 {v16.8b}, [x13]\n"

          "smlal v21.4s, v6.4h, v12.4h\n"
          "smlal2 v22.4s, v6.8h, v12.8h\n"
          "smlal v23.4s, v0.4h, v12.4h\n"
          "uaddw v17.8h, v28.8h, v17.8b\n"
          "smlal2 v24.4s, v0.8h, v12.8h\n"
          "smlal v21.4s, v7.4h, v13.4h\n"
          "smlal2 v22.4s, v7.8h, v13.8h\n"
          "smlal v23.4s, v1.4h, v13.4h\n"
          "smlal2 v24.4s, v1.8h, v13.8h\n"
          "smlal v21.4s, v8.4h, v17.4h\n"
          "smlal2 v22.4s, v8.8h, v17.8h\n"
          "smlal v23.4s, v2.4h, v17.4h\n"
          "smlal2 v24.4s, v2.8h, v17.8h\n"

          "dup v26.4s, w9\n"
          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "and v18.16b, v21.16b, v26.16b\n"
          "and v19.16b, v22.16b, v26.16b\n"
          "sshr v18.4s, v18.4s, #31\n"
          "sshr v19.4s, v19.4s, #31\n"
          "sqadd v21.4s, v21.4s, v18.4s\n"
          "sqadd v22.4s, v22.4s, v19.4s\n"
          "srshl v21.4s, v21.4s, v26.4s\n"
          "srshl v22.4s, v22.4s, v26.4s\n"
          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "umax v21.8b, v21.8b, v30.8b\n"
          "umin v21.8b, v21.8b, v31.8b\n"
          "uaddw v9.8h, v28.8h, v9.8b\n"
          "st1 {v21.8b}, [x6]\n"
          "uaddw v10.8h, v28.8h, v10.8b\n"

          "smlal v23.4s, v3.4h, v9.4h\n"
          "uaddw v11.8h, v28.8h, v11.8b\n"
          "smlal2 v24.4s, v3.8h, v9.8h\n"
          "uaddw v14.8h, v28.8h, v14.8b\n"
          "smlal v23.4s, v4.4h, v10.4h\n"
          "uaddw v15.8h, v28.8h, v15.8b\n"
          "smlal2 v24.4s, v4.8h, v10.8h\n"
          "uaddw v16.8h, v28.8h, v16.8b\n"
          "smlal v23.4s, v5.4h, v11.4h\n"
          "smlal2 v24.4s, v5.8h, v11.8h\n"

          "smlal v23.4s, v6.4h, v14.4h\n"
          "smlal2 v24.4s, v6.8h, v14.8h\n"
          "smlal v23.4s, v7.4h, v15.4h\n"
          "smlal2 v24.4s, v7.8h, v15.8h\n"
          "smlal v23.4s, v8.4h, v16.4h\n"
          "smlal2 v24.4s, v8.8h, v16.8h\n"

          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "and v18.16b, v23.16b, v26.16b\n"
          "and v19.16b, v24.16b, v26.16b\n"
          "sshr v18.4s, v18.4s, #31\n"
          "sshr v19.4s, v19.4s, #31\n"
          "sqadd v23.4s, v23.4s, v18.4s\n"
          "sqadd v24.4s, v24.4s, v19.4s\n"
          "srshl v23.4s, v23.4s, v26.4s\n"
          "srshl v24.4s, v24.4s, v26.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v23.8b, v23.8h\n"
          "umax v23.8b, v23.8b, v30.8b\n"
          "umin v23.8b, v23.8b, v31.8b\n"
          "st1 {v23.8b}, [x7]\n"

          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP ":\n"
          "subs %w[output_window_height], %w[output_window_height], #2\n"
          "add %[input_ptr], %[input_ptr], %[input_height_increment]\n"
          "cmp %w[output_window_height], #2\n"
          "add %[output_ptr], %[output_ptr], %[output_height_increment]\n"
          "bge " DEPTHWISECONV_LABEL_HEIGHT_2_LOOP "b\n"

        DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP ":\n"
        "cmp %w[output_window_height], #1\n"
        "blt " DEPTHWISECONV_LABEL_HEIGHT_1_END "f\n"

        DEPTHWISECONV_LABEL_HEIGHT_1 ":\n"
        "mov x11, %[input_ptr]\n"
        "mov x12, x11\n"
        "add x13, x12, %[input_row_size]\n"
        "ld1 {v9.8b}, [x12], %[input_depth]\n"
        "add x15, x13, %[input_row_size]\n"
        "ld1 {v10.8b}, [x12], %[input_depth]\n"
        "mov x6, %[output_ptr]\n"
        "ld1 {v11.8b}, [x12], %[input_depth]\n"
        "mov w14, %w[output_window_width]\n"
        // The height 1 / width 2 loop loads an extra 1x1 output in anticipation
        // for the next iteration. Make sure |output_window_width| is large
        // enough to handle the additional load, otherwise jump to the
        // appropriate label to handle smaller widths.
        "cmp w14, #2\n"
        "ld1 {v12.8b}, [x13], %[input_depth]\n"
        "ld1 {v13.8b}, [x13], %[input_depth]\n"
        "ld1 {v14.8b}, [x13], %[input_depth]\n"
        "ld1 {v15.8b}, [x15], %[input_depth]\n"
        "ld1 {v16.8b}, [x15], %[input_depth]\n"
        "ld1 {v17.8b}, [x15], %[input_depth]\n"

        "uaddw v9.8h, v28.8h, v9.8b\n"
        "ld1 {v24.4s}, [%[bias_ptr]]\n"
        "uaddw v10.8h, v28.8h, v10.8b\n"
        "ld1 {v25.4s}, [x10]\n"
        "uaddw v11.8h, v28.8h, v11.8b\n"
        "ld1 {v26.4s}, [%[bias_ptr]]\n"
        "ld1 {v27.4s}, [x10]\n"
        "uaddw v12.8h, v28.8h, v12.8b\n"
        "uaddw v13.8h, v28.8h, v13.8b\n"
        "uaddw v14.8h, v28.8h, v14.8b\n"
        "uaddw v15.8h, v28.8h, v15.8b\n"
        "uaddw v16.8h, v28.8h, v16.8b\n"
        "uaddw v17.8h, v28.8h, v17.8b\n"

        "beq " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER "f\n"
        "cmp w14, #1\n"
        "beq " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP ":\n"
          "smlal v24.4s, v0.4h, v9.4h\n"
          "ld1 {v18.8b}, [x12], %[input_depth]\n"
          "smlal2 v25.4s, v0.8h, v9.8h\n"
          "ld1 {v19.8b}, [x12]\n"
          "smlal v26.4s, v0.4h, v11.4h\n"
          "ld1 {v20.8b}, [x13], %[input_depth]\n"
          "smlal2 v27.4s, v0.8h, v11.8h\n"
          "ld1 {v21.8b}, [x13]\n"
          "smlal v24.4s, v1.4h, v10.4h\n"
          "ld1 {v22.8b}, [x15], %[input_depth]\n"
          "smlal2 v25.4s, v1.8h, v10.8h\n"
          "ld1 {v23.8b}, [x15]\n"
          "smlal v24.4s, v2.4h, v11.4h\n"
          "subs w14, w14, #2\n"
          "smlal2 v25.4s, v2.8h, v11.8h\n"
          "cmp w14, #3\n"
          "smlal v24.4s, v3.4h, v12.4h\n"
          "add x11, x11, %[input_width_increment]\n"
          "smlal2 v25.4s, v3.8h, v12.8h\n"
          "mov x12, x11\n"
          "smlal v26.4s, v3.4h, v14.4h\n"
          "add x13, x12, %[input_row_size]\n"
          "smlal2 v27.4s, v3.8h, v14.8h\n"
          "add x15, x13, %[input_row_size]\n"
          "smlal v24.4s, v4.4h, v13.4h\n"
          "ld1 {v9.8b}, [x12], %[input_depth]\n"
          "smlal2 v25.4s, v4.8h, v13.8h\n"
          "ld1 {v10.8b}, [x12], %[input_depth]\n"
          "smlal v24.4s, v5.4h, v14.4h\n"
          "ld1 {v11.8b}, [x12], %[input_depth]\n"
          "smlal2 v25.4s, v5.8h, v14.8h\n"
          "ld1 {v12.8b}, [x13], %[input_depth]\n"
          "smlal v24.4s, v6.4h, v15.4h\n"
          "ld1 {v13.8b}, [x13], %[input_depth]\n"
          "smlal2 v25.4s, v6.8h, v15.8h\n"
          "ld1 {v14.8b}, [x13], %[input_depth]\n"
          "smlal v26.4s, v6.4h, v17.4h\n"
          "ld1 {v15.8b}, [x15], %[input_depth]\n"
          "smlal2 v27.4s, v6.8h, v17.8h\n"
          "smlal v24.4s, v7.4h, v16.4h\n"
          "smlal2 v25.4s, v7.8h, v16.8h\n"
          "ld1 {v16.8b}, [x15], %[input_depth]\n"
          "smlal v24.4s, v8.4h, v17.4h\n"
          "uaddw v18.8h, v28.8h, v18.8b\n"
          "smlal2 v25.4s, v8.8h, v17.8h\n"
          "ld1 {v17.8b}, [x15], %[input_depth]\n"
          "uaddw v19.8h, v28.8h, v19.8b\n"

          "smlal v26.4s, v1.4h, v18.4h\n"
          "uaddw v20.8h, v28.8h, v20.8b\n"
          "smlal2 v27.4s, v1.8h, v18.8h\n"
          "smlal v26.4s, v2.4h, v19.4h\n"
          "uaddw v21.8h, v28.8h, v21.8b\n"
          "smlal2 v27.4s, v2.8h, v19.8h\n"
          "smlal v26.4s, v4.4h, v20.4h\n"
          "smlal v26.4s, v5.4h, v21.4h\n"
          "smlal2 v27.4s, v4.8h, v20.8h\n"
          "uaddw v22.8h, v28.8h, v22.8b\n"
          "smlal2 v27.4s, v5.8h, v21.8h\n"
          "uaddw v23.8h, v28.8h, v23.8b\n"
          "smlal v26.4s, v7.4h, v22.4h\n"
          "smlal2 v27.4s, v7.8h, v22.8h\n"
          "smlal v26.4s, v8.4h, v23.4h\n"
          "smlal2 v27.4s, v8.8h, v23.8h\n"

          "dup v28.4s, w1\n"
          "dup v29.4s, w9\n"
          "sqrdmulh v24.4s, v24.4s, v28.4s\n"
          "sqrdmulh v25.4s, v25.4s, v28.4s\n"
          "sqrdmulh v26.4s, v26.4s, v28.4s\n"
          "sqrdmulh v27.4s, v27.4s, v28.4s\n"
          "dup v28.8h, w2\n"
          "and v30.16b, v24.16b, v29.16b\n"
          "and v31.16b, v25.16b, v29.16b\n"
          "sshr v30.4s, v30.4s, #31\n"
          "sshr v31.4s, v31.4s, #31\n"
          "sqadd v24.4s, v24.4s, v30.4s\n"
          "sqadd v25.4s, v25.4s, v31.4s\n"
          "and v30.16b, v26.16b, v29.16b\n"
          "and v31.16b, v27.16b, v29.16b\n"
          "sshr v30.4s, v30.4s, #31\n"
          "sshr v31.4s, v31.4s, #31\n"
          "sqadd v26.4s, v26.4s, v30.4s\n"
          "dup v30.16b, w3\n"
          "sqadd v27.4s, v27.4s, v31.4s\n"
          "dup v31.16b, w4\n"
          "srshl v24.4s, v24.4s, v29.4s\n"
          "srshl v25.4s, v25.4s, v29.4s\n"
          "srshl v26.4s, v26.4s, v29.4s\n"
          "srshl v27.4s, v27.4s, v29.4s\n"
          "sqxtn v24.4h, v24.4s\n"
          "sqxtn2 v24.8h, v25.4s\n"
          "sqxtn v26.4h, v26.4s\n"
          "sqxtn2 v26.8h, v27.4s\n"
          "sqadd v24.8h, v24.8h, v28.8h\n"
          "sqadd v26.8h, v26.8h, v28.8h\n"
          "sqxtun v24.8b, v24.8h\n"
          "sqxtun2 v24.16b, v26.8h\n"
          "dup v28.8h, w0\n"
          "ld1 {v25.4s}, [x10]\n"
          "umax v24.16b, v24.16b, v30.16b\n"
          "umin v24.16b, v24.16b, v31.16b\n"
          "ld1 {v27.4s}, [x10]\n"
          "uaddw v9.8h, v28.8h, v9.8b\n"
          "st1 {v24.8b}, [x6], x5\n"
          "uaddw v10.8h, v28.8h, v10.8b\n"
          "mov v26.d[0], v24.d[1]\n"
          "st1 {v26.8b}, [x6], x5\n"
          "uaddw v11.8h, v28.8h, v11.8b\n"
          "uaddw v12.8h, v28.8h, v12.8b\n"
          "uaddw v13.8h, v28.8h, v13.8b\n"
          "uaddw v14.8h, v28.8h, v14.8b\n"
          "ld1 {v24.4s}, [%[bias_ptr]]\n"
          "uaddw v15.8h, v28.8h, v15.8b\n"
          "ld1 {v26.4s}, [%[bias_ptr]]\n"
          "uaddw v16.8h, v28.8h, v16.8b\n"
          "uaddw v17.8h, v28.8h, v17.8b\n"

          "bge " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP "b\n"

        // At this point, there will be one of 2 width or 1 width leftover,
        // not both.
        "cmp w14, #2\n"
        "blt " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "f\n"

        // Handle last two horizontal outputs if exists.
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER ":\n"
        "smlal v24.4s, v0.4h, v9.4h\n"
        "ld1 {v18.8b}, [x12], %[input_depth]\n"
        "smlal2 v25.4s, v0.8h, v9.8h\n"
        "ld1 {v19.8b}, [x12]\n"
        "smlal v26.4s, v0.4h, v11.4h\n"
        "ld1 {v20.8b}, [x13], %[input_depth]\n"
        "smlal2 v27.4s, v0.8h, v11.8h\n"
        "ld1 {v21.8b}, [x13]\n"
        "smlal v24.4s, v1.4h, v10.4h\n"
        "ld1 {v22.8b}, [x15], %[input_depth]\n"
        "smlal2 v25.4s, v1.8h, v10.8h\n"
        "ld1 {v23.8b}, [x15]\n"
        "smlal v24.4s, v2.4h, v11.4h\n"
        "smlal2 v25.4s, v2.8h, v11.8h\n"
        "smlal v24.4s, v3.4h, v12.4h\n"
        "smlal2 v25.4s, v3.8h, v12.8h\n"
        "smlal v26.4s, v3.4h, v14.4h\n"
        "smlal2 v27.4s, v3.8h, v14.8h\n"
        "smlal v24.4s, v4.4h, v13.4h\n"
        "smlal2 v25.4s, v4.8h, v13.8h\n"
        "smlal v24.4s, v5.4h, v14.4h\n"
        "smlal2 v25.4s, v5.8h, v14.8h\n"
        "smlal v24.4s, v6.4h, v15.4h\n"
        "smlal2 v25.4s, v6.8h, v15.8h\n"
        "smlal v26.4s, v6.4h, v17.4h\n"
        "smlal2 v27.4s, v6.8h, v17.8h\n"
        "smlal v24.4s, v7.4h, v16.4h\n"
        "smlal2 v25.4s, v7.8h, v16.8h\n"
        "smlal v24.4s, v8.4h, v17.4h\n"
        "uaddw v18.8h, v28.8h, v18.8b\n"
        "smlal2 v25.4s, v8.8h, v17.8h\n"
        "uaddw v19.8h, v28.8h, v19.8b\n"

        "smlal v26.4s, v1.4h, v18.4h\n"
        "uaddw v20.8h, v28.8h, v20.8b\n"
        "smlal2 v27.4s, v1.8h, v18.8h\n"
        "smlal v26.4s, v2.4h, v19.4h\n"
        "uaddw v21.8h, v28.8h, v21.8b\n"
        "smlal2 v27.4s, v2.8h, v19.8h\n"
        "smlal v26.4s, v4.4h, v20.4h\n"
        "smlal v26.4s, v5.4h, v21.4h\n"
        "smlal2 v27.4s, v4.8h, v20.8h\n"
        "uaddw v22.8h, v28.8h, v22.8b\n"
        "smlal2 v27.4s, v5.8h, v21.8h\n"
        "uaddw v23.8h, v28.8h, v23.8b\n"
        "smlal v26.4s, v7.4h, v22.4h\n"
        "smlal2 v27.4s, v7.8h, v22.8h\n"
        "smlal v26.4s, v8.4h, v23.4h\n"
        "smlal2 v27.4s, v8.8h, v23.8h\n"

        "dup v28.4s, w1\n"
        "dup v29.4s, w9\n"
        "sqrdmulh v24.4s, v24.4s, v28.4s\n"
        "sqrdmulh v25.4s, v25.4s, v28.4s\n"
        "sqrdmulh v26.4s, v26.4s, v28.4s\n"
        "sqrdmulh v27.4s, v27.4s, v28.4s\n"
        "dup v28.8h, w2\n"
        "and v30.16b, v24.16b, v29.16b\n"
        "and v31.16b, v25.16b, v29.16b\n"
        "sshr v30.4s, v30.4s, #31\n"
        "sshr v31.4s, v31.4s, #31\n"
        "sqadd v24.4s, v24.4s, v30.4s\n"
        "sqadd v25.4s, v25.4s, v31.4s\n"
        "and v30.16b, v26.16b, v29.16b\n"
        "and v31.16b, v27.16b, v29.16b\n"
        "sshr v30.4s, v30.4s, #31\n"
        "sshr v31.4s, v31.4s, #31\n"
        "sqadd v26.4s, v26.4s, v30.4s\n"
        "dup v30.16b, w3\n"
        "sqadd v27.4s, v27.4s, v31.4s\n"
        "dup v31.16b, w4\n"
        "srshl v24.4s, v24.4s, v29.4s\n"
        "srshl v25.4s, v25.4s, v29.4s\n"
        "srshl v26.4s, v26.4s, v29.4s\n"
        "srshl v27.4s, v27.4s, v29.4s\n"
        "sqxtn v24.4h, v24.4s\n"
        "sqxtn2 v24.8h, v25.4s\n"
        "sqxtn v26.4h, v26.4s\n"
        "sqxtn2 v26.8h, v27.4s\n"
        "sqadd v24.8h, v24.8h, v28.8h\n"
        "sqadd v26.8h, v26.8h, v28.8h\n"
        "sqxtun v24.8b, v24.8h\n"
        "sqxtun2 v24.16b, v26.8h\n"
        "dup v28.8h, w0\n"
        "umax v24.16b, v24.16b, v30.16b\n"
        "umin v24.16b, v24.16b, v31.16b\n"
        "st1 {v24.8b}, [x6], x5\n"
        "mov v26.d[0], v24.d[1]\n"
        "st1 {v26.8b}, [x6]\n"
        "b " DEPTHWISECONV_LABEL_HEIGHT_1_END "f\n"

        // Handle bottom right output if exists.
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER ":\n"
        "dup v26.4s, w9\n"
        "dup v27.4s, w1\n"
        "dup v29.8h, w2\n"

        "smlal v24.4s, v0.4h, v9.4h\n"
        "smlal2 v25.4s, v0.8h, v9.8h\n"
        "smlal v24.4s, v1.4h, v10.4h\n"
        "smlal2 v25.4s, v1.8h, v10.8h\n"
        "smlal v24.4s, v2.4h, v11.4h\n"
        "smlal2 v25.4s, v2.8h, v11.8h\n"
        "smlal v24.4s, v3.4h, v12.4h\n"
        "smlal2 v25.4s, v3.8h, v12.8h\n"
        "smlal v24.4s, v4.4h, v13.4h\n"
        "smlal2 v25.4s, v4.8h, v13.8h\n"
        "smlal v24.4s, v5.4h, v14.4h\n"
        "smlal2 v25.4s, v5.8h, v14.8h\n"
        "smlal v24.4s, v6.4h, v15.4h\n"
        "smlal2 v25.4s, v6.8h, v15.8h\n"
        "smlal v24.4s, v7.4h, v16.4h\n"
        "smlal2 v25.4s, v7.8h, v16.8h\n"
        "smlal v24.4s, v8.4h, v17.4h\n"
        "smlal2 v25.4s, v8.8h, v17.8h\n"

        "sqrdmulh v24.4s, v24.4s, v27.4s\n"
        "sqrdmulh v25.4s, v25.4s, v27.4s\n"
        "and v18.16b, v24.16b, v26.16b\n"
        "and v19.16b, v25.16b, v26.16b\n"
        "sshr v18.4s, v18.4s, #31\n"
        "sshr v19.4s, v19.4s, #31\n"
        "sqadd v24.4s, v24.4s, v18.4s\n"
        "sqadd v25.4s, v25.4s, v19.4s\n"
        "srshl v24.4s, v24.4s, v26.4s\n"
        "srshl v25.4s, v25.4s, v26.4s\n"
        "sqxtn v24.4h, v24.4s\n"
        "sqxtn2 v24.8h, v25.4s\n"
        "sqadd v24.8h, v24.8h, v29.8h\n"
        "sqxtun v24.8b, v24.8h\n"
        "umax v24.8b, v24.8b, v30.8b\n"
        "umin v24.8b, v24.8b, v31.8b\n"
        "st1 {v24.8b}, [x6]\n"

        DEPTHWISECONV_LABEL_HEIGHT_1_END ":\n"
    :
    // Outputs.
    [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
    [output_ptr] "+r"(output_ptr),
    [output_window_height] "+r"(output_window_height)
    :
    // Inputs.
    [bias_ptr] "r"(bias_ptr), [input_row_size] "r"(input_row_size),
    [input_depth] "r"(input_depth),
    [output_window_width] "r"(output_window_width),
    [input_width_increment] "r"(input_width_increment),
    [input_height_increment] "r"(input_height_increment),
    [output_height_increment] "r"(output_height_increment),
    [params_ptr] "r"(params_ptr)
    :
    // Clobbers.
    "cc", "memory",
    // We use these NEON registers.
    "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9",
    "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19",
    "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
    "v30", "v31",
    // We use these general-purpose registers.
    "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
    "x9", "x10", "x11", "x12", "x13", "x14", "x15",
    "x19", "x20");
#undef DEPTHWISECONV_LABEL_HEIGHT_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_1
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_1_END
  }
};

template <>
struct DepthwiseConvWindow<DepthwiseConvOutputRounding::kUpward, 8, 2, 2> {
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         int64_t input_depth, int64_t input_row_size,
                         int32 output_window_height, int32 output_window_width,
                         const DepthwiseConvParams* params_ptr) {
    const int64_t input_width_increment = 4 * input_depth;
    const int64_t input_height_increment = 4 * input_row_size;
    const int64_t output_height_increment = 2 * params_ptr->output_row_size;

#define DEPTHWISECONV_LABEL_HEIGHT_2_LOOP "1"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP "2"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "3"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER "4"
#define DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP "5"
#define DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP "6"
#define DEPTHWISECONV_LABEL_HEIGHT_1 "7"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP "8"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "9"
#define DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER "10"
#define DEPTHWISECONV_LABEL_HEIGHT_1_END "11"

    asm volatile(
        // Performs depthwise convolutions for a window specified by
        // |output_window_height| and |output_window_width|. The inner-most loop
        // processes 2x2 outputs, and any leftovers at the end.
        //
        // Algorithm works as follows:
        //
        //   1. Load filters of 8 depth (8x3x3). Registers v0--v8 hold filter
        //      values.
        //   2. For 2 output heights at a time:
        //        i.  For 2 output widths at a time at stride 2, a 5x5 input
        //            window is required. To avoid register exhaustion, we load
        //            the first 2 rows of the 5x5 input window into registers
        //            v9--v18, and use the same registers to load the next 2
        //            rows, and finally v9--v13 to load the last row.
        //            Accumulators for all 2x2 outputs are reserved by registers
        //            v21-v22 (top left output), v23-v24 (top right output),
        //            v19-v20 (bottom left output), v25-v26 (bottom right
        //            output).
        //        ii. Handle single leftover width if exists.
        //   3. Handle single leftover height if exists.
        //        i.  For 2 output widths at a time at stride 2, load inputs for
        //            a 1x2 (1 height, 2 width) output window (3x5 input
        //            window). Registers v9--v24 hold input values. Mul-add with
        //            accumulators v24--v27.
        //        ii. Handle single leftover width if exists.
        //
        // Loads are placed as soon as the register is no longer needed and
        // interleaved with arithmetic operations to take advantage of
        // dual-issue pipelines. We also add input offsets as far from the loads
        // as possible to give loads enough cycles to fetch data from memory.

        // Set "constant" registers. These registers may be replaced with temp
        // values from time to time when there are not enough NEON registers.
        // We use x9--x15 general purpose registers as they are caller-saved
        // temporary registers (see http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf).  // NOLINT
        "ldr w9, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "ldr w0, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "cmp %w[output_window_height], #2\n"
        "dup v28.8h, w0\n"
        "ldr w1, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "dup v26.4s, w9\n"
        "ldr w2, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w1\n"
        "ldr w3, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v29.8h, w2\n"
        "ldr w4, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v30.16b, w3\n"
        "ldr x5, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"
        "dup v31.16b, w4\n"
        "ldr x19, [%[params_ptr], #" STR(OFFSET_OUTPUT_ROW_SIZE) "]\n"
        "ldr w20, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"

        // Load filters and add offsets.
        "add x10, %[bias_ptr], #16\n"
        "ld1 {v0.8b}, [%[filter_ptr]], x5\n"
        "dup v9.8h, w20\n"
        "ld1 {v1.8b}, [%[filter_ptr]], x5\n"
        "uaddw v0.8h, v9.8h, v0.8b\n"
        "ld1 {v2.8b}, [%[filter_ptr]], x5\n"
        "uaddw v1.8h, v9.8h, v1.8b\n"
        "ld1 {v3.8b}, [%[filter_ptr]], x5\n"
        "uaddw v2.8h, v9.8h, v2.8b\n"
        "ld1 {v4.8b}, [%[filter_ptr]], x5\n"
        "uaddw v3.8h, v9.8h, v3.8b\n"
        "ld1 {v5.8b}, [%[filter_ptr]], x5\n"
        "uaddw v4.8h, v9.8h, v4.8b\n"
        "ld1 {v6.8b}, [%[filter_ptr]], x5\n"
        "uaddw v5.8h, v9.8h, v5.8b\n"
        "ld1 {v7.8b}, [%[filter_ptr]], x5\n"
        "uaddw v6.8h, v9.8h, v6.8b\n"
        "ld1 {v8.8b}, [%[filter_ptr]]\n"
        "uaddw v7.8h, v9.8h, v7.8b\n"
        "uaddw v8.8h, v9.8h, v8.8b\n"

        "blt " DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_HEIGHT_2_LOOP ":\n"
          // Load the first two rows of the 5x5 input window, then reuse the
          // same registers to load subsequent rows as they become available.
          "mov x11, %[input_ptr]\n"
          "mov x12, x11\n"
          "add x13, x12, %[input_row_size]\n"
          "ld1 {v9.8b}, [x12], %[input_depth]\n"
          "mov w14, %w[output_window_width]\n"
          "ld1 {v10.8b}, [x12], %[input_depth]\n"
          // The height 2 / width 2 loop loads an extra 1 output horizontally in
          // anticipation for the next iteration. Make sure
          // |output_window_width| is large enough to handle the additional
          // load, otherwise jump to the appropriate label to handle smaller
          // widths.
          "cmp w14, #2\n"
          "ld1 {v11.8b}, [x12], %[input_depth]\n"
          "add x15, x13, %[input_row_size]\n"
          "ld1 {v14.8b}, [x13], %[input_depth]\n"
          "mov x6, %[output_ptr]\n"
          "ld1 {v15.8b}, [x13], %[input_depth]\n"
          "add x7, %[output_ptr], x19\n"
          "ld1 {v16.8b}, [x13], %[input_depth]\n"
          "ld1 {v21.4s}, [%[bias_ptr]]\n"
          "ld1 {v22.4s}, [x10]\n"
          "ld1 {v23.4s}, [%[bias_ptr]]\n"
          "uaddw v9.8h, v28.8h, v9.8b\n"
          "ld1 {v24.4s}, [x10]\n"
          "uaddw v10.8h, v28.8h, v10.8b\n"
          "ld1 {v19.4s}, [%[bias_ptr]]\n"
          "uaddw v11.8h, v28.8h, v11.8b\n"
          "ld1 {v20.4s}, [x10]\n"
          "uaddw v14.8h, v28.8h, v14.8b\n"
          "ld1 {v25.4s}, [%[bias_ptr]]\n"
          "uaddw v15.8h, v28.8h, v15.8b\n"
          "ld1 {v26.4s}, [x10]\n"
          "uaddw v16.8h, v28.8h, v16.8b\n"

          "beq " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER "f\n"
          "cmp w14, #1\n"
          "beq " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "f\n"

          //"loop_%=:\n"
          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP ":\n"
            "smlal v21.4s, v0.4h, v9.4h\n"
            "ld1 {v12.8b}, [x12], %[input_depth]\n"
            "smlal2 v22.4s, v0.8h, v9.8h\n"
            "ld1 {v13.8b}, [x12]\n"
            "add x12, x15, %[input_row_size]\n"
            "smlal v23.4s, v0.4h, v11.4h\n"
            "ld1 {v17.8b}, [x13], %[input_depth]\n"
            "smlal2 v24.4s, v0.8h, v11.8h\n"
            "ld1 {v18.8b}, [x13]\n"
            "add x13, x12, %[input_row_size]\n"
            "smlal v21.4s, v1.4h, v10.4h\n"
            "ld1 {v9.8b}, [x15], %[input_depth]\n"
            "smlal2 v22.4s, v1.8h, v10.8h\n"
            "ld1 {v10.8b}, [x15], %[input_depth]\n"
            "smlal v21.4s, v2.4h, v11.4h\n"
            "smlal2 v22.4s, v2.8h, v11.8h\n"
            "ld1 {v11.8b}, [x15], %[input_depth]\n"
            "smlal v21.4s, v3.4h, v14.4h\n"
            "smlal2 v22.4s, v3.8h, v14.8h\n"
            "ld1 {v14.8b}, [x12], %[input_depth]\n"
            "smlal v23.4s, v3.4h, v16.4h\n"
            "subs w14, w14, #2\n"
            "smlal2 v24.4s, v3.8h, v16.8h\n"
            "cmp w14, #3\n"
            "smlal v21.4s, v4.4h, v15.4h\n"
            "uaddw v12.8h, v28.8h, v12.8b\n"
            "smlal2 v22.4s, v4.8h, v15.8h\n"
            "ld1 {v15.8b}, [x12], %[input_depth]\n"
            "smlal v21.4s, v5.4h, v16.4h\n"
            "uaddw v13.8h, v28.8h, v13.8b\n"
            "smlal2 v22.4s, v5.8h, v16.8h\n"
            "ld1 {v16.8b}, [x12], %[input_depth]\n"
            "smlal v23.4s, v1.4h, v12.4h\n"
            "uaddw v17.8h, v28.8h, v17.8b\n"
            "smlal2 v24.4s, v1.8h, v12.8h\n"
            "ld1 {v12.8b}, [x15], %[input_depth]\n"
            "smlal v23.4s, v2.4h, v13.4h\n"
            "uaddw v18.8h, v28.8h, v18.8b\n"
            "smlal2 v24.4s, v2.8h, v13.8h\n"
            "ld1 {v13.8b}, [x15]\n"
            "smlal v23.4s, v4.4h, v17.4h\n"
            "uaddw v9.8h, v28.8h, v9.8b\n"
            "smlal2 v24.4s, v4.8h, v17.8h\n"
            "ld1 {v17.8b}, [x12], %[input_depth]\n"
            "smlal v23.4s, v5.4h, v18.4h\n"
            "uaddw v10.8h, v28.8h, v10.8b\n"
            "smlal2 v24.4s, v5.8h, v18.8h\n"
            "ld1 {v18.8b}, [x12]\n"

            "smlal v21.4s, v6.4h, v9.4h\n"
            "smlal2 v22.4s, v6.8h, v9.8h\n"
            "smlal v19.4s, v0.4h, v9.4h\n"
            "uaddw v11.8h, v28.8h, v11.8b\n"
            "smlal2 v20.4s, v0.8h, v9.8h\n"
            "ld1 {v9.8b}, [x13], %[input_depth]\n"
            "smlal v23.4s, v6.4h, v11.4h\n"
            "smlal2 v24.4s, v6.8h, v11.8h\n"
            "smlal v21.4s, v7.4h, v10.4h\n"
            "smlal2 v22.4s, v7.8h, v10.8h\n"
            "uaddw v12.8h, v28.8h, v12.8b\n"
            "smlal v19.4s, v1.4h, v10.4h\n"
            "smlal2 v20.4s, v1.8h, v10.8h\n"
            "ld1 {v10.8b}, [x13], %[input_depth]\n"
            "smlal v23.4s, v7.4h, v12.4h\n"
            "smlal2 v24.4s, v7.8h, v12.8h\n"
            "smlal v25.4s, v1.4h, v12.4h\n"
            "smlal2 v26.4s, v1.8h, v12.8h\n"
            "smlal v21.4s, v8.4h, v11.4h\n"
            "smlal2 v22.4s, v8.8h, v11.8h\n"
            "add x11, x11, %[input_width_increment]\n"
            "smlal v19.4s, v2.4h, v11.4h\n"
            "mov x12, x11\n"
            "smlal2 v20.4s, v2.8h, v11.8h\n"
            "uaddw v13.8h, v28.8h, v13.8b\n"
            "smlal v25.4s, v0.4h, v11.4h\n"
            "smlal2 v26.4s, v0.8h, v11.8h\n"
            "ld1 {v11.8b}, [x13], %[input_depth]\n"
            "smlal v23.4s, v8.4h, v13.4h\n"
            "ld1 {v12.8b}, [x13], %[input_depth]\n"
            "smlal2 v24.4s, v8.8h, v13.8h\n"
            "smlal v25.4s, v2.4h, v13.4h\n"
            "smlal2 v26.4s, v2.8h, v13.8h\n"
            "ld1 {v13.8b}, [x13]\n"
            "add x13, x12, %[input_row_size]\n"
            "add x15, x13, %[input_row_size]\n"

            "dup v28.4s, w9\n"
            "sqrdmulh v21.4s, v21.4s, v27.4s\n"
            "sqrdmulh v22.4s, v22.4s, v27.4s\n"
            "sqrdmulh v23.4s, v23.4s, v27.4s\n"
            "sqrdmulh v24.4s, v24.4s, v27.4s\n"
            "sqrshl v21.4s, v21.4s, v28.4s\n"
            "sqrshl v22.4s, v22.4s, v28.4s\n"
            "sqrshl v23.4s, v23.4s, v28.4s\n"
            "sqrshl v24.4s, v24.4s, v28.4s\n"
            "dup v28.8h, w0\n"
            "sqxtn v21.4h, v21.4s\n"
            "sqxtn2 v21.8h, v22.4s\n"
            "sqxtn v23.4h, v23.4s\n"
            "sqxtn2 v23.8h, v24.4s\n"
            "sqadd v21.8h, v21.8h, v29.8h\n"
            "sqadd v23.8h, v23.8h, v29.8h\n"
            "sqxtun v21.8b, v21.8h\n"
            "sqxtun2 v21.16b, v23.8h\n"
            "ld1 {v22.4s}, [x10]\n"
            "umax v21.16b, v21.16b, v30.16b\n"
            "umin v21.16b, v21.16b, v31.16b\n"
            "ld1 {v24.4s}, [x10]\n"
            "uaddw v9.8h, v28.8h, v9.8b\n"
            "st1 {v21.8b}, [x6], x5\n"
            "uaddw v10.8h, v28.8h, v10.8b\n"
            "mov v23.d[0], v21.d[1]\n"
            "st1 {v23.8b}, [x6], x5\n"
            "uaddw v11.8h, v28.8h, v11.8b\n"

            "smlal v19.4s, v6.4h, v9.4h\n"
            "smlal2 v20.4s, v6.8h, v9.8h\n"
            "ld1 {v9.8b}, [x12], %[input_depth]\n"
            "smlal v25.4s, v6.4h, v11.4h\n"
            "smlal2 v26.4s, v6.8h, v11.8h\n"
            "smlal v19.4s, v7.4h, v10.4h\n"
            "uaddw v12.8h, v28.8h, v12.8b\n"
            "smlal2 v20.4s, v7.8h, v10.8h\n"
            "ld1 {v10.8b}, [x12], %[input_depth]\n"
            "smlal v25.4s, v7.4h, v12.4h\n"
            "smlal2 v26.4s, v7.8h, v12.8h\n"
            "smlal v19.4s, v8.4h, v11.4h\n"
            "uaddw v13.8h, v28.8h, v13.8b\n"
            "smlal2 v20.4s, v8.8h, v11.8h\n"
            "ld1 {v11.8b}, [x12], %[input_depth]\n"
            "smlal v25.4s, v8.4h, v13.4h\n"
            "uaddw v14.8h, v28.8h, v14.8b\n"
            "smlal2 v26.4s, v8.8h, v13.8h\n"
            "uaddw v16.8h, v28.8h, v16.8b\n"
            "smlal v19.4s, v3.4h, v14.4h\n"
            "uaddw v15.8h, v28.8h, v15.8b\n"
            "smlal2 v20.4s, v3.8h, v14.8h\n"
            "ld1 {v14.8b}, [x13], %[input_depth]\n"
            "smlal v25.4s, v3.4h, v16.4h\n"
            "ld1 {v21.4s}, [%[bias_ptr]]\n"
            "smlal2 v26.4s, v3.8h, v16.8h\n"
            "ld1 {v23.4s}, [%[bias_ptr]]\n"
            "smlal v19.4s, v4.4h, v15.4h\n"
            "uaddw v17.8h, v28.8h, v17.8b\n"
            "smlal2 v20.4s, v4.8h, v15.8h\n"
            "ld1 {v15.8b}, [x13], %[input_depth]\n"
            "smlal v25.4s, v4.4h, v17.4h\n"
            "smlal2 v26.4s, v4.8h, v17.8h\n"
            "smlal v19.4s, v5.4h, v16.4h\n"
            "uaddw v18.8h, v28.8h, v18.8b\n"
            "smlal2 v20.4s, v5.8h, v16.8h\n"
            "ld1 {v16.8b}, [x13], %[input_depth]\n"
            "smlal v25.4s, v5.4h, v18.4h\n"
            "smlal2 v26.4s, v5.8h, v18.8h\n"

            "dup v28.4s, w9\n"
            "sqrdmulh v19.4s, v19.4s, v27.4s\n"
            "sqrdmulh v20.4s, v20.4s, v27.4s\n"
            "sqrdmulh v25.4s, v25.4s, v27.4s\n"
            "sqrdmulh v26.4s, v26.4s, v27.4s\n"
            "sqrshl v19.4s, v19.4s, v28.4s\n"
            "sqrshl v20.4s, v20.4s, v28.4s\n"
            "sqrshl v25.4s, v25.4s, v28.4s\n"
            "sqrshl v26.4s, v26.4s, v28.4s\n"
            "dup v28.8h, w0\n"
            "sqxtn v19.4h, v19.4s\n"
            "sqxtn2 v19.8h, v20.4s\n"
            "sqxtn v25.4h, v25.4s\n"
            "sqxtn2 v25.8h, v26.4s\n"
            "sqadd v19.8h, v19.8h, v29.8h\n"
            "sqadd v25.8h, v25.8h, v29.8h\n"
            "sqxtun v19.8b, v19.8h\n"
            "sqxtun2 v19.16b, v25.8h\n"
            "ld1 {v20.4s}, [x10]\n"
            "umax v19.16b, v19.16b, v30.16b\n"
            "umin v19.16b, v19.16b, v31.16b\n"
            "ld1 {v26.4s}, [x10]\n"
            "uaddw v9.8h, v28.8h, v9.8b\n"
            "st1 {v19.8b}, [x7], x5\n"
            "uaddw v10.8h, v28.8h, v10.8b\n"
            "mov v25.d[0], v19.d[1]\n"
            "st1 {v25.8b}, [x7], x5\n"
            "uaddw v11.8h, v28.8h, v11.8b\n"
            "ld1 {v19.4s}, [%[bias_ptr]]\n"
            "uaddw v14.8h, v28.8h, v14.8b\n"
            "ld1 {v25.4s}, [%[bias_ptr]]\n"
            "uaddw v15.8h, v28.8h, v15.8b\n"
            "uaddw v16.8h, v28.8h, v16.8b\n"

            "bge " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP "b\n"

          // At this point, there will be one of 2 width or 1 width leftover,
          // not both.
          "cmp w14, #2\n"
          "blt " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER "f\n"

          // Handle last 2 columns if exists.
          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER ":\n"
          "smlal v21.4s, v0.4h, v9.4h\n"
          "ld1 {v12.8b}, [x12], %[input_depth]\n"
          "smlal2 v22.4s, v0.8h, v9.8h\n"
          "ld1 {v13.8b}, [x12]\n"
          "add x12, x15, %[input_row_size]\n"
          "smlal v23.4s, v0.4h, v11.4h\n"
          "ld1 {v17.8b}, [x13], %[input_depth]\n"
          "smlal2 v24.4s, v0.8h, v11.8h\n"
          "ld1 {v18.8b}, [x13]\n"
          "add x13, x12, %[input_row_size]\n"
          "smlal v21.4s, v1.4h, v10.4h\n"
          "ld1 {v9.8b}, [x15], %[input_depth]\n"
          "smlal2 v22.4s, v1.8h, v10.8h\n"
          "ld1 {v10.8b}, [x15], %[input_depth]\n"
          "smlal v21.4s, v2.4h, v11.4h\n"
          "smlal2 v22.4s, v2.8h, v11.8h\n"
          "ld1 {v11.8b}, [x15], %[input_depth]\n"
          "smlal v21.4s, v3.4h, v14.4h\n"
          "smlal2 v22.4s, v3.8h, v14.8h\n"
          "ld1 {v14.8b}, [x12], %[input_depth]\n"
          "smlal v23.4s, v3.4h, v16.4h\n"
          "smlal2 v24.4s, v3.8h, v16.8h\n"
          "smlal v21.4s, v4.4h, v15.4h\n"
          "uaddw v12.8h, v28.8h, v12.8b\n"
          "smlal2 v22.4s, v4.8h, v15.8h\n"
          "ld1 {v15.8b}, [x12], %[input_depth]\n"
          "smlal v21.4s, v5.4h, v16.4h\n"
          "uaddw v13.8h, v28.8h, v13.8b\n"
          "smlal2 v22.4s, v5.8h, v16.8h\n"
          "ld1 {v16.8b}, [x12], %[input_depth]\n"
          "smlal v23.4s, v1.4h, v12.4h\n"
          "uaddw v17.8h, v28.8h, v17.8b\n"
          "smlal2 v24.4s, v1.8h, v12.8h\n"
          "ld1 {v12.8b}, [x15], %[input_depth]\n"
          "smlal v23.4s, v2.4h, v13.4h\n"
          "uaddw v18.8h, v28.8h, v18.8b\n"
          "smlal2 v24.4s, v2.8h, v13.8h\n"
          "ld1 {v13.8b}, [x15]\n"
          "smlal v23.4s, v4.4h, v17.4h\n"
          "uaddw v9.8h, v28.8h, v9.8b\n"
          "smlal2 v24.4s, v4.8h, v17.8h\n"
          "ld1 {v17.8b}, [x12], %[input_depth]\n"
          "smlal v23.4s, v5.4h, v18.4h\n"
          "uaddw v10.8h, v28.8h, v10.8b\n"
          "smlal2 v24.4s, v5.8h, v18.8h\n"
          "ld1 {v18.8b}, [x12]\n"

          "smlal v21.4s, v6.4h, v9.4h\n"
          "smlal2 v22.4s, v6.8h, v9.8h\n"
          "smlal v19.4s, v0.4h, v9.4h\n"
          "uaddw v11.8h, v28.8h, v11.8b\n"
          "smlal2 v20.4s, v0.8h, v9.8h\n"
          "ld1 {v9.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v6.4h, v11.4h\n"
          "smlal2 v24.4s, v6.8h, v11.8h\n"
          "smlal v21.4s, v7.4h, v10.4h\n"
          "smlal2 v22.4s, v7.8h, v10.8h\n"
          "uaddw v12.8h, v28.8h, v12.8b\n"
          "smlal v19.4s, v1.4h, v10.4h\n"
          "smlal2 v20.4s, v1.8h, v10.8h\n"
          "ld1 {v10.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v7.4h, v12.4h\n"
          "smlal2 v24.4s, v7.8h, v12.8h\n"
          "smlal v25.4s, v1.4h, v12.4h\n"
          "smlal2 v26.4s, v1.8h, v12.8h\n"
          "smlal v21.4s, v8.4h, v11.4h\n"
          "smlal2 v22.4s, v8.8h, v11.8h\n"
          "smlal v19.4s, v2.4h, v11.4h\n"
          "smlal2 v20.4s, v2.8h, v11.8h\n"
          "uaddw v13.8h, v28.8h, v13.8b\n"
          "smlal v25.4s, v0.4h, v11.4h\n"
          "smlal2 v26.4s, v0.8h, v11.8h\n"
          "ld1 {v11.8b}, [x13], %[input_depth]\n"
          "smlal v23.4s, v8.4h, v13.4h\n"
          "ld1 {v12.8b}, [x13], %[input_depth]\n"
          "smlal2 v24.4s, v8.8h, v13.8h\n"
          "smlal v25.4s, v2.4h, v13.4h\n"
          "smlal2 v26.4s, v2.8h, v13.8h\n"
          "ld1 {v13.8b}, [x13]\n"

          "dup v28.4s, w9\n"
          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "sqrshl v21.4s, v21.4s, v28.4s\n"
          "sqrshl v22.4s, v22.4s, v28.4s\n"
          "sqrshl v23.4s, v23.4s, v28.4s\n"
          "sqrshl v24.4s, v24.4s, v28.4s\n"
          "dup v28.8h, w0\n"
          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "sqxtun2 v21.16b, v23.8h\n"
          "ld1 {v22.4s}, [x10]\n"
          "umax v21.16b, v21.16b, v30.16b\n"
          "umin v21.16b, v21.16b, v31.16b\n"
          "ld1 {v24.4s}, [x10]\n"
          "uaddw v9.8h, v28.8h, v9.8b\n"
          "st1 {v21.8b}, [x6], x5\n"
          "uaddw v10.8h, v28.8h, v10.8b\n"
          "mov v23.d[0], v21.d[1]\n"
          "st1 {v23.8b}, [x6]\n"
          "uaddw v11.8h, v28.8h, v11.8b\n"

          "smlal v19.4s, v6.4h, v9.4h\n"
          "smlal2 v20.4s, v6.8h, v9.8h\n"
          "smlal v25.4s, v6.4h, v11.4h\n"
          "smlal2 v26.4s, v6.8h, v11.8h\n"
          "smlal v19.4s, v7.4h, v10.4h\n"
          "uaddw v12.8h, v28.8h, v12.8b\n"
          "smlal2 v20.4s, v7.8h, v10.8h\n"
          "smlal v25.4s, v7.4h, v12.4h\n"
          "smlal2 v26.4s, v7.8h, v12.8h\n"
          "smlal v19.4s, v8.4h, v11.4h\n"
          "uaddw v13.8h, v28.8h, v13.8b\n"
          "smlal2 v20.4s, v8.8h, v11.8h\n"
          "smlal v25.4s, v8.4h, v13.4h\n"
          "uaddw v14.8h, v28.8h, v14.8b\n"
          "smlal2 v26.4s, v8.8h, v13.8h\n"
          "uaddw v16.8h, v28.8h, v16.8b\n"
          "smlal v19.4s, v3.4h, v14.4h\n"
          "uaddw v15.8h, v28.8h, v15.8b\n"
          "smlal2 v20.4s, v3.8h, v14.8h\n"
          "smlal v25.4s, v3.4h, v16.4h\n"
          "smlal2 v26.4s, v3.8h, v16.8h\n"
          "smlal v19.4s, v4.4h, v15.4h\n"
          "uaddw v17.8h, v28.8h, v17.8b\n"
          "smlal2 v20.4s, v4.8h, v15.8h\n"
          "smlal v25.4s, v4.4h, v17.4h\n"
          "smlal2 v26.4s, v4.8h, v17.8h\n"
          "smlal v19.4s, v5.4h, v16.4h\n"
          "uaddw v18.8h, v28.8h, v18.8b\n"
          "smlal2 v20.4s, v5.8h, v16.8h\n"
          "smlal v25.4s, v5.4h, v18.4h\n"
          "smlal2 v26.4s, v5.8h, v18.8h\n"

          "dup v28.4s, w9\n"
          "sqrdmulh v19.4s, v19.4s, v27.4s\n"
          "sqrdmulh v20.4s, v20.4s, v27.4s\n"
          "sqrdmulh v25.4s, v25.4s, v27.4s\n"
          "sqrdmulh v26.4s, v26.4s, v27.4s\n"
          "sqrshl v19.4s, v19.4s, v28.4s\n"
          "sqrshl v20.4s, v20.4s, v28.4s\n"
          "sqrshl v25.4s, v25.4s, v28.4s\n"
          "sqrshl v26.4s, v26.4s, v28.4s\n"
          "dup v28.8h, w0\n"
          "sqxtn v19.4h, v19.4s\n"
          "sqxtn2 v19.8h, v20.4s\n"
          "sqxtn v25.4h, v25.4s\n"
          "sqxtn2 v25.8h, v26.4s\n"
          "sqadd v19.8h, v19.8h, v29.8h\n"
          "sqadd v25.8h, v25.8h, v29.8h\n"
          "sqxtun v19.8b, v19.8h\n"
          "sqxtun2 v19.16b, v25.8h\n"
          "umax v19.16b, v19.16b, v30.16b\n"
          "umin v19.16b, v19.16b, v31.16b\n"
          "st1 {v19.8b}, [x7], x5\n"
          "mov v25.d[0], v19.d[1]\n"
          "st1 {v25.8b}, [x7]\n"
          "b " DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP "f\n"

          // Handle last column if exists.
          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER ":\n"
          // Registers v9, v10, v11, v14, v15, and v16 have already been loaded
          // with the correct values at this point. This corresponds to the
          // first two input rows of the top left output. Now load the last
          // input row for this output. Once these inputs are no longer needed,
          // load the input rows for the bottom left output.
          "add x12, x15, %[input_row_size]\n"
          "add x13, x12, %[input_row_size]\n"

          "ld1 {v12.8b}, [x15], %[input_depth]\n"
          "smlal v21.4s, v0.4h, v9.4h\n"
          "ld1 {v13.8b}, [x15], %[input_depth]\n"
          "smlal2 v22.4s, v0.8h, v9.8h\n"
          "ld1 {v17.8b}, [x15]\n"
          "smlal v21.4s, v1.4h, v10.4h\n"
          "ld1 {v9.8b}, [x12], %[input_depth]\n"
          "smlal2 v22.4s, v1.8h, v10.8h\n"
          "ld1 {v10.8b}, [x12], %[input_depth]\n"
          "smlal v21.4s, v2.4h, v11.4h\n"
          "smlal2 v22.4s, v2.8h, v11.8h\n"
          "ld1 {v11.8b}, [x12]\n"
          "smlal v21.4s, v3.4h, v14.4h\n"
          "smlal2 v22.4s, v3.8h, v14.8h\n"
          "ld1 {v14.8b}, [x13], %[input_depth]\n"
          "smlal v21.4s, v4.4h, v15.4h\n"
          "smlal2 v22.4s, v4.8h, v15.8h\n"
          "ld1 {v15.8b}, [x13], %[input_depth]\n"
          "smlal v21.4s, v5.4h, v16.4h\n"
          "uaddw v12.8h, v28.8h, v12.8b\n"
          "smlal2 v22.4s, v5.8h, v16.8h\n"
          "uaddw v13.8h, v28.8h, v13.8b\n"
          "ld1 {v16.8b}, [x13]\n"

          "smlal v21.4s, v6.4h, v12.4h\n"
          "smlal2 v22.4s, v6.8h, v12.8h\n"
          "smlal v23.4s, v0.4h, v12.4h\n"
          "uaddw v17.8h, v28.8h, v17.8b\n"
          "smlal2 v24.4s, v0.8h, v12.8h\n"
          "smlal v21.4s, v7.4h, v13.4h\n"
          "smlal2 v22.4s, v7.8h, v13.8h\n"
          "smlal v23.4s, v1.4h, v13.4h\n"
          "smlal2 v24.4s, v1.8h, v13.8h\n"
          "smlal v21.4s, v8.4h, v17.4h\n"
          "smlal2 v22.4s, v8.8h, v17.8h\n"
          "smlal v23.4s, v2.4h, v17.4h\n"
          "smlal2 v24.4s, v2.8h, v17.8h\n"

          "dup v26.4s, w9\n"
          "sqrdmulh v21.4s, v21.4s, v27.4s\n"
          "sqrdmulh v22.4s, v22.4s, v27.4s\n"
          "sqrshl v21.4s, v21.4s, v26.4s\n"
          "sqrshl v22.4s, v22.4s, v26.4s\n"
          "sqxtn v21.4h, v21.4s\n"
          "sqxtn2 v21.8h, v22.4s\n"
          "sqadd v21.8h, v21.8h, v29.8h\n"
          "sqxtun v21.8b, v21.8h\n"
          "umax v21.8b, v21.8b, v30.8b\n"
          "umin v21.8b, v21.8b, v31.8b\n"
          "uaddw v9.8h, v28.8h, v9.8b\n"
          "st1 {v21.8b}, [x6]\n"
          "uaddw v10.8h, v28.8h, v10.8b\n"

          "smlal v23.4s, v3.4h, v9.4h\n"
          "uaddw v11.8h, v28.8h, v11.8b\n"
          "smlal2 v24.4s, v3.8h, v9.8h\n"
          "uaddw v14.8h, v28.8h, v14.8b\n"
          "smlal v23.4s, v4.4h, v10.4h\n"
          "uaddw v15.8h, v28.8h, v15.8b\n"
          "smlal2 v24.4s, v4.8h, v10.8h\n"
          "uaddw v16.8h, v28.8h, v16.8b\n"
          "smlal v23.4s, v5.4h, v11.4h\n"
          "smlal2 v24.4s, v5.8h, v11.8h\n"

          "smlal v23.4s, v6.4h, v14.4h\n"
          "smlal2 v24.4s, v6.8h, v14.8h\n"
          "smlal v23.4s, v7.4h, v15.4h\n"
          "smlal2 v24.4s, v7.8h, v15.8h\n"
          "smlal v23.4s, v8.4h, v16.4h\n"
          "smlal2 v24.4s, v8.8h, v16.8h\n"

          "sqrdmulh v23.4s, v23.4s, v27.4s\n"
          "sqrdmulh v24.4s, v24.4s, v27.4s\n"
          "sqrshl v23.4s, v23.4s, v26.4s\n"
          "sqrshl v24.4s, v24.4s, v26.4s\n"
          "sqxtn v23.4h, v23.4s\n"
          "sqxtn2 v23.8h, v24.4s\n"
          "sqadd v23.8h, v23.8h, v29.8h\n"
          "sqxtun v23.8b, v23.8h\n"
          "umax v23.8b, v23.8b, v30.8b\n"
          "umin v23.8b, v23.8b, v31.8b\n"
          "st1 {v23.8b}, [x7]\n"

          DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP ":\n"
          "subs %w[output_window_height], %w[output_window_height], #2\n"
          "add %[input_ptr], %[input_ptr], %[input_height_increment]\n"
          "cmp %w[output_window_height], #2\n"
          "add %[output_ptr], %[output_ptr], %[output_height_increment]\n"
          "bge " DEPTHWISECONV_LABEL_HEIGHT_2_LOOP "b\n"

        DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP ":\n"
        "cmp %w[output_window_height], #1\n"
        "blt " DEPTHWISECONV_LABEL_HEIGHT_1_END "f\n"

        DEPTHWISECONV_LABEL_HEIGHT_1 ":\n"
        "mov x11, %[input_ptr]\n"
        "mov x12, x11\n"
        "add x13, x12, %[input_row_size]\n"
        "ld1 {v9.8b}, [x12], %[input_depth]\n"
        "add x15, x13, %[input_row_size]\n"
        "ld1 {v10.8b}, [x12], %[input_depth]\n"
        "mov x6, %[output_ptr]\n"
        "ld1 {v11.8b}, [x12], %[input_depth]\n"
        "mov w14, %w[output_window_width]\n"
        // The height 1 / width 2 loop loads an extra 1x1 output in anticipation
        // for the next iteration. Make sure |output_window_width| is large
        // enough to handle the additional load, otherwise jump to the
        // appropriate label to handle smaller widths.
        "cmp w14, #2\n"
        "ld1 {v12.8b}, [x13], %[input_depth]\n"
        "ld1 {v13.8b}, [x13], %[input_depth]\n"
        "ld1 {v14.8b}, [x13], %[input_depth]\n"
        "ld1 {v15.8b}, [x15], %[input_depth]\n"
        "ld1 {v16.8b}, [x15], %[input_depth]\n"
        "ld1 {v17.8b}, [x15], %[input_depth]\n"

        "uaddw v9.8h, v28.8h, v9.8b\n"
        "ld1 {v24.4s}, [%[bias_ptr]]\n"
        "uaddw v10.8h, v28.8h, v10.8b\n"
        "ld1 {v25.4s}, [x10]\n"
        "uaddw v11.8h, v28.8h, v11.8b\n"
        "ld1 {v26.4s}, [%[bias_ptr]]\n"
        "ld1 {v27.4s}, [x10]\n"
        "uaddw v12.8h, v28.8h, v12.8b\n"
        "uaddw v13.8h, v28.8h, v13.8b\n"
        "uaddw v14.8h, v28.8h, v14.8b\n"
        "uaddw v15.8h, v28.8h, v15.8b\n"
        "uaddw v16.8h, v28.8h, v16.8b\n"
        "uaddw v17.8h, v28.8h, v17.8b\n"

        "beq " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER "f\n"
        "cmp w14, #1\n"
        "beq " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP ":\n"
          "smlal v24.4s, v0.4h, v9.4h\n"
          "ld1 {v18.8b}, [x12], %[input_depth]\n"
          "smlal2 v25.4s, v0.8h, v9.8h\n"
          "ld1 {v19.8b}, [x12]\n"
          "smlal v26.4s, v0.4h, v11.4h\n"
          "ld1 {v20.8b}, [x13], %[input_depth]\n"
          "smlal2 v27.4s, v0.8h, v11.8h\n"
          "ld1 {v21.8b}, [x13]\n"
          "smlal v24.4s, v1.4h, v10.4h\n"
          "ld1 {v22.8b}, [x15], %[input_depth]\n"
          "smlal2 v25.4s, v1.8h, v10.8h\n"
          "ld1 {v23.8b}, [x15]\n"
          "smlal v24.4s, v2.4h, v11.4h\n"
          "subs w14, w14, #2\n"
          "smlal2 v25.4s, v2.8h, v11.8h\n"
          "cmp w14, #3\n"
          "smlal v24.4s, v3.4h, v12.4h\n"
          "add x11, x11, %[input_width_increment]\n"
          "smlal2 v25.4s, v3.8h, v12.8h\n"
          "mov x12, x11\n"
          "smlal v26.4s, v3.4h, v14.4h\n"
          "add x13, x12, %[input_row_size]\n"
          "smlal2 v27.4s, v3.8h, v14.8h\n"
          "add x15, x13, %[input_row_size]\n"
          "smlal v24.4s, v4.4h, v13.4h\n"
          "ld1 {v9.8b}, [x12], %[input_depth]\n"
          "smlal2 v25.4s, v4.8h, v13.8h\n"
          "ld1 {v10.8b}, [x12], %[input_depth]\n"
          "smlal v24.4s, v5.4h, v14.4h\n"
          "ld1 {v11.8b}, [x12], %[input_depth]\n"
          "smlal2 v25.4s, v5.8h, v14.8h\n"
          "ld1 {v12.8b}, [x13], %[input_depth]\n"
          "smlal v24.4s, v6.4h, v15.4h\n"
          "ld1 {v13.8b}, [x13], %[input_depth]\n"
          "smlal2 v25.4s, v6.8h, v15.8h\n"
          "ld1 {v14.8b}, [x13], %[input_depth]\n"
          "smlal v26.4s, v6.4h, v17.4h\n"
          "ld1 {v15.8b}, [x15], %[input_depth]\n"
          "smlal2 v27.4s, v6.8h, v17.8h\n"
          "smlal v24.4s, v7.4h, v16.4h\n"
          "smlal2 v25.4s, v7.8h, v16.8h\n"
          "ld1 {v16.8b}, [x15], %[input_depth]\n"
          "smlal v24.4s, v8.4h, v17.4h\n"
          "uaddw v18.8h, v28.8h, v18.8b\n"
          "smlal2 v25.4s, v8.8h, v17.8h\n"
          "ld1 {v17.8b}, [x15], %[input_depth]\n"
          "uaddw v19.8h, v28.8h, v19.8b\n"

          "smlal v26.4s, v1.4h, v18.4h\n"
          "uaddw v20.8h, v28.8h, v20.8b\n"
          "smlal2 v27.4s, v1.8h, v18.8h\n"
          "smlal v26.4s, v2.4h, v19.4h\n"
          "uaddw v21.8h, v28.8h, v21.8b\n"
          "smlal2 v27.4s, v2.8h, v19.8h\n"
          "smlal v26.4s, v4.4h, v20.4h\n"
          "smlal v26.4s, v5.4h, v21.4h\n"
          "smlal2 v27.4s, v4.8h, v20.8h\n"
          "uaddw v22.8h, v28.8h, v22.8b\n"
          "smlal2 v27.4s, v5.8h, v21.8h\n"
          "uaddw v23.8h, v28.8h, v23.8b\n"
          "smlal v26.4s, v7.4h, v22.4h\n"
          "smlal2 v27.4s, v7.8h, v22.8h\n"
          "smlal v26.4s, v8.4h, v23.4h\n"
          "smlal2 v27.4s, v8.8h, v23.8h\n"

          "dup v28.4s, w1\n"
          "dup v29.4s, w9\n"
          "sqrdmulh v24.4s, v24.4s, v28.4s\n"
          "sqrdmulh v25.4s, v25.4s, v28.4s\n"
          "sqrdmulh v26.4s, v26.4s, v28.4s\n"
          "sqrdmulh v27.4s, v27.4s, v28.4s\n"
          "dup v28.8h, w2\n"
          "sqrshl v24.4s, v24.4s, v29.4s\n"
          "sqrshl v25.4s, v25.4s, v29.4s\n"
          "sqrshl v26.4s, v26.4s, v29.4s\n"
          "sqrshl v27.4s, v27.4s, v29.4s\n"
          "sqxtn v24.4h, v24.4s\n"
          "sqxtn2 v24.8h, v25.4s\n"
          "sqxtn v26.4h, v26.4s\n"
          "sqxtn2 v26.8h, v27.4s\n"
          "sqadd v24.8h, v24.8h, v28.8h\n"
          "sqadd v26.8h, v26.8h, v28.8h\n"
          "sqxtun v24.8b, v24.8h\n"
          "sqxtun2 v24.16b, v26.8h\n"
          "dup v28.8h, w0\n"
          "ld1 {v25.4s}, [x10]\n"
          "umax v24.16b, v24.16b, v30.16b\n"
          "umin v24.16b, v24.16b, v31.16b\n"
          "ld1 {v27.4s}, [x10]\n"
          "uaddw v9.8h, v28.8h, v9.8b\n"
          "st1 {v24.8b}, [x6], x5\n"
          "uaddw v10.8h, v28.8h, v10.8b\n"
          "mov v26.d[0], v24.d[1]\n"
          "st1 {v26.8b}, [x6], x5\n"
          "uaddw v11.8h, v28.8h, v11.8b\n"
          "uaddw v12.8h, v28.8h, v12.8b\n"
          "uaddw v13.8h, v28.8h, v13.8b\n"
          "uaddw v14.8h, v28.8h, v14.8b\n"
          "ld1 {v24.4s}, [%[bias_ptr]]\n"
          "uaddw v15.8h, v28.8h, v15.8b\n"
          "ld1 {v26.4s}, [%[bias_ptr]]\n"
          "uaddw v16.8h, v28.8h, v16.8b\n"
          "uaddw v17.8h, v28.8h, v17.8b\n"

          "bge " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP "b\n"

        // At this point, there will be one of 2 width or 1 width leftover,
        // not both.
        "cmp w14, #2\n"
        "blt " DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER "f\n"

        // Handle last two horizontal outputs if exists.
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER ":\n"
        "smlal v24.4s, v0.4h, v9.4h\n"
        "ld1 {v18.8b}, [x12], %[input_depth]\n"
        "smlal2 v25.4s, v0.8h, v9.8h\n"
        "ld1 {v19.8b}, [x12]\n"
        "smlal v26.4s, v0.4h, v11.4h\n"
        "ld1 {v20.8b}, [x13], %[input_depth]\n"
        "smlal2 v27.4s, v0.8h, v11.8h\n"
        "ld1 {v21.8b}, [x13]\n"
        "smlal v24.4s, v1.4h, v10.4h\n"
        "ld1 {v22.8b}, [x15], %[input_depth]\n"
        "smlal2 v25.4s, v1.8h, v10.8h\n"
        "ld1 {v23.8b}, [x15]\n"
        "smlal v24.4s, v2.4h, v11.4h\n"
        "smlal2 v25.4s, v2.8h, v11.8h\n"
        "smlal v24.4s, v3.4h, v12.4h\n"
        "smlal2 v25.4s, v3.8h, v12.8h\n"
        "smlal v26.4s, v3.4h, v14.4h\n"
        "smlal2 v27.4s, v3.8h, v14.8h\n"
        "smlal v24.4s, v4.4h, v13.4h\n"
        "smlal2 v25.4s, v4.8h, v13.8h\n"
        "smlal v24.4s, v5.4h, v14.4h\n"
        "smlal2 v25.4s, v5.8h, v14.8h\n"
        "smlal v24.4s, v6.4h, v15.4h\n"
        "smlal2 v25.4s, v6.8h, v15.8h\n"
        "smlal v26.4s, v6.4h, v17.4h\n"
        "smlal2 v27.4s, v6.8h, v17.8h\n"
        "smlal v24.4s, v7.4h, v16.4h\n"
        "smlal2 v25.4s, v7.8h, v16.8h\n"
        "smlal v24.4s, v8.4h, v17.4h\n"
        "uaddw v18.8h, v28.8h, v18.8b\n"
        "smlal2 v25.4s, v8.8h, v17.8h\n"
        "uaddw v19.8h, v28.8h, v19.8b\n"

        "smlal v26.4s, v1.4h, v18.4h\n"
        "uaddw v20.8h, v28.8h, v20.8b\n"
        "smlal2 v27.4s, v1.8h, v18.8h\n"
        "smlal v26.4s, v2.4h, v19.4h\n"
        "uaddw v21.8h, v28.8h, v21.8b\n"
        "smlal2 v27.4s, v2.8h, v19.8h\n"
        "smlal v26.4s, v4.4h, v20.4h\n"
        "smlal v26.4s, v5.4h, v21.4h\n"
        "smlal2 v27.4s, v4.8h, v20.8h\n"
        "uaddw v22.8h, v28.8h, v22.8b\n"
        "smlal2 v27.4s, v5.8h, v21.8h\n"
        "uaddw v23.8h, v28.8h, v23.8b\n"
        "smlal v26.4s, v7.4h, v22.4h\n"
        "smlal2 v27.4s, v7.8h, v22.8h\n"
        "smlal v26.4s, v8.4h, v23.4h\n"
        "smlal2 v27.4s, v8.8h, v23.8h\n"

        "dup v28.4s, w1\n"
        "dup v29.4s, w9\n"
        "sqrdmulh v24.4s, v24.4s, v28.4s\n"
        "sqrdmulh v25.4s, v25.4s, v28.4s\n"
        "sqrdmulh v26.4s, v26.4s, v28.4s\n"
        "sqrdmulh v27.4s, v27.4s, v28.4s\n"
        "dup v28.8h, w2\n"
        "sqrshl v24.4s, v24.4s, v29.4s\n"
        "sqrshl v25.4s, v25.4s, v29.4s\n"
        "sqrshl v26.4s, v26.4s, v29.4s\n"
        "sqrshl v27.4s, v27.4s, v29.4s\n"
        "sqxtn v24.4h, v24.4s\n"
        "sqxtn2 v24.8h, v25.4s\n"
        "sqxtn v26.4h, v26.4s\n"
        "sqxtn2 v26.8h, v27.4s\n"
        "sqadd v24.8h, v24.8h, v28.8h\n"
        "sqadd v26.8h, v26.8h, v28.8h\n"
        "sqxtun v24.8b, v24.8h\n"
        "sqxtun2 v24.16b, v26.8h\n"
        "dup v28.8h, w0\n"
        "umax v24.16b, v24.16b, v30.16b\n"
        "umin v24.16b, v24.16b, v31.16b\n"
        "st1 {v24.8b}, [x6], x5\n"
        "mov v26.d[0], v24.d[1]\n"
        "st1 {v26.8b}, [x6]\n"
        "b " DEPTHWISECONV_LABEL_HEIGHT_1_END "f\n"

        // Handle bottom right output if exists.
        DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER ":\n"
        "dup v26.4s, w9\n"
        "dup v27.4s, w1\n"
        "dup v29.8h, w2\n"

        "smlal v24.4s, v0.4h, v9.4h\n"
        "smlal2 v25.4s, v0.8h, v9.8h\n"
        "smlal v24.4s, v1.4h, v10.4h\n"
        "smlal2 v25.4s, v1.8h, v10.8h\n"
        "smlal v24.4s, v2.4h, v11.4h\n"
        "smlal2 v25.4s, v2.8h, v11.8h\n"
        "smlal v24.4s, v3.4h, v12.4h\n"
        "smlal2 v25.4s, v3.8h, v12.8h\n"
        "smlal v24.4s, v4.4h, v13.4h\n"
        "smlal2 v25.4s, v4.8h, v13.8h\n"
        "smlal v24.4s, v5.4h, v14.4h\n"
        "smlal2 v25.4s, v5.8h, v14.8h\n"
        "smlal v24.4s, v6.4h, v15.4h\n"
        "smlal2 v25.4s, v6.8h, v15.8h\n"
        "smlal v24.4s, v7.4h, v16.4h\n"
        "smlal2 v25.4s, v7.8h, v16.8h\n"
        "smlal v24.4s, v8.4h, v17.4h\n"
        "smlal2 v25.4s, v8.8h, v17.8h\n"

        "sqrdmulh v24.4s, v24.4s, v27.4s\n"
        "sqrdmulh v25.4s, v25.4s, v27.4s\n"
        "sqrshl v24.4s, v24.4s, v26.4s\n"
        "sqrshl v25.4s, v25.4s, v26.4s\n"
        "sqxtn v24.4h, v24.4s\n"
        "sqxtn2 v24.8h, v25.4s\n"
        "sqadd v24.8h, v24.8h, v29.8h\n"
        "sqxtun v24.8b, v24.8h\n"
        "umax v24.8b, v24.8b, v30.8b\n"
        "umin v24.8b, v24.8b, v31.8b\n"
        "st1 {v24.8b}, [x6]\n"

        DEPTHWISECONV_LABEL_HEIGHT_1_END ":\n"
    :
    // Outputs.
    [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
    [output_ptr] "+r"(output_ptr),
    [output_window_height] "+r"(output_window_height)
    :
    // Inputs.
    [bias_ptr] "r"(bias_ptr), [input_row_size] "r"(input_row_size),
    [input_depth] "r"(input_depth),
    [output_window_width] "r"(output_window_width),
    [input_width_increment] "r"(input_width_increment),
    [input_height_increment] "r"(input_height_increment),
    [output_height_increment] "r"(output_height_increment),
    [params_ptr] "r"(params_ptr)
    :
    // Clobbers.
    "cc", "memory",
    // We use these NEON registers.
    "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9",
    "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19",
    "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
    "v30", "v31",
    // We use these general-purpose registers.
    "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
    "x9", "x10", "x11", "x12", "x13", "x14", "x15",
    "x19", "x20");
#undef DEPTHWISECONV_LABEL_HEIGHT_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_1_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_2_WIDTH_2_AFTER_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_2_AFTER_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_1
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LOOP
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_1_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_1_WIDTH_2_LEFTOVER
#undef DEPTHWISECONV_LABEL_HEIGHT_1_END
  }
};

template <>
struct DepthwiseConvPartial<DepthwiseConvOutputRounding::kAwayFromZero,
                            EdgeType::kCenter, 1, 1> {
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         const DepthwiseConvParams* params_ptr) {
#define DEPTHWISECONV_LABEL_DEPTH_8_LOOP "1"
#define DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "2"
    asm volatile(
        // Performs depthwise convolutions for an input window of size 1x1 and
        // padding of 1 across the full depth. Expects |input_ptr| and
        // |filter_ptr| to be pointing to the 1x1 input and filter values.
        "ld1 {v8.8b}, [%[input_ptr]], #8\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "ldr x11, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"
        "ldr w10, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "dup v26.8h, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w10\n"
        "ld1 {v0.8b}, [%[filter_ptr]], #8\n"
        "cmp x11, #16\n"
        "ldr w10, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "dup v28.8h, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v29.4s, w10\n"
        "ldr w10, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v30.16b, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"
        "dup v31.16b, w10\n"
        "dup v25.8h, w9\n"

        "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
        "uaddw v8.8h, v26.8h, v8.8b\n"
        "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
        "uaddw v0.8h, v25.8h, v0.8b\n"

        "blt " DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_DEPTH_8_LOOP ":\n"
          "smlal v16.4s, v0.4h, v8.4h\n"
          "subs x11, x11, #8\n"
          "smlal2 v17.4s, v0.8h, v8.8h\n"
          "ld1 {v8.8b}, [%[input_ptr]], #8\n"
          "cmp x11, #16\n"
          "ld1 {v0.8b}, [%[filter_ptr]], #8\n"

          "sqrdmulh v16.4s, v16.4s, v27.4s\n"
          "sqrdmulh v17.4s, v17.4s, v27.4s\n"
          "and v18.16b, v16.16b, v29.16b\n"
          "and v19.16b, v17.16b, v29.16b\n"
          "sshr v18.4s, v18.4s, #31\n"
          "sshr v19.4s, v19.4s, #31\n"
          "sqadd v16.4s, v16.4s, v18.4s\n"
          "sqadd v17.4s, v17.4s, v19.4s\n"
          "srshl v16.4s, v16.4s, v29.4s\n"
          "srshl v17.4s, v17.4s, v29.4s\n"
          "sqxtn v16.4h, v16.4s\n"
          "sqxtn2 v16.8h, v17.4s\n"
          "sqadd v16.8h, v16.8h, v28.8h\n"
          "sqxtun v16.8b, v16.8h\n"
          "umax v16.8b, v16.8b, v30.8b\n"
          "umin v16.8b, v16.8b, v31.8b\n"
          "st1 {v16.8b}, [%[output_ptr]], #8\n"
          "uaddw v8.8h, v26.8h, v8.8b\n"
          "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
          "uaddw v0.8h, v25.8h, v0.8b\n"
          "ld1 {v17.4s}, [%[bias_ptr]], #16\n"

          "bge " DEPTHWISECONV_LABEL_DEPTH_8_LOOP "b\n"

        DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP ":\n"
        "smlal v16.4s, v0.4h, v8.4h\n"
        "smlal2 v17.4s, v0.8h, v8.8h\n"

        "sqrdmulh v16.4s, v16.4s, v27.4s\n"
        "sqrdmulh v17.4s, v17.4s, v27.4s\n"
        "and v18.16b, v16.16b, v29.16b\n"
        "and v19.16b, v17.16b, v29.16b\n"
        "sshr v18.4s, v18.4s, #31\n"
        "sshr v19.4s, v19.4s, #31\n"
        "sqadd v16.4s, v16.4s, v18.4s\n"
        "sqadd v17.4s, v17.4s, v19.4s\n"
        "srshl v16.4s, v16.4s, v29.4s\n"
        "srshl v17.4s, v17.4s, v29.4s\n"

        "sqxtn v16.4h, v16.4s\n"
        "sqxtn2 v16.8h, v17.4s\n"
        "sqadd v16.8h, v16.8h, v28.8h\n"
        "sqxtun v16.8b, v16.8h\n"
        "umax v16.8b, v16.8b, v30.8b\n"
        "umin v16.8b, v16.8b, v31.8b\n"
        "st1 {v16.8b}, [%[output_ptr]]\n"
        :
        // Outputs.
        [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
        [output_ptr] "+r"(output_ptr), [bias_ptr] "+r"(bias_ptr)
        :
        // Inputs.
        [params_ptr] "r"(params_ptr)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v8", "v16", "v17", "v18", "v19", "v25", "v26", "v27", "v28",
        "v29", "v30", "v31",
        // We use these general-purpose registers.
        "x9", "x10", "x11");
#undef DEPTHWISECONV_LABEL_DEPTH_8_LOOP
#undef DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP
  }
};

template <>
struct DepthwiseConvPartial<DepthwiseConvOutputRounding::kUpward,
                            EdgeType::kCenter, 1, 1> {
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         const DepthwiseConvParams* params_ptr) {
#define DEPTHWISECONV_LABEL_DEPTH_8_LOOP "1"
#define DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "2"
    asm volatile(
        // Performs depthwise convolutions for an input window of size 1x1 and
        // padding of 1 across the full depth. Expects |input_ptr| and
        // |filter_ptr| to be pointing to the 1x1 input and filter values.
        "ld1 {v8.8b}, [%[input_ptr]], #8\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "ldr x11, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"
        "ldr w10, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "dup v26.8h, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w10\n"
        "ld1 {v0.8b}, [%[filter_ptr]], #8\n"
        "cmp x11, #16\n"
        "ldr w10, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "dup v28.8h, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v29.4s, w10\n"
        "ldr w10, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v30.16b, w9\n"
        "ldr w9, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"
        "dup v31.16b, w10\n"
        "dup v25.8h, w9\n"

        "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
        "uaddw v8.8h, v26.8h, v8.8b\n"
        "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
        "uaddw v0.8h, v25.8h, v0.8b\n"

        "blt " DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_DEPTH_8_LOOP ":\n"
          "smlal v16.4s, v0.4h, v8.4h\n"
          "subs x11, x11, #8\n"
          "smlal2 v17.4s, v0.8h, v8.8h\n"
          "ld1 {v8.8b}, [%[input_ptr]], #8\n"
          "cmp x11, #16\n"
          "ld1 {v0.8b}, [%[filter_ptr]], #8\n"

          "sqrdmulh v16.4s, v16.4s, v27.4s\n"
          "sqrdmulh v17.4s, v17.4s, v27.4s\n"
          "sqrshl v16.4s, v16.4s, v29.4s\n"
          "sqrshl v17.4s, v17.4s, v29.4s\n"
          "sqxtn v16.4h, v16.4s\n"
          "sqxtn2 v16.8h, v17.4s\n"
          "sqadd v16.8h, v16.8h, v28.8h\n"
          "sqxtun v16.8b, v16.8h\n"
          "umax v16.8b, v16.8b, v30.8b\n"
          "umin v16.8b, v16.8b, v31.8b\n"
          "st1 {v16.8b}, [%[output_ptr]], #8\n"
          "uaddw v8.8h, v26.8h, v8.8b\n"
          "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
          "uaddw v0.8h, v25.8h, v0.8b\n"
          "ld1 {v17.4s}, [%[bias_ptr]], #16\n"

          "bge " DEPTHWISECONV_LABEL_DEPTH_8_LOOP "b\n"

        DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP ":\n"
        "smlal v16.4s, v0.4h, v8.4h\n"
        "smlal2 v17.4s, v0.8h, v8.8h\n"

        "sqrdmulh v16.4s, v16.4s, v27.4s\n"
        "sqrdmulh v17.4s, v17.4s, v27.4s\n"
        "sqrshl v16.4s, v16.4s, v29.4s\n"
        "sqrshl v17.4s, v17.4s, v29.4s\n"

        "sqxtn v16.4h, v16.4s\n"
        "sqxtn2 v16.8h, v17.4s\n"
        "sqadd v16.8h, v16.8h, v28.8h\n"
        "sqxtun v16.8b, v16.8h\n"
        "umax v16.8b, v16.8b, v30.8b\n"
        "umin v16.8b, v16.8b, v31.8b\n"
        "st1 {v16.8b}, [%[output_ptr]]\n"
        :
        // Outputs.
        [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
        [output_ptr] "+r"(output_ptr), [bias_ptr] "+r"(bias_ptr)
        :
        // Inputs.
        [params_ptr] "r"(params_ptr)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v8", "v16", "v17", "v18", "v19", "v25", "v26", "v27", "v28",
        "v29", "v30", "v31",
        // We use these general-purpose registers.
        "x9", "x10", "x11");
#undef DEPTHWISECONV_LABEL_DEPTH_8_LOOP
#undef DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP
  }
};

template <>
struct DepthwiseConvPartial<DepthwiseConvOutputRounding::kAwayFromZero,
                            EdgeType::kCorner, 1, 1> {
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         const DepthwiseConvParams* params_ptr) {
#define DEPTHWISECONV_LABEL_DEPTH_8_LOOP "1"
#define DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "2"
    asm volatile(
        // Performs depthwise convolutions for an input window of size 2x2 and
        // padding of 1 across the full depth. Expects |input_ptr| and
        // |filter_ptr| to be pointing to the beginning of the 2x2 input and
        // filter values.

        // Load input and filter values.
        "ldr x15, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"
        "ldr x9, [%[params_ptr], #" STR(OFFSET_INPUT_ROW_SIZE) "]\n"
        "cmp x15, #16\n"
        "add x12, %[input_ptr], x15\n"
        "add x13, %[input_ptr], x9\n"
        "ld1 {v8.8b}, [%[input_ptr]], #8\n"
        "add x14, x13, x15\n"
        "ld1 {v9.8b}, [x12], #8\n"
        "ldr x6, [%[params_ptr], #" STR(OFFSET_FILTER_ROW_SIZE) "]\n"

        "add x9, %[filter_ptr], x15\n"
        "ld1 {v10.8b}, [x13], #8\n"
        "add x10, %[filter_ptr], x6\n"
        "ld1 {v11.8b}, [x14], #8\n"
        "ld1 {v0.8b}, [%[filter_ptr]], #8\n"
        "add x11, x10, x15\n"
        "ld1 {v1.8b}, [x9], #8\n"
        "ld1 {v2.8b}, [x10], #8\n"
        "ld1 {v3.8b}, [x11], #8\n"

        // Load constants.
        "ldr w6, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "ldr w7, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "dup v26.8h, w6\n"
        "ldr w6, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w7\n"
        "ldr w7, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "dup v28.8h, w6\n"
        "ldr w6, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v29.4s, w7\n"
        "ldr w7, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v30.16b, w6\n"
        "ldr w6, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"
        "dup v31.16b, w7\n"
        "dup v25.8h, w6\n"

        // Add input and filter offsets.
        "uaddw v8.8h, v26.8h, v8.8b\n"
        "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
        "uaddw v9.8h, v26.8h, v9.8b\n"
        "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
        "uaddw v10.8h, v26.8h, v10.8b\n"
        "uaddw v11.8h, v26.8h, v11.8b\n"

        "uaddw v0.8h, v25.8h, v0.8b\n"
        "uaddw v1.8h, v25.8h, v1.8b\n"
        "uaddw v2.8h, v25.8h, v2.8b\n"
        "uaddw v3.8h, v25.8h, v3.8b\n"

        "blt " DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_DEPTH_8_LOOP ":\n"
          "smlal v16.4s, v0.4h, v8.4h\n"
          "subs x15, x15, #8\n"
          "smlal2 v17.4s, v0.8h, v8.8h\n"
          "ld1 {v8.8b}, [%[input_ptr]], #8\n"
          "cmp x15, #16\n"
          "ld1 {v0.8b}, [%[filter_ptr]], #8\n"
          "smlal v16.4s, v1.4h, v9.4h\n"
          "smlal2 v17.4s, v1.8h, v9.8h\n"
          "ld1 {v9.8b}, [x12], #8\n"
          "smlal v16.4s, v2.4h, v10.4h\n"
          "ld1 {v1.8b}, [x9], #8\n"
          "smlal2 v17.4s, v2.8h, v10.8h\n"
          "ld1 {v10.8b}, [x13], #8\n"
          "smlal v16.4s, v3.4h, v11.4h\n"
          "ld1 {v2.8b}, [x10], #8\n"
          "smlal2 v17.4s, v3.8h, v11.8h\n"
          "ld1 {v11.8b}, [x14], #8\n"
          "ld1 {v3.8b}, [x11], #8\n"

          "sqrdmulh v16.4s, v16.4s, v27.4s\n"
          "sqrdmulh v17.4s, v17.4s, v27.4s\n"
          "and v18.16b, v16.16b, v29.16b\n"
          "and v19.16b, v17.16b, v29.16b\n"
          "sshr v18.4s, v18.4s, #31\n"
          "sshr v19.4s, v19.4s, #31\n"
          "sqadd v16.4s, v16.4s, v18.4s\n"
          "sqadd v17.4s, v17.4s, v19.4s\n"
          "srshl v16.4s, v16.4s, v29.4s\n"
          "srshl v17.4s, v17.4s, v29.4s\n"
          "sqxtn v16.4h, v16.4s\n"
          "sqxtn2 v16.8h, v17.4s\n"
          "sqadd v16.8h, v16.8h, v28.8h\n"
          "sqxtun v16.8b, v16.8h\n"
          "umax v16.8b, v16.8b, v30.8b\n"
          "umin v16.8b, v16.8b, v31.8b\n"
          "st1 {v16.8b}, [%[output_ptr]], #8\n"
          "uaddw v8.8h, v26.8h, v8.8b\n"
          "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
          "uaddw v10.8h, v26.8h, v10.8b\n"
          "uaddw v11.8h, v26.8h, v11.8b\n"
          "uaddw v0.8h, v25.8h, v0.8b\n"
          "uaddw v1.8h, v25.8h, v1.8b\n"
          "uaddw v2.8h, v25.8h, v2.8b\n"
          "uaddw v3.8h, v25.8h, v3.8b\n"

          "bge " DEPTHWISECONV_LABEL_DEPTH_8_LOOP "b\n"

        DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP ":\n"
        "smlal v16.4s, v0.4h, v8.4h\n"
        "smlal2 v17.4s, v0.8h, v8.8h\n"
        "smlal v16.4s, v1.4h, v9.4h\n"
        "smlal2 v17.4s, v1.8h, v9.8h\n"
        "smlal v16.4s, v2.4h, v10.4h\n"
        "smlal2 v17.4s, v2.8h, v10.8h\n"
        "smlal v16.4s, v3.4h, v11.4h\n"
        "smlal2 v17.4s, v3.8h, v11.8h\n"

        "sqrdmulh v16.4s, v16.4s, v27.4s\n"
        "sqrdmulh v17.4s, v17.4s, v27.4s\n"
        "and v18.16b, v16.16b, v29.16b\n"
        "and v19.16b, v17.16b, v29.16b\n"
        "sshr v18.4s, v18.4s, #31\n"
        "sshr v19.4s, v19.4s, #31\n"
        "sqadd v16.4s, v16.4s, v18.4s\n"
        "sqadd v17.4s, v17.4s, v19.4s\n"
        "srshl v16.4s, v16.4s, v29.4s\n"
        "srshl v17.4s, v17.4s, v29.4s\n"

        "sqxtn v16.4h, v16.4s\n"
        "sqxtn2 v16.8h, v17.4s\n"
        "sqadd v16.8h, v16.8h, v28.8h\n"
        "sqxtun v16.8b, v16.8h\n"
        "umax v16.8b, v16.8b, v30.8b\n"
        "umin v16.8b, v16.8b, v31.8b\n"
        "st1 {v16.8b}, [%[output_ptr]]\n"
        :
        // Outputs.
        [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
        [output_ptr] "+r"(output_ptr), [bias_ptr] "+r"(bias_ptr)
        :
        // Inputs.
        [params_ptr] "r"(params_ptr)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v8", "v9", "v10", "v11", "v16", "v17", "v18",
        "v19", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
        // We use these general-purpose registers.
        "x6", "x7", "x9", "x10", "x11", "x12", "x13", "x14", "x15");
#undef DEPTHWISECONV_LABEL_DEPTH_8_LOOP
#undef DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP
  }
};

template <>
struct DepthwiseConvPartial<DepthwiseConvOutputRounding::kUpward,
                            EdgeType::kCorner, 1, 1> {
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         const DepthwiseConvParams* params_ptr) {
#define DEPTHWISECONV_LABEL_DEPTH_8_LOOP "1"
#define DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "2"
    asm volatile(
        // Performs depthwise convolutions for an input window of size 2x2 and
        // padding of 1 across the full depth. Expects |input_ptr| and
        // |filter_ptr| to be pointing to the beginning of the 2x2 input and
        // filter values.

        // Load input and filter values.
        "ldr x15, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"
        "ldr x9, [%[params_ptr], #" STR(OFFSET_INPUT_ROW_SIZE) "]\n"
        "cmp x15, #16\n"
        "add x12, %[input_ptr], x15\n"
        "add x13, %[input_ptr], x9\n"
        "ld1 {v8.8b}, [%[input_ptr]], #8\n"
        "add x14, x13, x15\n"
        "ld1 {v9.8b}, [x12], #8\n"
        "ldr x6, [%[params_ptr], #" STR(OFFSET_FILTER_ROW_SIZE) "]\n"

        "add x9, %[filter_ptr], x15\n"
        "ld1 {v10.8b}, [x13], #8\n"
        "add x10, %[filter_ptr], x6\n"
        "ld1 {v11.8b}, [x14], #8\n"
        "ld1 {v0.8b}, [%[filter_ptr]], #8\n"
        "add x11, x10, x15\n"
        "ld1 {v1.8b}, [x9], #8\n"
        "ld1 {v2.8b}, [x10], #8\n"
        "ld1 {v3.8b}, [x11], #8\n"

        // Load constants.
        "ldr w6, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "ldr w7, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "dup v26.8h, w6\n"
        "ldr w6, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w7\n"
        "ldr w7, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "dup v28.8h, w6\n"
        "ldr w6, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v29.4s, w7\n"
        "ldr w7, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v30.16b, w6\n"
        "ldr w6, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"
        "dup v31.16b, w7\n"
        "dup v25.8h, w6\n"

        // Add input and filter offsets.
        "uaddw v8.8h, v26.8h, v8.8b\n"
        "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
        "uaddw v9.8h, v26.8h, v9.8b\n"
        "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
        "uaddw v10.8h, v26.8h, v10.8b\n"
        "uaddw v11.8h, v26.8h, v11.8b\n"

        "uaddw v0.8h, v25.8h, v0.8b\n"
        "uaddw v1.8h, v25.8h, v1.8b\n"
        "uaddw v2.8h, v25.8h, v2.8b\n"
        "uaddw v3.8h, v25.8h, v3.8b\n"

        "blt " DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_DEPTH_8_LOOP ":\n"
          "smlal v16.4s, v0.4h, v8.4h\n"
          "subs x15, x15, #8\n"
          "smlal2 v17.4s, v0.8h, v8.8h\n"
          "ld1 {v8.8b}, [%[input_ptr]], #8\n"
          "cmp x15, #16\n"
          "ld1 {v0.8b}, [%[filter_ptr]], #8\n"
          "smlal v16.4s, v1.4h, v9.4h\n"
          "smlal2 v17.4s, v1.8h, v9.8h\n"
          "ld1 {v9.8b}, [x12], #8\n"
          "smlal v16.4s, v2.4h, v10.4h\n"
          "ld1 {v1.8b}, [x9], #8\n"
          "smlal2 v17.4s, v2.8h, v10.8h\n"
          "ld1 {v10.8b}, [x13], #8\n"
          "smlal v16.4s, v3.4h, v11.4h\n"
          "ld1 {v2.8b}, [x10], #8\n"
          "smlal2 v17.4s, v3.8h, v11.8h\n"
          "ld1 {v11.8b}, [x14], #8\n"
          "ld1 {v3.8b}, [x11], #8\n"

          "sqrdmulh v16.4s, v16.4s, v27.4s\n"
          "sqrdmulh v17.4s, v17.4s, v27.4s\n"
          "sqrshl v16.4s, v16.4s, v29.4s\n"
          "sqrshl v17.4s, v17.4s, v29.4s\n"
          "sqxtn v16.4h, v16.4s\n"
          "sqxtn2 v16.8h, v17.4s\n"
          "sqadd v16.8h, v16.8h, v28.8h\n"
          "sqxtun v16.8b, v16.8h\n"
          "umax v16.8b, v16.8b, v30.8b\n"
          "umin v16.8b, v16.8b, v31.8b\n"
          "st1 {v16.8b}, [%[output_ptr]], #8\n"
          "uaddw v8.8h, v26.8h, v8.8b\n"
          "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
          "uaddw v10.8h, v26.8h, v10.8b\n"
          "uaddw v11.8h, v26.8h, v11.8b\n"
          "uaddw v0.8h, v25.8h, v0.8b\n"
          "uaddw v1.8h, v25.8h, v1.8b\n"
          "uaddw v2.8h, v25.8h, v2.8b\n"
          "uaddw v3.8h, v25.8h, v3.8b\n"

          "bge " DEPTHWISECONV_LABEL_DEPTH_8_LOOP "b\n"

        DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP ":\n"
        "smlal v16.4s, v0.4h, v8.4h\n"
        "smlal2 v17.4s, v0.8h, v8.8h\n"
        "smlal v16.4s, v1.4h, v9.4h\n"
        "smlal2 v17.4s, v1.8h, v9.8h\n"
        "smlal v16.4s, v2.4h, v10.4h\n"
        "smlal2 v17.4s, v2.8h, v10.8h\n"
        "smlal v16.4s, v3.4h, v11.4h\n"
        "smlal2 v17.4s, v3.8h, v11.8h\n"

        "sqrdmulh v16.4s, v16.4s, v27.4s\n"
        "sqrdmulh v17.4s, v17.4s, v27.4s\n"
        "sqrshl v16.4s, v16.4s, v29.4s\n"
        "sqrshl v17.4s, v17.4s, v29.4s\n"

        "sqxtn v16.4h, v16.4s\n"
        "sqxtn2 v16.8h, v17.4s\n"
        "sqadd v16.8h, v16.8h, v28.8h\n"
        "sqxtun v16.8b, v16.8h\n"
        "umax v16.8b, v16.8b, v30.8b\n"
        "umin v16.8b, v16.8b, v31.8b\n"
        "st1 {v16.8b}, [%[output_ptr]]\n"
        :
        // Outputs.
        [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
        [output_ptr] "+r"(output_ptr), [bias_ptr] "+r"(bias_ptr)
        :
        // Inputs.
        [params_ptr] "r"(params_ptr)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v8", "v9", "v10", "v11", "v16", "v17", "v18",
        "v19", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
        // We use these general-purpose registers.
        "x6", "x7", "x9", "x10", "x11", "x12", "x13", "x14", "x15");
#undef DEPTHWISECONV_LABEL_DEPTH_8_LOOP
#undef DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP
  }
};

template <>
struct DepthwiseConvPartial<DepthwiseConvOutputRounding::kAwayFromZero,
                            EdgeType::kHorizontal, 1, 1> {
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         const DepthwiseConvParams* params_ptr) {
#define DEPTHWISECONV_LABEL_DEPTH_8_LOOP "1"
#define DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "2"
    asm volatile(
        // Performs depthwise convolutions for an input window of size 2x3 and
        // padding of 1 across the full depth. Expects |input_ptr| and
        // |filter_ptr| to be pointing to the beginning of the 2x3 input and
        // filter values.

        // Load input and filter values.
        "ldr x7, [%[params_ptr], #" STR(OFFSET_INPUT_DEPTH) "]\n"
        "mov x12, %[input_ptr]\n"
        "ldr x11, [%[params_ptr], #" STR(OFFSET_INPUT_ROW_SIZE) "]\n"
        "mov x9, %[filter_ptr]\n"
        "ldr x14, [%[params_ptr], #" STR(OFFSET_FILTER_ROW_SIZE) "]\n"
        "add x13, x12, x11\n"
        "ldr x15, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"

        "ld1 {v8.8b}, [x12], x7\n"
        "add x10, x9, x14\n"
        "ld1 {v9.8b}, [x12], x7\n"
        "cmp x15, #16\n"
        "ld1 {v10.8b}, [x12]\n"
        "add %[input_ptr], %[input_ptr], #8\n"
        "ld1 {v11.8b}, [x13], x7\n"
        "add %[filter_ptr], %[filter_ptr], #8\n"
        "ld1 {v12.8b}, [x13], x7\n"
        "ld1 {v13.8b}, [x13]\n"

        "ld1 {v0.8b}, [x9], x7\n"
        "ld1 {v1.8b}, [x9], x7\n"
        "ld1 {v2.8b}, [x9]\n"
        "ld1 {v3.8b}, [x10], x7\n"
        "ld1 {v4.8b}, [x10], x7\n"
        "ld1 {v5.8b}, [x10]\n"

        // Load constants.
        "ldr w12, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "dup v26.8h, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w13\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "dup v28.8h, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v29.4s, w13\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v30.8b, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"
        "dup v31.8b, w13\n"
        "dup v25.8h, w12\n"

        // Add input and filter offsets.
        "uaddw v8.8h, v26.8h, v8.8b\n"
        "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
        "uaddw v9.8h, v26.8h, v9.8b\n"
        "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
        "uaddw v10.8h, v26.8h, v10.8b\n"
        "uaddw v11.8h, v26.8h, v11.8b\n"
        "uaddw v12.8h, v26.8h, v12.8b\n"
        "uaddw v13.8h, v26.8h, v13.8b\n"

        "uaddw v0.8h, v25.8h, v0.8b\n"
        "uaddw v1.8h, v25.8h, v1.8b\n"
        "uaddw v2.8h, v25.8h, v2.8b\n"
        "uaddw v3.8h, v25.8h, v3.8b\n"
        "uaddw v4.8h, v25.8h, v4.8b\n"
        "uaddw v5.8h, v25.8h, v5.8b\n"

        "blt " DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_DEPTH_8_LOOP ":\n"
          "mov x12, %[input_ptr]\n"
          "subs x15, x15, #8\n"
          "add x13, x12, x11\n"
          "cmp x15, #16\n"
          "add %[input_ptr], %[input_ptr], #8\n"

          "smlal v16.4s, v0.4h, v8.4h\n"
          "mov x9, %[filter_ptr]\n"
          "smlal2 v17.4s, v0.8h, v8.8h\n"
          "ld1 {v8.8b}, [x12], x7\n"
          "smlal v16.4s, v1.4h, v9.4h\n"
          "add x10, x9, x14\n"
          "smlal2 v17.4s, v1.8h, v9.8h\n"
          "ld1 {v9.8b}, [x12], x7\n"
          "smlal v16.4s, v2.4h, v10.4h\n"
          "add %[filter_ptr], %[filter_ptr], #8\n"
          "smlal2 v17.4s, v2.8h, v10.8h\n"
          "ld1 {v10.8b}, [x12]\n"
          "smlal v16.4s, v3.4h, v11.4h\n"
          "ld1 {v0.8b}, [x9], x7\n"
          "smlal2 v17.4s, v3.8h, v11.8h\n"
          "ld1 {v11.8b}, [x13], x7\n"
          "smlal v16.4s, v4.4h, v12.4h\n"
          "ld1 {v1.8b}, [x9], x7\n"
          "smlal2 v17.4s, v4.8h, v12.8h\n"
          "ld1 {v12.8b}, [x13], x7\n"
          "smlal v16.4s, v5.4h, v13.4h\n"
          "ld1 {v2.8b}, [x9]\n"
          "smlal2 v17.4s, v5.8h, v13.8h\n"
          "ld1 {v13.8b}, [x13]\n"

          "sqrdmulh v16.4s, v16.4s, v27.4s\n"
          "ld1 {v3.8b}, [x10], x7\n"
          "sqrdmulh v17.4s, v17.4s, v27.4s\n"
          "ld1 {v4.8b}, [x10], x7\n"
          "and v18.16b, v16.16b, v29.16b\n"
          "ld1 {v5.8b}, [x10]\n"
          "and v19.16b, v17.16b, v29.16b\n"
          "sshr v18.4s, v18.4s, #31\n"
          "sshr v19.4s, v19.4s, #31\n"
          "sqadd v16.4s, v16.4s, v18.4s\n"
          "sqadd v17.4s, v17.4s, v19.4s\n"
          "srshl v16.4s, v16.4s, v29.4s\n"
          "srshl v17.4s, v17.4s, v29.4s\n"
          "sqxtn v16.4h, v16.4s\n"
          "sqxtn2 v16.8h, v17.4s\n"
          "sqadd v16.8h, v16.8h, v28.8h\n"
          "sqxtun v16.8b, v16.8h\n"
          "umax v16.8b, v16.8b, v30.8b\n"
          "umin v16.8b, v16.8b, v31.8b\n"
          "uaddw v8.8h, v26.8h, v8.8b\n"
          "st1 {v16.8b}, [%[output_ptr]], #8\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "uaddw v10.8h, v26.8h, v10.8b\n"
          "uaddw v11.8h, v26.8h, v11.8b\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "uaddw v13.8h, v26.8h, v13.8b\n"

          "uaddw v0.8h, v25.8h, v0.8b\n"
          "uaddw v1.8h, v25.8h, v1.8b\n"
          "uaddw v2.8h, v25.8h, v2.8b\n"
          "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
          "uaddw v3.8h, v25.8h, v3.8b\n"
          "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
          "uaddw v4.8h, v25.8h, v4.8b\n"
          "uaddw v5.8h, v25.8h, v5.8b\n"

          "bge " DEPTHWISECONV_LABEL_DEPTH_8_LOOP "b\n"

        DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP ":\n"
        "smlal v16.4s, v0.4h, v8.4h\n"
        "smlal2 v17.4s, v0.8h, v8.8h\n"
        "smlal v16.4s, v1.4h, v9.4h\n"
        "smlal2 v17.4s, v1.8h, v9.8h\n"
        "smlal v16.4s, v2.4h, v10.4h\n"
        "smlal2 v17.4s, v2.8h, v10.8h\n"
        "smlal v16.4s, v3.4h, v11.4h\n"
        "smlal2 v17.4s, v3.8h, v11.8h\n"
        "smlal v16.4s, v4.4h, v12.4h\n"
        "smlal2 v17.4s, v4.8h, v12.8h\n"
        "smlal v16.4s, v5.4h, v13.4h\n"
        "smlal2 v17.4s, v5.8h, v13.8h\n"

        "sqrdmulh v16.4s, v16.4s, v27.4s\n"
        "sqrdmulh v17.4s, v17.4s, v27.4s\n"
        "and v18.16b, v16.16b, v29.16b\n"
        "and v19.16b, v17.16b, v29.16b\n"
        "sshr v18.4s, v18.4s, #31\n"
        "sshr v19.4s, v19.4s, #31\n"
        "sqadd v16.4s, v16.4s, v18.4s\n"
        "sqadd v17.4s, v17.4s, v19.4s\n"
        "srshl v16.4s, v16.4s, v29.4s\n"
        "srshl v17.4s, v17.4s, v29.4s\n"
        "sqxtn v16.4h, v16.4s\n"
        "sqxtn2 v16.8h, v17.4s\n"
        "sqadd v16.8h, v16.8h, v28.8h\n"
        "sqxtun v16.8b, v16.8h\n"
        "umax v16.8b, v16.8b, v30.8b\n"
        "umin v16.8b, v16.8b, v31.8b\n"
        "st1 {v16.8b}, [%[output_ptr]]\n"
        :
        // Outputs.
        [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
        [output_ptr] "+r"(output_ptr), [bias_ptr] "+r"(bias_ptr)
        :
        // Inputs.
        [params_ptr] "r"(params_ptr)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v4", "v5", "v8", "v9", "v10", "v11", "v12",
        "v13", "v16", "v17", "v18", "v19", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31",
        // We use these general-purpose registers.
        "x7", "x9", "x10", "x11", "x12", "x13", "x14", "x15");
#undef DEPTHWISECONV_LABEL_DEPTH_8_LOOP
#undef DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP
  }
};

template <>
struct DepthwiseConvPartial<DepthwiseConvOutputRounding::kUpward,
                            EdgeType::kHorizontal, 1, 1> {
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         const DepthwiseConvParams* params_ptr) {
#define DEPTHWISECONV_LABEL_DEPTH_8_LOOP "1"
#define DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "2"
    asm volatile(
        // Performs depthwise convolutions for an input window of size 2x3 and
        // padding of 1 across the full depth. Expects |input_ptr| and
        // |filter_ptr| to be pointing to the beginning of the 2x3 input and
        // filter values.

        // Load input and filter values.
        "ldr x7, [%[params_ptr], #" STR(OFFSET_INPUT_DEPTH) "]\n"
        "mov x12, %[input_ptr]\n"
        "ldr x11, [%[params_ptr], #" STR(OFFSET_INPUT_ROW_SIZE) "]\n"
        "mov x9, %[filter_ptr]\n"
        "ldr x14, [%[params_ptr], #" STR(OFFSET_FILTER_ROW_SIZE) "]\n"
        "add x13, x12, x11\n"
        "ldr x15, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"

        "ld1 {v8.8b}, [x12], x7\n"
        "add x10, x9, x14\n"
        "ld1 {v9.8b}, [x12], x7\n"
        "cmp x15, #16\n"
        "ld1 {v10.8b}, [x12]\n"
        "add %[input_ptr], %[input_ptr], #8\n"
        "ld1 {v11.8b}, [x13], x7\n"
        "add %[filter_ptr], %[filter_ptr], #8\n"
        "ld1 {v12.8b}, [x13], x7\n"
        "ld1 {v13.8b}, [x13]\n"

        "ld1 {v0.8b}, [x9], x7\n"
        "ld1 {v1.8b}, [x9], x7\n"
        "ld1 {v2.8b}, [x9]\n"
        "ld1 {v3.8b}, [x10], x7\n"
        "ld1 {v4.8b}, [x10], x7\n"
        "ld1 {v5.8b}, [x10]\n"

        // Load constants.
        "ldr w12, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "dup v26.8h, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w13\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "dup v28.8h, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v29.4s, w13\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v30.8b, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"
        "dup v31.8b, w13\n"
        "dup v25.8h, w12\n"

        // Add input and filter offsets.
        "uaddw v8.8h, v26.8h, v8.8b\n"
        "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
        "uaddw v9.8h, v26.8h, v9.8b\n"
        "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
        "uaddw v10.8h, v26.8h, v10.8b\n"
        "uaddw v11.8h, v26.8h, v11.8b\n"
        "uaddw v12.8h, v26.8h, v12.8b\n"
        "uaddw v13.8h, v26.8h, v13.8b\n"

        "uaddw v0.8h, v25.8h, v0.8b\n"
        "uaddw v1.8h, v25.8h, v1.8b\n"
        "uaddw v2.8h, v25.8h, v2.8b\n"
        "uaddw v3.8h, v25.8h, v3.8b\n"
        "uaddw v4.8h, v25.8h, v4.8b\n"
        "uaddw v5.8h, v25.8h, v5.8b\n"

        "blt " DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_DEPTH_8_LOOP ":\n"
          "mov x12, %[input_ptr]\n"
          "subs x15, x15, #8\n"
          "add x13, x12, x11\n"
          "cmp x15, #16\n"
          "add %[input_ptr], %[input_ptr], #8\n"

          "smlal v16.4s, v0.4h, v8.4h\n"
          "mov x9, %[filter_ptr]\n"
          "smlal2 v17.4s, v0.8h, v8.8h\n"
          "ld1 {v8.8b}, [x12], x7\n"
          "smlal v16.4s, v1.4h, v9.4h\n"
          "add x10, x9, x14\n"
          "smlal2 v17.4s, v1.8h, v9.8h\n"
          "ld1 {v9.8b}, [x12], x7\n"
          "smlal v16.4s, v2.4h, v10.4h\n"
          "add %[filter_ptr], %[filter_ptr], #8\n"
          "smlal2 v17.4s, v2.8h, v10.8h\n"
          "ld1 {v10.8b}, [x12]\n"
          "smlal v16.4s, v3.4h, v11.4h\n"
          "ld1 {v0.8b}, [x9], x7\n"
          "smlal2 v17.4s, v3.8h, v11.8h\n"
          "ld1 {v11.8b}, [x13], x7\n"
          "smlal v16.4s, v4.4h, v12.4h\n"
          "ld1 {v1.8b}, [x9], x7\n"
          "smlal2 v17.4s, v4.8h, v12.8h\n"
          "ld1 {v12.8b}, [x13], x7\n"
          "smlal v16.4s, v5.4h, v13.4h\n"
          "ld1 {v2.8b}, [x9]\n"
          "smlal2 v17.4s, v5.8h, v13.8h\n"
          "ld1 {v13.8b}, [x13]\n"

          "sqrdmulh v16.4s, v16.4s, v27.4s\n"
          "ld1 {v3.8b}, [x10], x7\n"
          "sqrdmulh v17.4s, v17.4s, v27.4s\n"
          "ld1 {v4.8b}, [x10], x7\n"
          "sqrshl v16.4s, v16.4s, v29.4s\n"
          "ld1 {v5.8b}, [x10]\n"
          "sqrshl v17.4s, v17.4s, v29.4s\n"
          "sqxtn v16.4h, v16.4s\n"
          "sqxtn2 v16.8h, v17.4s\n"
          "sqadd v16.8h, v16.8h, v28.8h\n"
          "sqxtun v16.8b, v16.8h\n"
          "umax v16.8b, v16.8b, v30.8b\n"
          "umin v16.8b, v16.8b, v31.8b\n"
          "uaddw v8.8h, v26.8h, v8.8b\n"
          "st1 {v16.8b}, [%[output_ptr]], #8\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "uaddw v10.8h, v26.8h, v10.8b\n"
          "uaddw v11.8h, v26.8h, v11.8b\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "uaddw v13.8h, v26.8h, v13.8b\n"

          "uaddw v0.8h, v25.8h, v0.8b\n"
          "uaddw v1.8h, v25.8h, v1.8b\n"
          "uaddw v2.8h, v25.8h, v2.8b\n"
          "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
          "uaddw v3.8h, v25.8h, v3.8b\n"
          "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
          "uaddw v4.8h, v25.8h, v4.8b\n"
          "uaddw v5.8h, v25.8h, v5.8b\n"

          "bge " DEPTHWISECONV_LABEL_DEPTH_8_LOOP "b\n"

        DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP ":\n"
        "smlal v16.4s, v0.4h, v8.4h\n"
        "smlal2 v17.4s, v0.8h, v8.8h\n"
        "smlal v16.4s, v1.4h, v9.4h\n"
        "smlal2 v17.4s, v1.8h, v9.8h\n"
        "smlal v16.4s, v2.4h, v10.4h\n"
        "smlal2 v17.4s, v2.8h, v10.8h\n"
        "smlal v16.4s, v3.4h, v11.4h\n"
        "smlal2 v17.4s, v3.8h, v11.8h\n"
        "smlal v16.4s, v4.4h, v12.4h\n"
        "smlal2 v17.4s, v4.8h, v12.8h\n"
        "smlal v16.4s, v5.4h, v13.4h\n"
        "smlal2 v17.4s, v5.8h, v13.8h\n"

        "sqrdmulh v16.4s, v16.4s, v27.4s\n"
        "sqrdmulh v17.4s, v17.4s, v27.4s\n"
        "sqrshl v16.4s, v16.4s, v29.4s\n"
        "sqrshl v17.4s, v17.4s, v29.4s\n"
        "sqxtn v16.4h, v16.4s\n"
        "sqxtn2 v16.8h, v17.4s\n"
        "sqadd v16.8h, v16.8h, v28.8h\n"
        "sqxtun v16.8b, v16.8h\n"
        "umax v16.8b, v16.8b, v30.8b\n"
        "umin v16.8b, v16.8b, v31.8b\n"
        "st1 {v16.8b}, [%[output_ptr]]\n"
        :
        // Outputs.
        [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
        [output_ptr] "+r"(output_ptr), [bias_ptr] "+r"(bias_ptr)
        :
        // Inputs.
        [params_ptr] "r"(params_ptr)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v4", "v5", "v8", "v9", "v10", "v11", "v12",
        "v13", "v16", "v17", "v18", "v19", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31",
        // We use these general-purpose registers.
        "x7", "x9", "x10", "x11", "x12", "x13", "x14", "x15");
#undef DEPTHWISECONV_LABEL_DEPTH_8_LOOP
#undef DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP
  }
};

template <>
struct DepthwiseConvPartial<DepthwiseConvOutputRounding::kAwayFromZero,
                            EdgeType::kVertical, 1, 1> {
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         const DepthwiseConvParams* params_ptr) {
#define DEPTHWISECONV_LABEL_DEPTH_8_LOOP "1"
#define DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "2"
    asm volatile(
        // Performs depthwise convolutions for an input window of size 3x2 and
        // padding of 1 across the full depth. Expects |input_ptr| and
        // |filter_ptr| to be pointing to the beginning of the 3x2 input and
        // filter values.

        // Load input and filter values.
        "ldr x6, [%[params_ptr], #" STR(OFFSET_INPUT_DEPTH) "]\n"
        "mov x12, %[input_ptr]\n"
        "ldr x11, [%[params_ptr], #" STR(OFFSET_INPUT_ROW_SIZE) "]\n"
        "mov x7, %[filter_ptr]\n"
        "ldr x5, [%[params_ptr], #" STR(OFFSET_FILTER_ROW_SIZE) "]\n"
        "add x13, x12, x11\n"
        "ldr x15, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"
        "add x14, x13, x11\n"

        "ld1 {v8.8b}, [x12], x6\n"
        "add x9, x7, x5\n"
        "ld1 {v9.8b}, [x12]\n"
        "cmp x15, #16\n"
        "add x10, x9, x5\n"
        "ld1 {v10.8b}, [x13], x6\n"
        "add %[input_ptr], %[input_ptr], #8\n"
        "ld1 {v11.8b}, [x13]\n"
        "add %[filter_ptr], %[filter_ptr], #8\n"
        "ld1 {v12.8b}, [x14], x6\n"
        "ld1 {v13.8b}, [x14]\n"

        "ld1 {v0.8b}, [x7], x6\n"
        "ld1 {v1.8b}, [x7]\n"
        "ld1 {v2.8b}, [x9], x6\n"
        "ld1 {v3.8b}, [x9]\n"
        "ld1 {v4.8b}, [x10], x6\n"
        "ld1 {v5.8b}, [x10]\n"

        // Load constants.
        "ldr w12, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "dup v26.8h, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w13\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "dup v28.8h, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v29.4s, w13\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v30.8b, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"
        "dup v31.8b, w13\n"
        "dup v25.8h, w12\n"

        // Add input and filter offsets.
        "uaddw v8.8h, v26.8h, v8.8b\n"
        "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
        "uaddw v9.8h, v26.8h, v9.8b\n"
        "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
        "uaddw v10.8h, v26.8h, v10.8b\n"
        "uaddw v11.8h, v26.8h, v11.8b\n"
        "uaddw v12.8h, v26.8h, v12.8b\n"
        "uaddw v13.8h, v26.8h, v13.8b\n"

        "uaddw v0.8h, v25.8h, v0.8b\n"
        "uaddw v1.8h, v25.8h, v1.8b\n"
        "uaddw v2.8h, v25.8h, v2.8b\n"
        "uaddw v3.8h, v25.8h, v3.8b\n"
        "uaddw v4.8h, v25.8h, v4.8b\n"
        "uaddw v5.8h, v25.8h, v5.8b\n"

        "blt " DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_DEPTH_8_LOOP ":\n"
          "mov x12, %[input_ptr]\n"
          "subs x15, x15, #8\n"
          "add x13, x12, x11\n"
          "cmp x15, #16\n"
          "add x14, x13, x11\n"
          "add %[input_ptr], %[input_ptr], #8\n"

          "smlal v16.4s, v0.4h, v8.4h\n"
          "mov x7, %[filter_ptr]\n"
          "smlal2 v17.4s, v0.8h, v8.8h\n"
          "ld1 {v8.8b}, [x12], x6\n"
          "smlal v16.4s, v1.4h, v9.4h\n"
          "add x9, x7, x5\n"
          "smlal2 v17.4s, v1.8h, v9.8h\n"
          "add x10, x9, x5\n"
          "ld1 {v9.8b}, [x12]\n"
          "smlal v16.4s, v2.4h, v10.4h\n"
          "add %[filter_ptr], %[filter_ptr], #8\n"
          "smlal2 v17.4s, v2.8h, v10.8h\n"
          "ld1 {v10.8b}, [x13], x6\n"
          "smlal v16.4s, v3.4h, v11.4h\n"
          "ld1 {v0.8b}, [x7], x6\n"
          "smlal2 v17.4s, v3.8h, v11.8h\n"
          "ld1 {v11.8b}, [x13]\n"
          "smlal v16.4s, v4.4h, v12.4h\n"
          "ld1 {v1.8b}, [x7]\n"
          "smlal2 v17.4s, v4.8h, v12.8h\n"
          "ld1 {v12.8b}, [x14], x6\n"
          "smlal v16.4s, v5.4h, v13.4h\n"
          "ld1 {v2.8b}, [x9], x6\n"
          "smlal2 v17.4s, v5.8h, v13.8h\n"
          "ld1 {v13.8b}, [x14]\n"

          "sqrdmulh v16.4s, v16.4s, v27.4s\n"
          "ld1 {v3.8b}, [x9]\n"
          "sqrdmulh v17.4s, v17.4s, v27.4s\n"
          "ld1 {v4.8b}, [x10], x6\n"
          "and v18.16b, v16.16b, v29.16b\n"
          "ld1 {v5.8b}, [x10]\n"
          "and v19.16b, v17.16b, v29.16b\n"
          "sshr v18.4s, v18.4s, #31\n"
          "sshr v19.4s, v19.4s, #31\n"
          "sqadd v16.4s, v16.4s, v18.4s\n"
          "sqadd v17.4s, v17.4s, v19.4s\n"
          "srshl v16.4s, v16.4s, v29.4s\n"
          "srshl v17.4s, v17.4s, v29.4s\n"
          "sqxtn v16.4h, v16.4s\n"
          "sqxtn2 v16.8h, v17.4s\n"
          "sqadd v16.8h, v16.8h, v28.8h\n"
          "sqxtun v16.8b, v16.8h\n"
          "umax v16.8b, v16.8b, v30.8b\n"
          "umin v16.8b, v16.8b, v31.8b\n"
          "uaddw v8.8h, v26.8h, v8.8b\n"
          "st1 {v16.8b}, [%[output_ptr]], #8\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "uaddw v10.8h, v26.8h, v10.8b\n"
          "uaddw v11.8h, v26.8h, v11.8b\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "uaddw v13.8h, v26.8h, v13.8b\n"

          "uaddw v0.8h, v25.8h, v0.8b\n"
          "uaddw v1.8h, v25.8h, v1.8b\n"
          "uaddw v2.8h, v25.8h, v2.8b\n"
          "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
          "uaddw v3.8h, v25.8h, v3.8b\n"
          "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
          "uaddw v4.8h, v25.8h, v4.8b\n"
          "uaddw v5.8h, v25.8h, v5.8b\n"

          "bge " DEPTHWISECONV_LABEL_DEPTH_8_LOOP "b\n"

        DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP ":\n"
        "smlal v16.4s, v0.4h, v8.4h\n"
        "smlal2 v17.4s, v0.8h, v8.8h\n"
        "smlal v16.4s, v1.4h, v9.4h\n"
        "smlal2 v17.4s, v1.8h, v9.8h\n"
        "smlal v16.4s, v2.4h, v10.4h\n"
        "smlal2 v17.4s, v2.8h, v10.8h\n"
        "smlal v16.4s, v3.4h, v11.4h\n"
        "smlal2 v17.4s, v3.8h, v11.8h\n"
        "smlal v16.4s, v4.4h, v12.4h\n"
        "smlal2 v17.4s, v4.8h, v12.8h\n"
        "smlal v16.4s, v5.4h, v13.4h\n"
        "smlal2 v17.4s, v5.8h, v13.8h\n"

        "sqrdmulh v16.4s, v16.4s, v27.4s\n"
        "sqrdmulh v17.4s, v17.4s, v27.4s\n"
        "and v18.16b, v16.16b, v29.16b\n"
        "and v19.16b, v17.16b, v29.16b\n"
        "sshr v18.4s, v18.4s, #31\n"
        "sshr v19.4s, v19.4s, #31\n"
        "sqadd v16.4s, v16.4s, v18.4s\n"
        "sqadd v17.4s, v17.4s, v19.4s\n"
        "srshl v16.4s, v16.4s, v29.4s\n"
        "srshl v17.4s, v17.4s, v29.4s\n"
        "sqxtn v16.4h, v16.4s\n"
        "sqxtn2 v16.8h, v17.4s\n"
        "sqadd v16.8h, v16.8h, v28.8h\n"
        "sqxtun v16.8b, v16.8h\n"
        // TODO(b/129852264): Improve testing coverage.
        "umax v16.8b, v16.8b, v30.8b\n"
        "umin v16.8b, v16.8b, v31.8b\n"
        "st1 {v16.8b}, [%[output_ptr]]\n"
        :
        // Outputs.
        [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
        [output_ptr] "+r"(output_ptr), [bias_ptr] "+r"(bias_ptr)
        :
        // Inputs.
        [params_ptr] "r"(params_ptr)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v4", "v5", "v8", "v9", "v10", "v11", "v12",
        "v13", "v16", "v17", "v18", "v19", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31",
        // We use these general-purpose registers.
        "x5", "x6", "x7", "x9", "x10", "x11", "x12", "x13", "x14", "x15");
#undef DEPTHWISECONV_LABEL_DEPTH_8_LOOP
#undef DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP
  }
};

template <>
struct DepthwiseConvPartial<DepthwiseConvOutputRounding::kUpward,
                            EdgeType::kVertical, 1, 1> {
  static inline void Run(const uint8* input_ptr, const uint8* filter_ptr,
                         const int32* bias_ptr, uint8* output_ptr,
                         const DepthwiseConvParams* params_ptr) {
#define DEPTHWISECONV_LABEL_DEPTH_8_LOOP "1"
#define DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "2"
    asm volatile(
        // Performs depthwise convolutions for an input window of size 3x2 and
        // padding of 1 across the full depth. Expects |input_ptr| and
        // |filter_ptr| to be pointing to the beginning of the 3x2 input and
        // filter values.

        // Load input and filter values.
        "ldr x6, [%[params_ptr], #" STR(OFFSET_INPUT_DEPTH) "]\n"
        "mov x12, %[input_ptr]\n"
        "ldr x11, [%[params_ptr], #" STR(OFFSET_INPUT_ROW_SIZE) "]\n"
        "mov x7, %[filter_ptr]\n"
        "ldr x5, [%[params_ptr], #" STR(OFFSET_FILTER_ROW_SIZE) "]\n"
        "add x13, x12, x11\n"
        "ldr x15, [%[params_ptr], #" STR(OFFSET_OUTPUT_DEPTH) "]\n"
        "add x14, x13, x11\n"

        "ld1 {v8.8b}, [x12], x6\n"
        "add x9, x7, x5\n"
        "ld1 {v9.8b}, [x12]\n"
        "cmp x15, #16\n"
        "add x10, x9, x5\n"
        "ld1 {v10.8b}, [x13], x6\n"
        "add %[input_ptr], %[input_ptr], #8\n"
        "ld1 {v11.8b}, [x13]\n"
        "add %[filter_ptr], %[filter_ptr], #8\n"
        "ld1 {v12.8b}, [x14], x6\n"
        "ld1 {v13.8b}, [x14]\n"

        "ld1 {v0.8b}, [x7], x6\n"
        "ld1 {v1.8b}, [x7]\n"
        "ld1 {v2.8b}, [x9], x6\n"
        "ld1 {v3.8b}, [x9]\n"
        "ld1 {v4.8b}, [x10], x6\n"
        "ld1 {v5.8b}, [x10]\n"

        // Load constants.
        "ldr w12, [%[params_ptr], #" STR(OFFSET_INPUT_OFFSET) "]\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_MULTIPLIER) "]\n"
        "dup v26.8h, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_OUTPUT_OFFSET) "]\n"
        "dup v27.4s, w13\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_RIGHT_SHIFT) "]\n"
        "dup v28.8h, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MIN) "]\n"
        "dup v29.4s, w13\n"
        "ldr w13, [%[params_ptr], #" STR(OFFSET_OUTPUT_ACTIVATION_MAX) "]\n"
        "dup v30.8b, w12\n"
        "ldr w12, [%[params_ptr], #" STR(OFFSET_FILTER_OFFSET) "]\n"
        "dup v31.8b, w13\n"
        "dup v25.8h, w12\n"

        // Add input and filter offsets.
        "uaddw v8.8h, v26.8h, v8.8b\n"
        "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
        "uaddw v9.8h, v26.8h, v9.8b\n"
        "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
        "uaddw v10.8h, v26.8h, v10.8b\n"
        "uaddw v11.8h, v26.8h, v11.8b\n"
        "uaddw v12.8h, v26.8h, v12.8b\n"
        "uaddw v13.8h, v26.8h, v13.8b\n"

        "uaddw v0.8h, v25.8h, v0.8b\n"
        "uaddw v1.8h, v25.8h, v1.8b\n"
        "uaddw v2.8h, v25.8h, v2.8b\n"
        "uaddw v3.8h, v25.8h, v3.8b\n"
        "uaddw v4.8h, v25.8h, v4.8b\n"
        "uaddw v5.8h, v25.8h, v5.8b\n"

        "blt " DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP "f\n"

        //"loop_%=:\n"
        DEPTHWISECONV_LABEL_DEPTH_8_LOOP ":\n"
          "mov x12, %[input_ptr]\n"
          "subs x15, x15, #8\n"
          "add x13, x12, x11\n"
          "cmp x15, #16\n"
          "add x14, x13, x11\n"
          "add %[input_ptr], %[input_ptr], #8\n"

          "smlal v16.4s, v0.4h, v8.4h\n"
          "mov x7, %[filter_ptr]\n"
          "smlal2 v17.4s, v0.8h, v8.8h\n"
          "ld1 {v8.8b}, [x12], x6\n"
          "smlal v16.4s, v1.4h, v9.4h\n"
          "add x9, x7, x5\n"
          "smlal2 v17.4s, v1.8h, v9.8h\n"
          "add x10, x9, x5\n"
          "ld1 {v9.8b}, [x12]\n"
          "smlal v16.4s, v2.4h, v10.4h\n"
          "add %[filter_ptr], %[filter_ptr], #8\n"
          "smlal2 v17.4s, v2.8h, v10.8h\n"
          "ld1 {v10.8b}, [x13], x6\n"
          "smlal v16.4s, v3.4h, v11.4h\n"
          "ld1 {v0.8b}, [x7], x6\n"
          "smlal2 v17.4s, v3.8h, v11.8h\n"
          "ld1 {v11.8b}, [x13]\n"
          "smlal v16.4s, v4.4h, v12.4h\n"
          "ld1 {v1.8b}, [x7]\n"
          "smlal2 v17.4s, v4.8h, v12.8h\n"
          "ld1 {v12.8b}, [x14], x6\n"
          "smlal v16.4s, v5.4h, v13.4h\n"
          "ld1 {v2.8b}, [x9], x6\n"
          "smlal2 v17.4s, v5.8h, v13.8h\n"
          "ld1 {v13.8b}, [x14]\n"

          "sqrdmulh v16.4s, v16.4s, v27.4s\n"
          "ld1 {v3.8b}, [x9]\n"
          "sqrdmulh v17.4s, v17.4s, v27.4s\n"
          "ld1 {v4.8b}, [x10], x6\n"
          "sqrshl v16.4s, v16.4s, v29.4s\n"
          "ld1 {v5.8b}, [x10]\n"
          "sqrshl v17.4s, v17.4s, v29.4s\n"
          "sqxtn v16.4h, v16.4s\n"
          "sqxtn2 v16.8h, v17.4s\n"
          "sqadd v16.8h, v16.8h, v28.8h\n"
          "sqxtun v16.8b, v16.8h\n"
          "umax v16.8b, v16.8b, v30.8b\n"
          "umin v16.8b, v16.8b, v31.8b\n"
          "uaddw v8.8h, v26.8h, v8.8b\n"
          "st1 {v16.8b}, [%[output_ptr]], #8\n"
          "uaddw v9.8h, v26.8h, v9.8b\n"
          "uaddw v10.8h, v26.8h, v10.8b\n"
          "uaddw v11.8h, v26.8h, v11.8b\n"
          "uaddw v12.8h, v26.8h, v12.8b\n"
          "uaddw v13.8h, v26.8h, v13.8b\n"

          "uaddw v0.8h, v25.8h, v0.8b\n"
          "uaddw v1.8h, v25.8h, v1.8b\n"
          "uaddw v2.8h, v25.8h, v2.8b\n"
          "ld1 {v16.4s}, [%[bias_ptr]], #16\n"
          "uaddw v3.8h, v25.8h, v3.8b\n"
          "ld1 {v17.4s}, [%[bias_ptr]], #16\n"
          "uaddw v4.8h, v25.8h, v4.8b\n"
          "uaddw v5.8h, v25.8h, v5.8b\n"

          "bge " DEPTHWISECONV_LABEL_DEPTH_8_LOOP "b\n"

        DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP ":\n"
        "smlal v16.4s, v0.4h, v8.4h\n"
        "smlal2 v17.4s, v0.8h, v8.8h\n"
        "smlal v16.4s, v1.4h, v9.4h\n"
        "smlal2 v17.4s, v1.8h, v9.8h\n"
        "smlal v16.4s, v2.4h, v10.4h\n"
        "smlal2 v17.4s, v2.8h, v10.8h\n"
        "smlal v16.4s, v3.4h, v11.4h\n"
        "smlal2 v17.4s, v3.8h, v11.8h\n"
        "smlal v16.4s, v4.4h, v12.4h\n"
        "smlal2 v17.4s, v4.8h, v12.8h\n"
        "smlal v16.4s, v5.4h, v13.4h\n"
        "smlal2 v17.4s, v5.8h, v13.8h\n"

        "sqrdmulh v16.4s, v16.4s, v27.4s\n"
        "sqrdmulh v17.4s, v17.4s, v27.4s\n"
        "sqrshl v16.4s, v16.4s, v29.4s\n"
        "sqrshl v17.4s, v17.4s, v29.4s\n"
        "sqxtn v16.4h, v16.4s\n"
        "sqxtn2 v16.8h, v17.4s\n"
        "sqadd v16.8h, v16.8h, v28.8h\n"
        "sqxtun v16.8b, v16.8h\n"
        // TODO(b/129852264): Improve testing coverage.
        "umax v16.8b, v16.8b, v30.8b\n"
        "umin v16.8b, v16.8b, v31.8b\n"
        "st1 {v16.8b}, [%[output_ptr]]\n"
        :
        // Outputs.
        [filter_ptr] "+r"(filter_ptr), [input_ptr] "+r"(input_ptr),
        [output_ptr] "+r"(output_ptr), [bias_ptr] "+r"(bias_ptr)
        :
        // Inputs.
        [params_ptr] "r"(params_ptr)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v4", "v5", "v8", "v9", "v10", "v11", "v12",
        "v13", "v16", "v17", "v18", "v19", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31",
        // We use these general-purpose registers.
        "x5", "x6", "x7", "x9", "x10", "x11", "x12", "x13", "x14", "x15");
#undef DEPTHWISECONV_LABEL_DEPTH_8_LOOP
#undef DEPTHWISECONV_LABEL_DEPTH_8_AFTER_LOOP
  }
};

#undef OFFSET_INPUT_DEPTH
#undef OFFSET_INPUT_ROW_SIZE
#undef OFFSET_OUTPUT_DEPTH
#undef OFFSET_OUTPUT_ROW_SIZE
#undef OFFSET_INPUT_OFFSET
#undef OFFSET_OUTPUT_OFFSET
#undef OFFSET_FILTER_OFFSET
#undef OFFSET_OUTPUT_MULTIPLIER
#undef OFFSET_OUTPUT_ACTIVATION_MIN
#undef OFFSET_OUTPUT_ACTIVATION_MAX
#undef OFFSET_OUTPUT_RIGHT_SHIFT
#undef OFFSET_INPUT_WIDTH
#undef OFFSET_INPUT_HEIGHT
#undef OFFSET_OUTPUT_WIDTH
#undef OFFSET_OUTPUT_HEIGHT

template <DepthwiseConvOutputRounding output_rounding, int32 kStrideWidth,
          int32 kStrideHeight>
struct DepthwiseConvThroughDepth {
  // Runs the DepthwiseConvWindow kernels through the depth dimension from
  // |start_depth| to |end_depth|. Keep this not inlined to maintain a small
  // binary size. We use a DepthwiseConvParams struct for read only params
  // to minimize call overhead.
  static void __attribute__((noinline))
  Run(const uint8* input_ptr, const uint8* filter_ptr, const int32* bias_ptr,
      uint8* output_ptr, int64_t start_depth, int64_t end_depth,
      int64_t input_depth, int64_t input_row_size, int32 output_window_height,
      int32 output_window_width, const DepthwiseConvParams& params) {
    for (; start_depth <= end_depth - 8; start_depth += 8) {
      DepthwiseConvWindow<output_rounding, 8, kStrideWidth, kStrideHeight>::Run(
          input_ptr, filter_ptr, bias_ptr, output_ptr, input_depth,
          input_row_size, output_window_height, output_window_width, &params);
      input_ptr += 8;
      output_ptr += 8;
      filter_ptr += 8;
      bias_ptr += 8;
    }
  }
};

template <DepthwiseConvOutputRounding output_rounding, int32 kStrideWidth,
          int32 kStrideHeight>
struct DepthwiseConvMultiRow {
  using ConvKernel =
      DepthwiseConvThroughDepth<output_rounding, kStrideWidth, kStrideHeight>;

  static inline void Run(const uint8* input_data, int32 start_x, int32 end_x,
                         const uint8* filter_data, const int32* bias_data,
                         uint8* output_data, const DepthwiseConvParams& params,
                         const ShuffleParams& shuffle_params,
                         uint8* shuffle_workspace) {
    TFLITE_DCHECK(
        shuffle_params.input_height ==
        get_shuffle_input_size(kStrideHeight, shuffle_params.output_height));
    TFLITE_DCHECK(
        shuffle_params.input_width ==
        get_shuffle_input_size(kStrideWidth, shuffle_params.output_width));
    TFLITE_DCHECK_LE(
        64 * shuffle_params.input_width * shuffle_params.input_height,
        kDepthwiseConvScratchWorkspaceSize);

    int32 out_x = start_x;

    // Run shuffling on inputs with sufficiently large depth and width. When
    // these parameters are large enough, more time is taken to load inputs
    // from memory. At this point, it becomes useful to prefetch and
    // preshuffle the input data to maximize locality.
    if (params.output_depth > 64 ||
        (params.output_depth <= 64 && params.input_width > 150)) {
      for (; out_x <= (end_x - shuffle_params.output_width);
           out_x += shuffle_params.output_width) {
        const uint8* input_ptr = input_data;
        const int32* bias_ptr = bias_data;
        const uint8* filter_ptr = filter_data;
        uint8* output_ptr = output_data;
        int64_t depth = 0;
        const int64_t shuffle_row_size = 64 * shuffle_params.input_width;

        for (; depth <= params.output_depth - 64; depth += 64) {
          // Preload.
          const uint8* h_ptr = input_ptr;
          for (int32 i = 0; i < shuffle_params.input_height; i++) {
            const uint8* ptr = h_ptr;
            for (int32 j = 0; j < shuffle_params.input_width; j++) {
              asm volatile("prfm pldl1keep, [%[ptr]]\n" ::[ptr] "r"(ptr) :);
              ptr += params.input_depth;
            }
            h_ptr += params.input_row_size;
          }

          // For a large enough input, shuffle into buckets.
          ShuffleInput(input_ptr, params.input_depth, params.input_width,
                       params.input_height, 64, shuffle_params.input_width,
                       shuffle_params.input_height, shuffle_workspace);
          ConvKernel::Run(shuffle_workspace, filter_ptr, bias_ptr, output_ptr,
                          0, 64, 64, shuffle_row_size,
                          shuffle_params.output_height,
                          shuffle_params.output_width, params);
          input_ptr += 64;
          output_ptr += 64;
          filter_ptr += 64;
          bias_ptr += 64;
        }

        // Preload.
        const uint8* h_ptr = input_ptr;
        for (int32 i = 0; i < shuffle_params.input_height; i++) {
          const uint8* ptr = h_ptr;
          for (int32 j = 0; j < shuffle_params.input_width; j++) {
            asm volatile("prfm pldl1keep, [%[ptr]]\n" ::[ptr] "r"(ptr) :);
            ptr += params.input_depth;
          }
          h_ptr += params.input_row_size;
        }

        // Handle leftover depth.
        ConvKernel::Run(input_ptr, filter_ptr, bias_ptr, output_ptr, depth,
                        params.output_depth, params.input_depth,
                        params.input_row_size, shuffle_params.output_height,
                        shuffle_params.output_width, params);

        input_data +=
            shuffle_params.output_width * kStrideWidth * params.input_depth;
        output_data += shuffle_params.output_width * params.output_depth;
      }
    }

    const int32 output_leftover_width = end_x - out_x;
    if (output_leftover_width > 0) {
      ConvKernel::Run(input_data, filter_data, bias_data, output_data, 0,
                      params.output_depth, params.input_depth,
                      params.input_row_size, shuffle_params.output_height,
                      output_leftover_width, params);
    }
  }
};

// Processes the borders of the input for pad_width and pad_height = 1.
// Calls 4 asm kernels:
//   * 1x1 input shape.
//   * Corner edges.
//   * Horizontal edges.
//   * Vertical edges.
template <DepthwiseConvOutputRounding output_rounding>
inline void DepthwiseConvHandlePadding(const uint8* input_data,
                                       const uint8* filter_data,
                                       const int32* bias_data,
                                       uint8* output_data,
                                       const DepthwiseConvParams& params) {
  if (params.input_width == 1 && params.input_height == 1) {
    const uint8* filter_ptr =
        filter_data + params.filter_row_size + params.output_depth;
    DepthwiseConvPartial<output_rounding, EdgeType::kCenter, 1, 1>::Run(
        input_data, filter_ptr, bias_data, output_data, &params);
    return;
  }

  const int32 out_x_start_corner = 0;
  const int32 out_x_end_corner = params.output_width - 1;
  const int32 out_y_start_corner = 0;
  const int32 out_y_end_corner = params.output_height - 1;

  // Handle top row.
  const uint8* input_ptr = input_data;
  const uint8* filter_ptr =
      filter_data + params.filter_row_size + params.output_depth;
  uint8* output_ptr = output_data;

  DepthwiseConvPartial<output_rounding, EdgeType::kCorner, 1, 1>::Run(
      input_ptr, filter_ptr, bias_data, output_ptr, &params);

  input_ptr += (params.stride_width - 1) * params.input_depth;
  filter_ptr = filter_data + params.filter_row_size;
  output_ptr += params.output_depth;

  for (int32 out_x = out_x_start_corner + 1; out_x < out_x_end_corner;
       out_x++) {
    DepthwiseConvPartial<output_rounding, EdgeType::kHorizontal, 1, 1>::Run(
        input_ptr, filter_ptr, bias_data, output_ptr, &params);
    input_ptr += params.stride_width * params.input_depth;
    output_ptr += params.output_depth;
  }

  DepthwiseConvPartial<output_rounding, EdgeType::kCorner, 1, 1>::Run(
      input_ptr, filter_ptr, bias_data, output_ptr, &params);

  // Handle left side.
  input_ptr = input_data + (params.stride_width - 1) * params.input_row_size;
  filter_ptr = filter_data + params.input_depth;
  output_ptr = output_data + params.output_row_size;

  for (int32 out_y = out_y_start_corner + 1; out_y < out_y_end_corner;
       out_y++) {
    DepthwiseConvPartial<output_rounding, EdgeType::kVertical, 1, 1>::Run(
        input_ptr, filter_ptr, bias_data, output_ptr, &params);
    input_ptr += params.stride_width * params.input_row_size;
    output_ptr += params.output_row_size;
  }

  // Handle right side.
  input_ptr = input_data + (params.input_width - 2) * params.input_depth +
              (params.stride_width - 1) * params.input_row_size;
  filter_ptr = filter_data;
  output_ptr = output_data + params.output_row_size +
               (params.output_width - 1) * params.output_depth;

  for (int32 out_y = out_y_start_corner + 1; out_y < out_y_end_corner;
       out_y++) {
    DepthwiseConvPartial<output_rounding, EdgeType::kVertical, 1, 1>::Run(
        input_ptr, filter_ptr, bias_data, output_ptr, &params);
    input_ptr += params.stride_width * params.input_row_size;
    output_ptr += params.output_row_size;
  }

  // Handle bottom row.
  input_ptr = input_data + (params.input_height - 2) * params.input_row_size;
  filter_ptr = filter_data + params.output_depth;
  output_ptr =
      output_data + (params.output_height - 1) * params.output_row_size;

  DepthwiseConvPartial<output_rounding, EdgeType::kCorner, 1, 1>::Run(
      input_ptr, filter_ptr, bias_data, output_ptr, &params);

  input_ptr += (params.stride_width == 1) ? 0 : params.input_depth;
  filter_ptr = filter_data;
  output_ptr += params.output_depth;

  for (int32 out_x = out_x_start_corner + 1; out_x < out_x_end_corner;
       out_x++) {
    DepthwiseConvPartial<output_rounding, EdgeType::kHorizontal, 1, 1>::Run(
        input_ptr, filter_ptr, bias_data, output_ptr, &params);
    input_ptr += params.stride_width * params.input_depth;
    output_ptr += params.output_depth;
  }

  DepthwiseConvPartial<output_rounding, EdgeType::kCorner, 1, 1>::Run(
      input_ptr, filter_ptr, bias_data, output_ptr, &params);
}

template <DepthwiseConvOutputRounding output_rounding>
inline void DepthwiseConv3x3Filter(
    const DepthwiseParams& rt_params, const RuntimeShape& input_shape,
    const uint8* input_data, const RuntimeShape& filter_shape,
    const uint8* filter_data, const RuntimeShape& bias_shape,
    const int32* bias_data, const RuntimeShape& output_shape,
    uint8* output_data, int thread_start, int thread_end, int thread_dim) {
  DepthwiseConvParams params;

  const int32 stride_width = rt_params.stride_width;
  const int32 stride_height = rt_params.stride_height;
  const int32 pad_width = rt_params.padding_values.width;
  const int32 pad_height = rt_params.padding_values.height;
  const int32 depth_multiplier = rt_params.depth_multiplier;
  const int32 output_activation_min = rt_params.quantized_activation_min;
  const int32 output_activation_max = rt_params.quantized_activation_max;
  const int32 input_offset = rt_params.input_offset;
  const int32 filter_offset = rt_params.weights_offset;
  const int32 output_offset = rt_params.output_offset;
  const int32 output_multiplier = rt_params.output_multiplier;
  const int32 output_shift = rt_params.output_shift;

  params.input_depth = input_shape.Dims(3);
  params.input_width = input_shape.Dims(2);
  params.input_height = input_shape.Dims(1);
  params.input_row_size = params.input_depth * params.input_width;
  params.input_offset = input_offset;
  params.stride_width = stride_width;
  params.stride_height = stride_height;
  params.output_depth = MatchingDim(filter_shape, 3, output_shape, 3);
  params.output_width = output_shape.Dims(2);
  params.output_height = output_shape.Dims(1);
  params.output_row_size = params.output_depth * params.output_width;
  params.output_offset = output_offset;
  params.filter_offset = filter_offset;
  params.output_multiplier = output_multiplier;
  params.output_right_shift = output_shift;
  params.output_activation_min = output_activation_min;
  params.output_activation_max = output_activation_max;

  const int32 filter_height = filter_shape.Dims(1);
  const int32 filter_width = filter_shape.Dims(2);
  params.filter_row_size = params.output_depth * filter_width;

  // Algorithm assumes below constraints. It is optimized for depth
  // multiplier of 1, 3x3 filter, no padding and strides 1 and 2.
  TFLITE_DCHECK(params.output_depth == params.input_depth * depth_multiplier);
  TFLITE_DCHECK(depth_multiplier == 1);
  TFLITE_DCHECK(filter_height == 3);
  TFLITE_DCHECK(filter_width == 3);
  TFLITE_DCHECK(stride_height == 1 || stride_height == 2);
  TFLITE_DCHECK(stride_width == 1 || stride_width == 2);
  TFLITE_DCHECK(stride_width == stride_height);
  TFLITE_DCHECK(pad_height == 0 || pad_height == 1);
  TFLITE_DCHECK(pad_width == 0 || pad_width == 1);
  TFLITE_DCHECK(pad_width == pad_height);
  TFLITE_DCHECK(thread_dim == 0 || thread_dim == 1);

  const int32 batches = MatchingDim(input_shape, 0, output_shape, 0);
  const int64_t input_batch_size = params.input_row_size * params.input_height;
  const int64_t output_batch_size =
      params.output_row_size * params.output_height;

  ShuffleParams one_row_shuffle_params, two_row_shuffle_params,
      four_row_shuffle_params, eight_row_shuffle_params;
  if (stride_width == 1) {
    one_row_shuffle_params = ShuffleParams(30, 1, 1, 1);
    two_row_shuffle_params = ShuffleParams(22, 2, 1, 1);
    four_row_shuffle_params = ShuffleParams(14, 4, 1, 1);
    eight_row_shuffle_params = ShuffleParams(8, 8, 1, 1);
  } else {
    one_row_shuffle_params = ShuffleParams(14, 1, 2, 2);
    two_row_shuffle_params = ShuffleParams(8, 2, 2, 2);
    four_row_shuffle_params = ShuffleParams(4, 4, 2, 2);
    eight_row_shuffle_params = ShuffleParams(2, 8, 2, 2);
  }

  using conv_multirow_func_t =
      decltype(&DepthwiseConvMultiRow<output_rounding, 1, 1>::Run);
  conv_multirow_func_t conv_multirow_func =
      DepthwiseConvMultiRow<output_rounding, 1, 1>::Run;
  if (stride_width == 2) {
    conv_multirow_func = DepthwiseConvMultiRow<output_rounding, 2, 2>::Run;
  }

  // Allocate maximum memory needed for shuffled input.
  // TODO(mariewhite): The size of this workspace is small enough to be
  // allocated on the stack. Eventually we will want to move it to the heap
  // and have it allocated outside of this function, like the im2col_array
  // used in gemmlowp.
  uint8 shuffle_workspace[kDepthwiseConvScratchWorkspaceSize];

  int batch_start = 0;
  int batch_end = batches;
  int row_start = 0;
  int row_end = params.output_height;

  switch (thread_dim) {
    case 0:
      TFLITE_DCHECK_GE(thread_start, 0);
      TFLITE_DCHECK_LE(thread_end, batches);
      batch_start = thread_start;
      batch_end = thread_end;
      break;
    case 1:
      TFLITE_DCHECK_GE(thread_start, 0);
      TFLITE_DCHECK_LE(thread_end, params.output_height);
      row_start = thread_start;
      row_end = thread_end;
      break;
  }

  for (int32 b = batch_start; b < batch_end; ++b) {
    // input_ptr and output_ptr point to the start of each batch
    const uint8* input_ptr = input_data + b * input_batch_size;
    uint8* output_ptr = output_data + b * output_batch_size;

    int32 out_x = 0;
    int32 out_y = row_start;
    int32 end_x = params.output_width;
    int32 end_y = row_end;

    if (pad_width == 1 && pad_height == 1) {
      DepthwiseConvHandlePadding<output_rounding>(
          input_ptr, filter_data, bias_data, output_ptr, params);

      // Update extents now that the edges have been handled.
      out_x = 1;
      end_x = params.output_width - 1;
      out_y = std::max(1, out_y);
      end_y = std::min(params.output_height - 1, end_y);
    }

    // pad_width and pad_height can both be 0 or 1, depending on padding option,
    // such as Padding_VALID / Padding_SAME.
    const int in_x = (out_x * stride_width) - pad_width;
    const int in_y = (out_y * stride_height) - pad_height;

    // input_ptr and output_ptr point to (in_y, in_x) and (out_y, out_x),
    // respectively. (in_y, in_x) and (out_y, out_x) change along with
    // row_start.
    input_ptr += in_y * params.input_row_size + in_x * params.input_depth;
    output_ptr += out_y * params.output_row_size + out_x * params.output_depth;

    // Shuffling shapes that maximize width over the shuffle workspace size
    // perform better since the inputs are closer together, minimizing
    // shuffling time.
    //
    // If the input shape has width large enough for the 2 row kernels,
    // we prefer to use this. The innermost loop of the kernels handle
    // 2 height x 2 width so this is the fastest path.
    //
    // If the input shape has smaller width but larger height, shuffling is
    // still useful and can benefit from kernels 4 row and 8 row kernels.

    // Handle 8 rows at a time.
    if (params.input_width < four_row_shuffle_params.input_width) {
      for (; out_y <= end_y - 8; out_y += 8) {
        conv_multirow_func(input_ptr, out_x, end_x, filter_data, bias_data,
                           output_ptr, params, eight_row_shuffle_params,
                           shuffle_workspace);
        input_ptr += 8 * stride_height * params.input_row_size;
        output_ptr += 8 * params.output_row_size;
      }
    }

    // Handle 4 rows at a time.
    if (params.input_width < two_row_shuffle_params.input_width) {
      for (; out_y <= end_y - 4; out_y += 4) {
        conv_multirow_func(input_ptr, out_x, end_x, filter_data, bias_data,
                           output_ptr, params, four_row_shuffle_params,
                           shuffle_workspace);
        input_ptr += 4 * stride_height * params.input_row_size;
        output_ptr += 4 * params.output_row_size;
      }
    }

    // Handle 2 rows at a time.
    for (; out_y <= end_y - 2; out_y += 2) {
      conv_multirow_func(input_ptr, out_x, end_x, filter_data, bias_data,
                         output_ptr, params, two_row_shuffle_params,
                         shuffle_workspace);
      input_ptr += 2 * stride_height * params.input_row_size;
      output_ptr += 2 * params.output_row_size;
    }

    // Handle one row at a time.
    for (; out_y < end_y; out_y++) {
      conv_multirow_func(input_ptr, out_x, end_x, filter_data, bias_data,
                         output_ptr, params, one_row_shuffle_params,
                         shuffle_workspace);
      input_ptr += stride_height * params.input_row_size;
      output_ptr += params.output_row_size;
    }
  }
}
#endif  // __aarch64__

#endif

// Perform any necessary cache hinting and pre-writing.
template <DepthwiseConvImplementation implementation>
struct WorkspacePrefetchWrite {
  static inline void Run(int8 fill_data, int size, int8* workspace) {}
};

#if defined(USE_NEON) && defined(__aarch64__)
// Encourage the processor to keep the workspace in cache. Both the cache hint
// and some memory writes are required.
//
// This code is extremely fragile.
// Do not edit without extensive comparative performance testing.
// Do not inline without great care.
// Do not rely on results before and after getting coffee: non-thermal changes
//    of more than 10% can occur with hidden underlying processor state changes.
template <>
struct WorkspacePrefetchWrite<
    DepthwiseConvImplementation::kUseNeon3x3DotProduct> {
  static void __attribute__((noinline))
  Run(int8 fill_data, int size, int8* workspace) {
    const int8x8_t fill_data_vec_int8 = vdup_n_s8(fill_data);
    const uint32x2_t fill_data_vec = vreinterpret_u32_s8(fill_data_vec_int8);
    for (int i = 0; i < (size - 15); i += 64) {
      int8* ptr = workspace + i;
      asm volatile("prfm pstl1keep, [%[ptr]]\n" ::[ptr] "r"(ptr) :);
      vst1_lane_u32(reinterpret_cast<uint32_t*>(ptr), fill_data_vec, 0);
    }
    vst1_lane_u32(reinterpret_cast<uint32_t*>(workspace + size - 4),
                  fill_data_vec, 0);
  }
};

#endif  // USE_NEON &&__aarch64__

#if defined(__aarch64__) && !defined(GOOGLE_L4T) && defined(__ANDROID__) && \
    defined(__clang__)
// Dot product ops hard-coded

template <>
struct ProcessPerDepth<DepthwiseConvImplementation::kUseNeon3x3DotProduct> {
  static inline void ProcessPerDepthNeon(
      const uint8* filter_data, const int32* bias_data,
      int8* shuffled_filter_data, int32* adjusted_bias_data,
      const DepthwiseConvDotProdParams* function_params) {
    // Note that argument registers may be reused after parameter loading.
    // x0 %[filter_data]
    // x1 %[bias_data]
    // x2 %[shuffled_filter_data]
    // x3 %[adjusted_bias_data]
    // x4 %[function_params]
#define DC_PER_DEPTH_1 "1"
#define DC_PER_DEPTH_2 "2"

    asm volatile(
        "ldp    w12, w11, [%[function_params], #" STR(DP_OFFSET_BIAS_INCREMENT) "]\n"
        "ldrsw  x9, [%[function_params], #" STR(DP_OFFSET_OUTPUT_DEPTH) "]\n"
        "ldr    w10, [%[function_params], #" STR(DP_OFFSET_DEPTH_MICRO_REPEATS) "]\n"
        "mov    x8, xzr\n"
        "add    w11, w11, #128\n"  // =128
        "sxtw   x12, w12\n"
        "movi   v0.16b, #128\n"
        "dup    v1.4s, w11\n"
        "lsl    x11, x12, #3\n"
        "lsl    x12, x12, #2\n"
        "movi   v2.16b, #1\n"
        // implicit-def: $q3
        // implicit-def: $q4
        // implicit-def: $q5
        // implicit-def: $q6
        // implicit-def: $q7
        // implicit-def: $q16
        // implicit-def: $q17
        // implicit-def: $q18
        // implicit-def: $q19
        "b      " DC_PER_DEPTH_2 "f\n"
        DC_PER_DEPTH_1 ":\n"  // in Loop: Header=BB177_2 Depth=1
        "add    x13, %[filter_data], x8, lsl #3\n"
        "ld1    { v19.d }[0], [x13], x9\n"
        "movi   v21.16b, #0\n"
        "movi   v20.16b, #0\n"
        "add    x8, x8, #1\n"  // =1
        "ld1    { v18.d }[0], [x13], x9\n"
        "ld1    { v17.d }[0], [x13], x9\n"
        "zip1   v22.16b, v19.16b, v18.16b\n"
        "eor    v22.16b, v22.16b, v0.16b\n"
        "ld1    { v16.d }[0], [x13], x9\n"
        "zip1   v23.16b, v17.16b, v0.16b\n"
        "eor    v23.16b, v23.16b, v0.16b\n"
        "zip1   v24.8h, v22.8h, v23.8h\n"
        "ld1    { v7.d }[0], [x13], x9\n"
        "zip2   v22.8h, v22.8h, v23.8h\n"
        ".word 0x4e8296d5  // sdot   v21.4s, v22.16b, v2.16b\n"
        ".word 0x4e829714  // sdot   v20.4s, v24.16b, v2.16b\n"
        "ld1    { v6.d }[0], [x13], x9\n"
        "zip1   v23.16b, v16.16b, v7.16b\n"
        "eor    v23.16b, v23.16b, v0.16b\n"
        "ld1    { v5.d }[0], [x13], x9\n"
        "zip1   v25.16b, v6.16b, v0.16b\n"
        "eor    v25.16b, v25.16b, v0.16b\n"
        "zip1   v26.8h, v23.8h, v25.8h\n"
        "ld1    { v4.d }[0], [x13], x9\n"
        "zip2   v23.8h, v23.8h, v25.8h\n"
        ".word 0x4e8296f5  // sdot   v21.4s, v23.16b, v2.16b\n"
        ".word 0x4e829754  // sdot   v20.4s, v26.16b, v2.16b\n"
        "ld1    { v3.d }[0], [x13]\n"
        "zip1   v25.16b, v5.16b, v4.16b\n"
        "stp    q26, q23, [%[shuffled_filter_data], #32]\n"
        "stp    q24, q22, [%[shuffled_filter_data]]\n"
        "zip1   v23.16b, v3.16b, v0.16b\n"
        "eor    v22.16b, v25.16b, v0.16b\n"
        "eor    v23.16b, v23.16b, v0.16b\n"
        "zip1   v24.8h, v22.8h, v23.8h\n"
        "zip2   v22.8h, v22.8h, v23.8h\n"
        "stp    q24, q22, [%[shuffled_filter_data], #64]\n"
        ".word 0x4e8296d5  // sdot   v21.4s, v22.16b, v2.16b\n"
        "ldr    q22, [%[bias_data]]\n"
        "ldr    q23, [%[bias_data], x12]\n"
        ".word 0x4e829714  // sdot   v20.4s, v24.16b, v2.16b\n"
        "add    %[shuffled_filter_data], x2, #96\n"  // =96
        "mla    v22.4s, v20.4s, v1.4s\n"
        "mla    v23.4s, v21.4s, v1.4s\n"
        "add    %[bias_data], x1, x11\n"
        "stp    q22, q23, [%[adjusted_bias_data]], #32\n"
        DC_PER_DEPTH_2 ":\n"  // =>This Inner Loop Header: Depth=1
        "cmp    w8, w10\n"
        "b.lt   " DC_PER_DEPTH_1 "b\n"
        :
        // Outputs.
        [ filter_data ] "+r"(filter_data),
        [ bias_data ] "+r"(bias_data),
        [ shuffled_filter_data ] "+r"(shuffled_filter_data),
        [ adjusted_bias_data ] "+r"(adjusted_bias_data)
        :
        // Inputs.
        [ function_params ] "r"(function_params)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16", "v17", "v18",
        "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27",
        // We use these general-purpose registers.
        "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x15", "x16");
  }

  static void __attribute__((noinline))
  Run(const uint8* filter_data, const int32* bias_data,
      int8* shuffled_filter_data, int32* adjusted_bias_data,
      const DepthwiseConvDotProdParams* function_params) {
    ProcessPerDepthNeon(filter_data, bias_data, shuffled_filter_data,
                        adjusted_bias_data, function_params);
  }
};

template <>
struct PackMacroBlock<DepthwiseConvImplementation::kUseNeon3x3DotProduct,
                      DepthwiseConvDepthMultiplication::kNoMultiplication,
                      /*max_padding=*/0> {
  static inline void PackMacroBlockNeon(
      const uint8* input_block_data, int8* scratch_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    TFLITE_DCHECK_EQ(function_params->padding_bottom, 0);
    TFLITE_DCHECK_EQ(function_params->padding_top, 0);
    TFLITE_DCHECK_EQ(function_params->padding_left, 0);
    TFLITE_DCHECK_EQ(function_params->padding_right, 0);
    const int workspace_height_stride =
        function_params->workspace_height_stride;
    const int width_overall_micro_repeats =
        function_params->input_width_overall_micro_repeats;
    const int input_width_micro_repeats =
        function_params->input_width_micro_repeats;
    const int depth_micro_repeats = function_params->depth_micro_repeats;
    const int block_height = function_params->inbound_block_height;
    const int residual_width = function_params->residual_width;
    const int input_height_stride = function_params->input_height_stride;
    const int input_depth = function_params->input_depth;

    TFLITE_DCHECK_GE(depth_micro_repeats, 0);
    constexpr uint8 kSignBit = 0x80;
    const int micro_block_size = 4 * 8;
    const int depth_advance = width_overall_micro_repeats * micro_block_size;
    const int width_advance =
        micro_block_size *
        (1 - depth_micro_repeats * width_overall_micro_repeats);
    const int height_advance = workspace_height_stride -
                               width_overall_micro_repeats * micro_block_size;
    const int input_depth_skip = 4 * input_depth - 8 * depth_micro_repeats;

    // Transpositions are 4x4, but doing 2 at a time is more efficient in NEON
    // code. Note the blocks of 4x4 are still interleaved down the depth.
    int8x16_t work_reg_a;
    int8x16_t work_reg_b;

    // Effect subtraction of zero-point = 128 by XOR of sign bit.
    const uint8x16_t sign_bit = vdupq_n_u8(kSignBit);

    // Work through one slice, by row, at a time.
    int8* scratch_data_0 = scratch_block_data;

    for (int k_height = 0; k_height < block_height; ++k_height) {
      const uint8* input_data_0 = input_block_data;
      int8x16_t input_data_a;
      int8x16_t input_data_b;
      int8x16_t input_data_c;
      int8x16_t input_data_d;

      // Traverse the width one point at a time, but the depth in (micro) blocks
      // of size 8.
      //
      // The depth and width margins, which are filled with "zeros", may be
      // larger than is strictly needed to calculate output. This is because the
      // conv calculation is performed across complete micro blocks.
      for (int j_width = 0; j_width < input_width_micro_repeats; ++j_width) {
        int8x16_t work_reg_a_sp;
        int8x16_t work_reg_b_sp;

        int i_depth = 0;

        if (depth_micro_repeats >= 2) {
          i_depth += 2;

          //

          input_data_a = vld1q_u8(input_data_0);
          input_data_b = vld1q_u8(input_data_0 + 1 * input_depth);
          input_data_c = vld1q_u8(input_data_0 + 2 * input_depth);
          input_data_d = vld1q_u8(input_data_0 + 3 * input_depth);
          input_data_0 += 16;

          //

          for (; i_depth < depth_micro_repeats - 1; i_depth += 2) {
            work_reg_a = vzip1q_s8(input_data_a, input_data_b);
            work_reg_b = vzip1q_s8(input_data_c, input_data_d);
            vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);
            work_reg_a = veorq_s8(work_reg_a, sign_bit);
            work_reg_b = veorq_s8(work_reg_b, sign_bit);

            work_reg_a_sp = vzip2q_s8(input_data_a, input_data_b);
            work_reg_b_sp = vzip2q_s8(input_data_c, input_data_d);
            vzipq_s8x2_in_place(&work_reg_a_sp, &work_reg_b_sp);

            input_data_a = vld1q_u8(input_data_0);
            input_data_b = vld1q_u8(input_data_0 + 1 * input_depth);
            optimized_ops_prefetch_write_l1_keep(scratch_data_0);
            optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
            vst1q_s8(scratch_data_0, work_reg_a);
            vst1q_s8(scratch_data_0 + 16, work_reg_b);

            scratch_data_0 += depth_advance;

            work_reg_a_sp = veorq_s8(work_reg_a_sp, sign_bit);
            work_reg_b_sp = veorq_s8(work_reg_b_sp, sign_bit);

            input_data_c = vld1q_u8(input_data_0 + 2 * input_depth);
            input_data_d = vld1q_u8(input_data_0 + 3 * input_depth);
            optimized_ops_prefetch_write_l1_keep(scratch_data_0);
            optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
            vst1q_s8(scratch_data_0, work_reg_a_sp);
            vst1q_s8(scratch_data_0 + 16, work_reg_b_sp);

            scratch_data_0 += depth_advance;

            //

            input_data_0 += 16;
          }

          work_reg_a = vzip1q_s8(input_data_a, input_data_b);
          work_reg_b = vzip1q_s8(input_data_c, input_data_d);
          vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);
          work_reg_a = veorq_s8(work_reg_a, sign_bit);
          work_reg_b = veorq_s8(work_reg_b, sign_bit);
          optimized_ops_prefetch_write_l1_keep(scratch_data_0);
          optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
          vst1q_s8(scratch_data_0, work_reg_a);
          vst1q_s8(scratch_data_0 + 16, work_reg_b);

          scratch_data_0 += depth_advance;
          //

          work_reg_a_sp = vzip2q_s8(input_data_a, input_data_b);
          work_reg_b_sp = vzip2q_s8(input_data_c, input_data_d);
          vzipq_s8x2_in_place(&work_reg_a_sp, &work_reg_b_sp);
          work_reg_a_sp = veorq_s8(work_reg_a_sp, sign_bit);
          work_reg_b_sp = veorq_s8(work_reg_b_sp, sign_bit);

          optimized_ops_prefetch_write_l1_keep(scratch_data_0);
          optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
          vst1q_s8(scratch_data_0, work_reg_a_sp);
          vst1q_s8(scratch_data_0 + 16, work_reg_b_sp);

          scratch_data_0 += depth_advance;
        }
        for (; i_depth < depth_micro_repeats; ++i_depth) {
          input_data_a = vld1q_lane_s8x8(input_data_0, input_data_a, 0);
          input_data_b =
              vld1q_lane_s8x8(input_data_0 + 1 * input_depth, input_data_b, 0);
          input_data_c =
              vld1q_lane_s8x8(input_data_0 + 2 * input_depth, input_data_c, 0);
          input_data_d =
              vld1q_lane_s8x8(input_data_0 + 3 * input_depth, input_data_d, 0);
          work_reg_a = vzip1q_s8(input_data_a, input_data_b);
          work_reg_b = vzip1q_s8(input_data_c, input_data_d);

          input_data_0 += 8;

          vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);
          work_reg_a = veorq_s8(work_reg_a, sign_bit);
          work_reg_b = veorq_s8(work_reg_b, sign_bit);

          optimized_ops_prefetch_write_l1_keep(scratch_data_0);
          optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
          vst1q_s8(scratch_data_0, work_reg_a);
          vst1q_s8(scratch_data_0 + 16, work_reg_b);

          scratch_data_0 += depth_advance;
        }
        scratch_data_0 += width_advance;
        input_data_0 += input_depth_skip;
      }
      if (width_overall_micro_repeats > input_width_micro_repeats) {
        TFLITE_DCHECK_EQ(width_overall_micro_repeats,
                         input_width_micro_repeats + 1);
        TFLITE_DCHECK_GT(residual_width, 0);
        TFLITE_DCHECK_LT(residual_width, 4);
        for (int i_depth = 0; i_depth < depth_micro_repeats; ++i_depth) {
          input_data_c = vdupq_n_u8(kSignBit);
          input_data_a = vld1q_lane_s8x8(input_data_0, input_data_a, 0);
          input_data_d = vdupq_n_u8(kSignBit);
          if (residual_width > 1) {
            input_data_b =
                vld1q_lane_s8x8(input_data_0 + input_depth, input_data_b, 0);
            if (residual_width == 3) {
              input_data_c = vld1q_lane_s8x8(input_data_0 + 2 * input_depth,
                                             input_data_c, 0);
            }
          }
          work_reg_a = vzip1q_s8(input_data_a, input_data_b);
          work_reg_b = vzip1q_s8(input_data_c, input_data_d);

          work_reg_a = veorq_s8(work_reg_a, sign_bit);
          work_reg_b = veorq_s8(work_reg_b, sign_bit);
          vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);

          optimized_ops_prefetch_write_l1_keep(scratch_data_0);
          optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
          vst1q_s8(scratch_data_0, work_reg_a);
          vst1q_s8(scratch_data_0 + 16, work_reg_b);

          scratch_data_0 += depth_advance;
          input_data_0 += 8;
        }
        scratch_data_0 += width_advance;
        input_data_0 += input_depth_skip;
      }
      scratch_data_0 += height_advance;
      input_block_data += input_height_stride;
    }
    TFLITE_DCHECK_EQ(
        scratch_data_0,
        scratch_block_data + block_height * workspace_height_stride);
  }

  static void __attribute__((noinline))
  Run(int32 height_block_number, int32 width_block_number,
      const uint8* input_block_data, int8* scratch_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    PreloadInputBlock<uint8>(input_block_data, function_params);
    PackMacroBlockNeon(input_block_data, scratch_block_data, function_params);
  }
};

template <>
struct PackMacroBlock<DepthwiseConvImplementation::kUseNeon3x3DotProduct,
                      DepthwiseConvDepthMultiplication::kNoMultiplication,
                      /*max_padding=*/1> {
  static inline void PackMacroBlockNeon(
      int32 height_block_number, int32 width_block_number,
      const uint8* input_block_data, int8* scratch_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    constexpr uint8 kSignBit = 0x80;

    const int workspace_height_stride =
        function_params->workspace_height_stride;
    const int width_overall_micro_repeats =
        function_params->input_width_overall_micro_repeats;
    const int input_width_micro_repeats =
        function_params->input_width_micro_repeats;
    const int depth_micro_repeats = function_params->depth_micro_repeats;
    const int block_height = function_params->inbound_block_height;
    const int residual_width = function_params->residual_width;
    const int input_height_stride = function_params->input_height_stride;
    const int input_depth = function_params->input_depth;

    const int padding_left = function_params->padding_left;
    const int padding_right = function_params->padding_right;
    const int padding_top = function_params->padding_top;
    const int padding_bottom = function_params->padding_bottom;

    TFLITE_DCHECK_GT(depth_micro_repeats, 0);
    constexpr int kSymmetricZeroPoint = 128;

    const int micro_block_size = 4 * 8;
    const int depth_advance = width_overall_micro_repeats * micro_block_size;
    const int width_advance =
        micro_block_size *
        (1 - depth_micro_repeats * width_overall_micro_repeats);
    const int height_advance = workspace_height_stride -
                               width_overall_micro_repeats * micro_block_size;
    const int input_depth_skip = 4 * input_depth - 8 * depth_micro_repeats;

    const bool leading_width_padding =
        padding_left > 0 && width_block_number == 0;
    const bool trailing_width_padding =
        padding_right > 0 &&
        width_block_number == (function_params->width_macro_count - 1);
    const bool leading_height_padding =
        padding_top > 0 && height_block_number < 0;
    const bool trailing_height_padding =
        padding_bottom > 0 &&
        height_block_number == (function_params->height_macro_count - 1);

    const int32 input_offset = function_params->input_offset;
    const int32 input_offset_difference = input_offset + kSymmetricZeroPoint;

    // Transpositions are 4x4, but doing 2 at a time is more efficient in NEON
    // code. Note the blocks of 4x4 are still interleaved down the depth.
    int8x16_t work_reg_a;
    int8x16_t work_reg_b;

    // Effect subtraction of zero-point = 128 by XOR of sign bit.
    const uint8x16_t sign_bit = vdupq_n_u8(kSignBit);

    // Work through one slice, by row, at a time.
    int8* scratch_data_0 = scratch_block_data;

    int copy_block_height = block_height;
    if (leading_height_padding) {
      copy_block_height -= 1;
      memset(scratch_data_0, -input_offset_difference, workspace_height_stride);
      scratch_data_0 += workspace_height_stride;
      input_block_data += input_height_stride;
    }
    if (trailing_height_padding) {
      copy_block_height -= 1;
    }

    for (int k_height = 0; k_height < copy_block_height; ++k_height) {
      const uint8* input_data_0 = input_block_data;
      int8x16_t input_data_a;
      int8x16_t input_data_b;
      int8x16_t input_data_c;
      int8x16_t input_data_d;

      // Traverse the width one point at a time, but the depth in (micro) blocks
      // of size 8.
      //
      // The depth and width margins, which are filled with "zeros", may be
      // larger than is strictly needed to calculate output. This is because the
      // conv calculation is performed across complete micro blocks.
      for (int j_width = 0; j_width < width_overall_micro_repeats; ++j_width) {
        // Figure out division of work (available input vs zero-ed).
        int adjusted_residual_width =
            j_width == (input_width_micro_repeats) ? residual_width : 4;

        if (trailing_width_padding &&
            j_width == (width_overall_micro_repeats - 1)) {
          adjusted_residual_width -= 1;
        }
        int start_width = 0;
        if (leading_width_padding && j_width == 0) {
          start_width = 1;
        }
        if (start_width == 0) {
          if (adjusted_residual_width == 4) {
            int8x16_t work_reg_a_sp;
            int8x16_t work_reg_b_sp;

            int i_depth = 0;

            if (depth_micro_repeats >= 2) {
              i_depth += 2;

              //

              input_data_a = vld1q_u8(input_data_0);
              input_data_b = vld1q_u8(input_data_0 + 1 * input_depth);
              input_data_c = vld1q_u8(input_data_0 + 2 * input_depth);
              input_data_d = vld1q_u8(input_data_0 + 3 * input_depth);
              input_data_0 += 16;

              //

              for (; i_depth < depth_micro_repeats - 1; i_depth += 2) {
                work_reg_a = vzip1q_s8(input_data_a, input_data_b);
                work_reg_b = vzip1q_s8(input_data_c, input_data_d);
                vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);
                work_reg_a = veorq_s8(work_reg_a, sign_bit);
                work_reg_b = veorq_s8(work_reg_b, sign_bit);

                work_reg_a_sp = vzip2q_s8(input_data_a, input_data_b);
                work_reg_b_sp = vzip2q_s8(input_data_c, input_data_d);
                vzipq_s8x2_in_place(&work_reg_a_sp, &work_reg_b_sp);

                input_data_a = vld1q_u8(input_data_0);
                input_data_b = vld1q_u8(input_data_0 + 1 * input_depth);
                optimized_ops_prefetch_write_l1_keep(scratch_data_0);
                optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
                vst1q_s8(scratch_data_0, work_reg_a);
                vst1q_s8(scratch_data_0 + 16, work_reg_b);

                scratch_data_0 += depth_advance;

                work_reg_a_sp = veorq_s8(work_reg_a_sp, sign_bit);
                work_reg_b_sp = veorq_s8(work_reg_b_sp, sign_bit);

                input_data_c = vld1q_u8(input_data_0 + 2 * input_depth);
                input_data_d = vld1q_u8(input_data_0 + 3 * input_depth);
                optimized_ops_prefetch_write_l1_keep(scratch_data_0);
                optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
                vst1q_s8(scratch_data_0, work_reg_a_sp);
                vst1q_s8(scratch_data_0 + 16, work_reg_b_sp);

                scratch_data_0 += depth_advance;

                //

                input_data_0 += 16;
              }

              work_reg_a = vzip1q_s8(input_data_a, input_data_b);
              work_reg_b = vzip1q_s8(input_data_c, input_data_d);
              vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);
              work_reg_a = veorq_s8(work_reg_a, sign_bit);
              work_reg_b = veorq_s8(work_reg_b, sign_bit);
              optimized_ops_prefetch_write_l1_keep(scratch_data_0);
              optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
              vst1q_s8(scratch_data_0, work_reg_a);
              vst1q_s8(scratch_data_0 + 16, work_reg_b);

              scratch_data_0 += depth_advance;
              //

              work_reg_a_sp = vzip2q_s8(input_data_a, input_data_b);
              work_reg_b_sp = vzip2q_s8(input_data_c, input_data_d);
              vzipq_s8x2_in_place(&work_reg_a_sp, &work_reg_b_sp);
              work_reg_a_sp = veorq_s8(work_reg_a_sp, sign_bit);
              work_reg_b_sp = veorq_s8(work_reg_b_sp, sign_bit);

              optimized_ops_prefetch_write_l1_keep(scratch_data_0);
              optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
              vst1q_s8(scratch_data_0, work_reg_a_sp);
              vst1q_s8(scratch_data_0 + 16, work_reg_b_sp);

              scratch_data_0 += depth_advance;
            }
            for (; i_depth < depth_micro_repeats; ++i_depth) {
              input_data_a = vld1q_lane_s8x8(input_data_0, input_data_a, 0);
              input_data_b = vld1q_lane_s8x8(input_data_0 + 1 * input_depth,
                                             input_data_b, 0);
              input_data_c = vld1q_lane_s8x8(input_data_0 + 2 * input_depth,
                                             input_data_c, 0);
              input_data_d = vld1q_lane_s8x8(input_data_0 + 3 * input_depth,
                                             input_data_d, 0);
              work_reg_a = vzip1q_s8(input_data_a, input_data_b);
              work_reg_b = vzip1q_s8(input_data_c, input_data_d);

              input_data_0 += 8;

              vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);
              work_reg_a = veorq_s8(work_reg_a, sign_bit);
              work_reg_b = veorq_s8(work_reg_b, sign_bit);

              optimized_ops_prefetch_write_l1_keep(scratch_data_0);
              optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
              vst1q_s8(scratch_data_0, work_reg_a);
              vst1q_s8(scratch_data_0 + 16, work_reg_b);

              scratch_data_0 += depth_advance;
            }
            scratch_data_0 += width_advance;
            input_data_0 += input_depth_skip;
          } else {
            TFLITE_DCHECK_LT(adjusted_residual_width, 4);
            for (int i_depth = 0; i_depth < depth_micro_repeats; ++i_depth) {
              input_data_a = vdupq_n_u8(-input_offset);
              input_data_b = vdupq_n_u8(-input_offset);
              input_data_c = vdupq_n_u8(-input_offset);
              input_data_d = vdupq_n_u8(-input_offset);
              if (adjusted_residual_width > 0) {
                input_data_a = vld1q_lane_s8x8(input_data_0, input_data_a, 0);
                if (adjusted_residual_width > 1) {
                  input_data_b = vld1q_lane_s8x8(input_data_0 + input_depth,
                                                 input_data_b, 0);
                  if (adjusted_residual_width == 3) {
                    input_data_c = vld1q_lane_s8x8(
                        input_data_0 + 2 * input_depth, input_data_c, 0);
                  }
                }
              }
              work_reg_a = vzip1q_s8(input_data_a, input_data_b);
              work_reg_b = vzip1q_s8(input_data_c, input_data_d);

              work_reg_a = veorq_s8(work_reg_a, sign_bit);
              work_reg_b = veorq_s8(work_reg_b, sign_bit);
              vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);

              optimized_ops_prefetch_write_l1_keep(scratch_data_0);
              optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
              vst1q_s8(scratch_data_0, work_reg_a);
              vst1q_s8(scratch_data_0 + 16, work_reg_b);

              scratch_data_0 += depth_advance;
              input_data_0 += 8;
            }
            scratch_data_0 += width_advance;
            input_data_0 += input_depth_skip;
          }
        } else {
          if (adjusted_residual_width == 4) {
            int8x16_t work_reg_a_sp;
            int8x16_t work_reg_b_sp;

            int i_depth = 0;

            if (depth_micro_repeats >= 2) {
              i_depth += 2;

              //

              input_data_a = vdupq_n_u8(-input_offset);
              input_data_b = vld1q_u8(input_data_0 + 1 * input_depth);
              input_data_c = vld1q_u8(input_data_0 + 2 * input_depth);
              input_data_d = vld1q_u8(input_data_0 + 3 * input_depth);
              input_data_0 += 16;

              //

              for (; i_depth < depth_micro_repeats - 1; i_depth += 2) {
                work_reg_a = vzip1q_s8(input_data_a, input_data_b);
                work_reg_b = vzip1q_s8(input_data_c, input_data_d);
                vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);
                work_reg_a = veorq_s8(work_reg_a, sign_bit);
                work_reg_b = veorq_s8(work_reg_b, sign_bit);

                work_reg_a_sp = vzip2q_s8(input_data_a, input_data_b);
                work_reg_b_sp = vzip2q_s8(input_data_c, input_data_d);
                vzipq_s8x2_in_place(&work_reg_a_sp, &work_reg_b_sp);

                input_data_a = vdupq_n_u8(-input_offset);
                input_data_b = vld1q_u8(input_data_0 + 1 * input_depth);
                optimized_ops_prefetch_write_l1_keep(scratch_data_0);
                optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
                vst1q_s8(scratch_data_0, work_reg_a);
                vst1q_s8(scratch_data_0 + 16, work_reg_b);

                scratch_data_0 += depth_advance;

                work_reg_a_sp = veorq_s8(work_reg_a_sp, sign_bit);
                work_reg_b_sp = veorq_s8(work_reg_b_sp, sign_bit);

                input_data_c = vld1q_u8(input_data_0 + 2 * input_depth);
                input_data_d = vld1q_u8(input_data_0 + 3 * input_depth);
                optimized_ops_prefetch_write_l1_keep(scratch_data_0);
                optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
                vst1q_s8(scratch_data_0, work_reg_a_sp);
                vst1q_s8(scratch_data_0 + 16, work_reg_b_sp);

                scratch_data_0 += depth_advance;

                //

                input_data_0 += 16;
              }

              work_reg_a = vzip1q_s8(input_data_a, input_data_b);
              work_reg_b = vzip1q_s8(input_data_c, input_data_d);
              vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);
              work_reg_a = veorq_s8(work_reg_a, sign_bit);
              work_reg_b = veorq_s8(work_reg_b, sign_bit);
              optimized_ops_prefetch_write_l1_keep(scratch_data_0);
              optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
              vst1q_s8(scratch_data_0, work_reg_a);
              vst1q_s8(scratch_data_0 + 16, work_reg_b);

              scratch_data_0 += depth_advance;
              //

              work_reg_a_sp = vzip2q_s8(input_data_a, input_data_b);
              work_reg_b_sp = vzip2q_s8(input_data_c, input_data_d);
              vzipq_s8x2_in_place(&work_reg_a_sp, &work_reg_b_sp);
              work_reg_a_sp = veorq_s8(work_reg_a_sp, sign_bit);
              work_reg_b_sp = veorq_s8(work_reg_b_sp, sign_bit);

              optimized_ops_prefetch_write_l1_keep(scratch_data_0);
              optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
              vst1q_s8(scratch_data_0, work_reg_a_sp);
              vst1q_s8(scratch_data_0 + 16, work_reg_b_sp);

              scratch_data_0 += depth_advance;
            }
            for (; i_depth < depth_micro_repeats; ++i_depth) {
              input_data_a = vdupq_n_u8(-input_offset);
              input_data_b = vld1q_lane_s8x8(input_data_0 + 1 * input_depth,
                                             input_data_b, 0);
              input_data_c = vld1q_lane_s8x8(input_data_0 + 2 * input_depth,
                                             input_data_c, 0);
              input_data_d = vld1q_lane_s8x8(input_data_0 + 3 * input_depth,
                                             input_data_d, 0);
              work_reg_a = vzip1q_s8(input_data_a, input_data_b);
              work_reg_b = vzip1q_s8(input_data_c, input_data_d);

              input_data_0 += 8;

              vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);
              work_reg_a = veorq_s8(work_reg_a, sign_bit);
              work_reg_b = veorq_s8(work_reg_b, sign_bit);

              optimized_ops_prefetch_write_l1_keep(scratch_data_0);
              optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
              vst1q_s8(scratch_data_0, work_reg_a);
              vst1q_s8(scratch_data_0 + 16, work_reg_b);

              scratch_data_0 += depth_advance;
            }
            scratch_data_0 += width_advance;
            input_data_0 += input_depth_skip;
          } else {
            TFLITE_DCHECK_LT(adjusted_residual_width, 4);

            for (int i_depth = 0; i_depth < depth_micro_repeats; ++i_depth) {
              input_data_a = vdupq_n_u8(-input_offset);
              input_data_b = vdupq_n_u8(-input_offset);
              input_data_c = vdupq_n_u8(-input_offset);
              input_data_d = vdupq_n_u8(-input_offset);
              // Skip loading first column.
              if (adjusted_residual_width > 1) {
                input_data_b = vld1q_lane_s8x8(input_data_0 + input_depth,
                                               input_data_b, 0);
                if (adjusted_residual_width == 3) {
                  input_data_c = vld1q_lane_s8x8(input_data_0 + 2 * input_depth,
                                                 input_data_c, 0);
                }
              }
              work_reg_a = vzip1q_s8(input_data_a, input_data_b);
              work_reg_b = vzip1q_s8(input_data_c, input_data_d);

              work_reg_a = veorq_s8(work_reg_a, sign_bit);
              work_reg_b = veorq_s8(work_reg_b, sign_bit);
              vzipq_s8x2_in_place(&work_reg_a, &work_reg_b);

              optimized_ops_prefetch_write_l1_keep(scratch_data_0);
              optimized_ops_prefetch_write_l1_keep(scratch_data_0 + 16);
              vst1q_s8(scratch_data_0, work_reg_a);
              vst1q_s8(scratch_data_0 + 16, work_reg_b);

              scratch_data_0 += depth_advance;
              input_data_0 += 8;
            }
            scratch_data_0 += width_advance;
            input_data_0 += input_depth_skip;
          }
        }
      }
      scratch_data_0 += height_advance;
      input_block_data += input_height_stride;
    }

    if (trailing_height_padding) {
      memset(scratch_data_0, -input_offset_difference, workspace_height_stride);
      scratch_data_0 += workspace_height_stride;
    }

    TFLITE_DCHECK_EQ(
        scratch_data_0,
        scratch_block_data + block_height * workspace_height_stride);
  }

  static void __attribute__((noinline))
  Run(int32 height_block_number, int32 width_block_number,
      const uint8* input_block_data, int8* scratch_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    PreloadInputBlock<uint8>(input_block_data, function_params);
    PackMacroBlockNeon(height_block_number, width_block_number,
                       input_block_data, scratch_block_data, function_params);
  }
};

template <>
struct PackMacroBlock<DepthwiseConvImplementation::kUseNeon3x3DotProduct,
                      DepthwiseConvDepthMultiplication::kUnitInputDepth,
                      /*max_padding=*/1> {
  static inline void PackMacroBlockNeon(
      int32 height_block_number, int32 width_block_number,
      const uint8* input_block_data, int8* scratch_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    const int workspace_height_stride =
        function_params->workspace_height_stride;
    const int width_overall_micro_repeats =
        function_params->input_width_overall_micro_repeats;
    const int input_width_micro_repeats =
        function_params->input_width_micro_repeats;
    const int block_height = function_params->inbound_block_height;
    const int residual_width = function_params->residual_width;
    const int input_height_stride = function_params->input_height_stride;

    const int padding_left = function_params->padding_left;
    const int padding_right = function_params->padding_right;
    const int padding_top = function_params->padding_top;
    const int padding_bottom = function_params->padding_bottom;

    constexpr int kSymmetricZeroPoint = 128;

    TFLITE_DCHECK_GE(workspace_height_stride, 4 * width_overall_micro_repeats);

    const bool leading_width_padding =
        padding_left > 0 && width_block_number == 0;
    const bool trailing_width_padding =
        padding_right > 0 &&
        width_block_number == (function_params->width_macro_count - 1);
    const bool leading_height_padding =
        padding_top > 0 && height_block_number < 0;
    const bool trailing_height_padding =
        padding_bottom > 0 &&
        height_block_number == (function_params->height_macro_count - 1);

    const int32 input_offset = function_params->input_offset;
    const int32 input_offset_difference = input_offset + kSymmetricZeroPoint;

    // Work through one slice, by row, at a time.
    int8* scratch_data_base = scratch_block_data;

    int copy_block_height = block_height;
    if (leading_height_padding) {
      copy_block_height -= 1;
      memset(scratch_data_base, -input_offset_difference,
             workspace_height_stride + kWorkspaceExtension);
      scratch_data_base += workspace_height_stride;
      input_block_data += input_height_stride;
    }
    if (trailing_height_padding) {
      copy_block_height -= 1;
    }

    int adjusted_residual_width =
        input_width_micro_repeats < width_overall_micro_repeats ? residual_width
                                                                : 4;

    if (trailing_width_padding) {
      adjusted_residual_width -= 1;
    }
    int start_width = 0;
    if (leading_width_padding) {
      start_width = 1;
      input_block_data += 1;
    }

    const int copy_size = (width_overall_micro_repeats - 1) * 4 +
                          adjusted_residual_width - start_width;
    // Adjusted so that later conditionals are simplified.
    const int copy_size_adjusted =
        trailing_width_padding ? copy_size + 1 : copy_size;

    TFLITE_DCHECK_LE(
        copy_size,
        input_height_stride - width_block_number * input_width_micro_repeats);
    // We may drop up to stride-1 of trailing input.
    TFLITE_DCHECK_GE(copy_size, input_height_stride - 1);

    int scratch_data_offset = 0;
    int input_block_offset = 0;

    constexpr uint8 kSignBit = 0x80;

    // Transpositions are 4x4, but doing 2 at a time is more efficient in NEON
    // code. Note the blocks of 4x4 are still interleaved down the depth.
    int8x16_t work_reg;
    int8x8_t half_work_reg;
    int8x8_t padding_mask;

    // Effect subtraction of zero-point = 128 by XOR of sign bit.
    const uint8x16_t sign_bit = vdupq_n_u8(kSignBit);
    const uint8x16_t padding_reg = vdupq_n_u8(-input_offset);
    padding_mask = vdup_n_s8(-1);
    half_work_reg = vdup_n_s8(0);

    if (copy_size >= 16) {
      const int copy_remaining = (copy_size + start_width) & 0x7;
      padding_mask = vshl_u64(padding_mask, vdup_n_s64(8 * copy_remaining));

      for (int k_height = 0; k_height < copy_block_height; ++k_height) {
        // Work through one slice, by row, at a time.
        int8* scratch_data = scratch_data_base + scratch_data_offset;

        int copy_done = 0;

        // The surrounding condition ensures that we always need at least one
        // iteration of the main copy loop. In the case of leading width
        // padding, we unroll this specially.
        if (leading_width_padding) {
          work_reg = vld1q_u8(input_block_data + input_block_offset);
          work_reg = vextq_s8(padding_reg, work_reg, 15);
          work_reg = veorq_s8(work_reg, sign_bit);
          optimized_ops_prefetch_write_l1_keep(scratch_data);
          vst1q_s8(scratch_data, work_reg);
          copy_done += 15;
        }

        // Main copy loop.
        for (; (copy_done + 16) <= copy_size; copy_done += 16) {
          work_reg =
              vld1q_u8(input_block_data + input_block_offset + copy_done);
          work_reg = veorq_s8(work_reg, sign_bit);
          TFLITE_DCHECK_EQ((start_width + copy_done) % 16, 0);
          optimized_ops_prefetch_write_l1_keep(scratch_data + start_width +
                                               copy_done);
          vst1q_s8(scratch_data + start_width + copy_done, work_reg);
        }

        if (copy_done + 8 <= copy_size) {
          half_work_reg =
              vld1_u8(input_block_data + input_block_offset + copy_done);
          half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
          TFLITE_DCHECK_EQ((start_width + copy_done) % 8, 0);
          optimized_ops_prefetch_write_l1_keep(scratch_data + start_width +
                                               copy_done);
          vst1_s8(scratch_data + start_width + copy_done, half_work_reg);
          copy_done += 8;
        }

        TFLITE_DCHECK_EQ(copy_remaining, copy_size - copy_done);
        // Total amount
        // = copy_size - copy_done + 4 - adjusted_residual_width
        // = width_overall_micro_repeats * 4 - start_width - copy_done.
        // Undone micro blocks
        // = width_overall_micro_repeats - (start_width + copy_done) / 4.

        // Conditional is (copy_remaining > 0 || trailing_width_padding).
        if (copy_done < copy_size_adjusted) {
          // Employ overlapping-load strategy in order to load full register,
          // but use only part.
          // This has the advantage of resulting in zeros after shifting.
          half_work_reg =
              vld1_u8(input_block_data + input_block_offset + copy_size - 8);

          half_work_reg =
              vshl_u64(half_work_reg, vdup_n_s64(-8 * (8 - copy_remaining)));
          half_work_reg =
              vbsl_s8(padding_mask, vget_low_s8(padding_reg), half_work_reg);

          half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
          TFLITE_DCHECK_EQ((start_width + copy_done) % 8, 0);
          optimized_ops_prefetch_write_l1_keep(scratch_data + start_width +
                                               copy_done);
          vst1_s8(scratch_data + start_width + copy_done, half_work_reg);
        }

        // Trailing guard.
        optimized_ops_prefetch_write_l1_keep(scratch_data + start_width +
                                             copy_done);
        optimized_ops_prefetch_write_l1_keep(scratch_data + start_width +
                                             copy_done + 8);
        vst1_s8(scratch_data + start_width + copy_done, half_work_reg);
        vst1_s8(scratch_data + start_width + copy_done + 8, half_work_reg);

        scratch_data_offset += workspace_height_stride;
        input_block_offset += input_height_stride;
      }
    } else if (copy_size >= 4) {
      const int copy_remaining = (copy_size + start_width) & 0x3;
      padding_mask = vshl_u64(padding_mask, vdup_n_s64(8 * copy_remaining));

      for (int k_height = 0; k_height < copy_block_height; ++k_height) {
        // Work through one slice, by row, at a time.
        int8* scratch_data = scratch_data_base + scratch_data_offset;

        int copy_done = 0;

        // The surrounding condition ensures that we always need at least one
        // iteration of the main copy loop. In the case of leading width
        // padding, we unroll this specially.
        if (leading_width_padding) {
          half_work_reg = vld1_lane_8x4(input_block_data + input_block_offset,
                                        half_work_reg, 0);
          half_work_reg = vext_s8(vget_low_s8(padding_reg), half_work_reg, 7);
          half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
          optimized_ops_prefetch_write_l1_keep(scratch_data);
          vst1_lane_8x4(scratch_data, half_work_reg, 0);
          copy_done += 3;
        }

        // Main copy loop.
        for (; (copy_done + 4) <= copy_size; copy_done += 4) {
          half_work_reg =
              vld1_lane_8x4(input_block_data + input_block_offset + copy_done,
                            half_work_reg, 0);
          half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
          TFLITE_DCHECK_EQ((start_width + copy_done) % 4, 0);
          optimized_ops_prefetch_write_l1_keep(scratch_data + start_width +
                                               copy_done);
          vst1_lane_8x4(scratch_data + start_width + copy_done, half_work_reg,
                        0);
        }

        TFLITE_DCHECK_EQ(copy_remaining, copy_size - copy_done);
        // Total amount
        // = copy_size - copy_done + 4 - adjusted_residual_width
        // = width_overall_micro_repeats * 4 - start_width - copy_done.
        // Undone micro blocks
        // = width_overall_micro_repeats - (start_width + copy_done) / 4.

        // Conditional is (copy_remaining > 0 || trailing_width_padding).
        if (copy_done < copy_size_adjusted) {
          TFLITE_DCHECK_LT(copy_remaining, 4);
          // Employ overlapping-load strategy in order to load full register,
          // but use only part.
          // This has the advantage of resulting in zeros after shifting.
          half_work_reg = vld1_lane_8x4(
              input_block_data + input_block_offset + copy_size - 4,
              half_work_reg, 0);

          half_work_reg =
              vshl_u64(half_work_reg, vdup_n_s64(-8 * (4 - copy_remaining)));
          half_work_reg =
              vbsl_s8(padding_mask, vget_low_s8(padding_reg), half_work_reg);

          half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
          TFLITE_DCHECK_EQ((start_width + copy_done) % 4, 0);
          optimized_ops_prefetch_write_l1_keep(scratch_data + start_width +
                                               copy_done);
          vst1_lane_8x4(scratch_data + start_width + copy_done, half_work_reg,
                        0);
          copy_done += 4;
        }
        // Trailing guard.
        optimized_ops_prefetch_write_l1_keep(scratch_data + start_width +
                                             copy_done);
        optimized_ops_prefetch_write_l1_keep(scratch_data + start_width +
                                             copy_done + 12);
        vst1_lane_8x4(scratch_data + start_width + copy_done, half_work_reg, 0);
        vst1_lane_8x4(scratch_data + start_width + copy_done + 4, half_work_reg,
                      0);
        vst1_lane_8x4(scratch_data + start_width + copy_done + 8, half_work_reg,
                      0);
        vst1_lane_8x4(scratch_data + start_width + copy_done + 12,
                      half_work_reg, 0);

        scratch_data_offset += workspace_height_stride;
        input_block_offset += input_height_stride;
      }
    } else if (width_overall_micro_repeats == 2) {
      // Special case of 1 + 3 + 1, padding + copy + padding.
      // This is rarely executed in practice.
      TFLITE_DCHECK_EQ(copy_size, 3);
      TFLITE_DCHECK_EQ(start_width, 1);
      TFLITE_DCHECK(leading_width_padding);
      TFLITE_DCHECK(trailing_width_padding);

      for (int k_height = 0; k_height < copy_block_height; ++k_height) {
        half_work_reg = vdup_n_u8(-input_offset);
        half_work_reg = vld1_lane_s8(reinterpret_cast<const int8*>(
                                         input_block_data + input_block_offset),
                                     half_work_reg, 1);
        half_work_reg =
            vld1_lane_s8(reinterpret_cast<const int8*>(input_block_data +
                                                       input_block_offset + 1),
                         half_work_reg, 2);
        half_work_reg =
            vld1_lane_s8(reinterpret_cast<const int8*>(input_block_data +
                                                       input_block_offset + 2),
                         half_work_reg, 3);

        half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
        TFLITE_DCHECK_EQ(scratch_data_offset % 8, 0);
        optimized_ops_prefetch_write_l1_keep(scratch_data_base +
                                             scratch_data_offset);
        vst1_s8(scratch_data_base + scratch_data_offset, half_work_reg);

        // Trailing guard.
        optimized_ops_prefetch_write_l1_keep(scratch_data_base +
                                             scratch_data_offset + 4);
        optimized_ops_prefetch_write_l1_keep(scratch_data_base +
                                             scratch_data_offset + 16);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 4,
                      half_work_reg, 0);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 8,
                      half_work_reg, 0);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 12,
                      half_work_reg, 0);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 16,
                      half_work_reg, 0);

        scratch_data_offset += workspace_height_stride;
        input_block_offset += input_height_stride;
      }
    } else {
      TFLITE_DCHECK_EQ(width_overall_micro_repeats, 1);
      const int copy_remaining = (copy_size + start_width) & 0x3;
      padding_mask = vshl_u64(padding_mask, vdup_n_s64(8 * copy_remaining));
      if (leading_width_padding) {
        padding_mask = vset_lane_u8(255, padding_mask, 0);
      }

      for (int k_height = 0; k_height < copy_block_height; ++k_height) {
        for (int i = 0; i < copy_size; ++i) {
          half_work_reg = vshl_n_u64(half_work_reg, 8);
          half_work_reg = vld1_lane_s8(
              reinterpret_cast<const int8*>(
                  input_block_data + input_block_offset + copy_size - 1 - i),
              half_work_reg, 0);
        }
        if (leading_width_padding) {
          half_work_reg = vshl_n_s64(half_work_reg, 8);
        }
        half_work_reg =
            vbsl_s8(padding_mask, vget_low_s8(padding_reg), half_work_reg);

        half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
        TFLITE_DCHECK_EQ(scratch_data_offset % 4, 0);
        optimized_ops_prefetch_write_l1_keep(scratch_data_base +
                                             scratch_data_offset);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset, half_work_reg,
                      0);

        // Trailing guard.
        optimized_ops_prefetch_write_l1_keep(scratch_data_base +
                                             scratch_data_offset + 4);
        optimized_ops_prefetch_write_l1_keep(scratch_data_base +
                                             scratch_data_offset + 16);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 4,
                      half_work_reg, 0);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 8,
                      half_work_reg, 0);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 12,
                      half_work_reg, 0);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 16,
                      half_work_reg, 0);

        scratch_data_offset += workspace_height_stride;
        input_block_offset += input_height_stride;
      }
    }

    scratch_data_base += copy_block_height * workspace_height_stride;

    if (trailing_height_padding) {
      memset(scratch_data_base, -input_offset_difference,
             workspace_height_stride + kWorkspaceExtension);
      scratch_data_base += workspace_height_stride;
    }

    TFLITE_DCHECK_EQ(
        scratch_data_base,
        scratch_block_data + block_height * workspace_height_stride);
  }

  static void __attribute__((noinline))
  Run(int32 height_block_number, int32 width_block_number,
      const uint8* input_block_data, int8* scratch_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    PreloadInputBlock<uint8>(input_block_data, function_params);
    PackMacroBlockNeon(height_block_number, width_block_number,
                       input_block_data, scratch_block_data, function_params);
  }
};

template <>
struct PackMacroBlock<DepthwiseConvImplementation::kUseNeon3x3DotProduct,
                      DepthwiseConvDepthMultiplication::kUnitInputDepth,
                      /*max_padding=*/0> {
  static inline void PackMacroBlockNeon(
      int32 height_block_number, int32 width_block_number,
      const uint8* input_block_data, int8* scratch_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    const int workspace_height_stride =
        function_params->workspace_height_stride;
    const int width_overall_micro_repeats =
        function_params->input_width_overall_micro_repeats;
    const int input_width_micro_repeats =
        function_params->input_width_micro_repeats;
    const int block_height = function_params->inbound_block_height;
    const int residual_width = function_params->residual_width;
    const int input_height_stride = function_params->input_height_stride;

    TFLITE_DCHECK_EQ(function_params->padding_left, 0);
    TFLITE_DCHECK_EQ(function_params->padding_right, 0);
    TFLITE_DCHECK_EQ(function_params->padding_top, 0);
    TFLITE_DCHECK_EQ(function_params->padding_bottom, 0);

    TFLITE_DCHECK_GE(workspace_height_stride, 4 * width_overall_micro_repeats);

    // Work through one slice, by row, at a time.
    int8* scratch_data_base = scratch_block_data;

    const int copy_block_height = block_height;

    int adjusted_residual_width =
        input_width_micro_repeats < width_overall_micro_repeats ? residual_width
                                                                : 4;

    const int copy_size =
        (width_overall_micro_repeats - 1) * 4 + adjusted_residual_width;

    TFLITE_DCHECK_LE(
        copy_size,
        input_height_stride - width_block_number * input_width_micro_repeats);
    // We may drop up to stride-1 of trailing input.
    TFLITE_DCHECK_GE(copy_size, input_height_stride - 1);

    int scratch_data_offset = 0;
    int input_block_offset = 0;

    constexpr uint8 kSignBit = 0x80;

    // Transpositions are 4x4, but doing 2 at a time is more efficient in NEON
    // code. Note the blocks of 4x4 are still interleaved down the depth.
    int8x16_t work_reg;
    int8x8_t half_work_reg;

    // Effect subtraction of zero-point = 128 by XOR of sign bit.
    const uint8x16_t sign_bit = vdupq_n_u8(kSignBit);
    half_work_reg = vdup_n_s8(0);

    if (copy_size >= 16) {
      const int copy_remaining = copy_size & 0x7;

      for (int k_height = 0; k_height < copy_block_height; ++k_height) {
        // Work through one slice, by row, at a time.
        int8* scratch_data = scratch_data_base + scratch_data_offset;

        int copy_done = 0;

        // Main copy loop.
        for (; (copy_done + 16) <= copy_size; copy_done += 16) {
          work_reg =
              vld1q_u8(input_block_data + input_block_offset + copy_done);
          work_reg = veorq_s8(work_reg, sign_bit);
          TFLITE_DCHECK_EQ(copy_done % 16, 0);
          optimized_ops_prefetch_write_l1_keep(scratch_data + copy_done);
          vst1q_s8(scratch_data + copy_done, work_reg);
        }

        if (copy_done + 8 <= copy_size) {
          half_work_reg =
              vld1_u8(input_block_data + input_block_offset + copy_done);
          half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
          TFLITE_DCHECK_EQ(copy_done % 8, 0);
          optimized_ops_prefetch_write_l1_keep(scratch_data + copy_done);
          vst1_s8(scratch_data + copy_done, half_work_reg);
          copy_done += 8;
        }

        TFLITE_DCHECK_EQ(copy_remaining, copy_size - copy_done);
        // Total amount
        // = copy_size - copy_done + 4 - adjusted_residual_width
        // = width_overall_micro_repeats * 4 - start_width - copy_done.
        // Undone micro blocks
        // = width_overall_micro_repeats - (start_width + copy_done) / 4.

        // Conditional is (copy_remaining > 0 || trailing_width_padding).
        if (copy_done < copy_size) {
          // Employ overlapping-load strategy in order to load full register,
          // but use only part.
          // This has the advantage of resulting in zeros after shifting.
          half_work_reg =
              vld1_u8(input_block_data + input_block_offset + copy_size - 8);

          half_work_reg =
              vshl_u64(half_work_reg, vdup_n_s64(-8 * (8 - copy_remaining)));

          half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
          TFLITE_DCHECK_EQ(copy_done % 8, 0);
          optimized_ops_prefetch_write_l1_keep(scratch_data + copy_done);
          vst1_s8(scratch_data + copy_done, half_work_reg);
          copy_done += 8;
        }

        // Trailing guard.
        optimized_ops_prefetch_write_l1_keep(scratch_data + copy_done);
        optimized_ops_prefetch_write_l1_keep(scratch_data + copy_done + 8);
        vst1_s8(scratch_data + copy_done, half_work_reg);
        vst1_s8(scratch_data + copy_done + 8, half_work_reg);

        scratch_data_offset += workspace_height_stride;
        input_block_offset += input_height_stride;
      }
    } else if (copy_size >= 4) {
      const int copy_remaining = copy_size & 0x3;

      for (int k_height = 0; k_height < copy_block_height; ++k_height) {
        // Work through one slice, by row, at a time.
        int8* scratch_data = scratch_data_base + scratch_data_offset;

        int copy_done = 0;

        // Main copy loop.
        for (; (copy_done + 4) <= copy_size; copy_done += 4) {
          half_work_reg =
              vld1_lane_8x4(input_block_data + input_block_offset + copy_done,
                            half_work_reg, 0);
          half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
          TFLITE_DCHECK_EQ(copy_done % 4, 0);
          optimized_ops_prefetch_write_l1_keep(scratch_data + copy_done);
          vst1_lane_8x4(scratch_data + copy_done, half_work_reg, 0);
        }

        TFLITE_DCHECK_EQ(copy_remaining, copy_size - copy_done);
        // Total amount
        // = copy_size - copy_done + 4 - adjusted_residual_width
        // = width_overall_micro_repeats * 4 - start_width - copy_done.
        // Undone micro blocks
        // = width_overall_micro_repeats - (start_width + copy_done) / 4.

        // Conditional is (copy_remaining > 0 || trailing_width_padding).
        if (copy_done < copy_size) {
          TFLITE_DCHECK_LT(copy_remaining, 4);
          // Employ overlapping-load strategy in order to load full register,
          // but use only part.
          // This has the advantage of resulting in zeros after shifting.
          half_work_reg = vld1_lane_8x4(
              input_block_data + input_block_offset + copy_size - 4,
              half_work_reg, 0);

          half_work_reg =
              vshl_u64(half_work_reg, vdup_n_s64(-8 * (4 - copy_remaining)));

          half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
          TFLITE_DCHECK_EQ(copy_done % 4, 0);
          optimized_ops_prefetch_write_l1_keep(scratch_data + copy_done);
          vst1_lane_8x4(scratch_data + copy_done, half_work_reg, 0);
          copy_done += 4;
        }
        // Trailing guard.
        optimized_ops_prefetch_write_l1_keep(scratch_data + copy_done);
        optimized_ops_prefetch_write_l1_keep(scratch_data + copy_done + 12);
        vst1_lane_8x4(scratch_data + copy_done, half_work_reg, 0);
        vst1_lane_8x4(scratch_data + copy_done + 4, half_work_reg, 0);
        vst1_lane_8x4(scratch_data + copy_done + 8, half_work_reg, 0);
        vst1_lane_8x4(scratch_data + copy_done + 12, half_work_reg, 0);

        scratch_data_offset += workspace_height_stride;
        input_block_offset += input_height_stride;
      }
    } else {
      TFLITE_DCHECK_EQ(width_overall_micro_repeats, 1);

      for (int k_height = 0; k_height < copy_block_height; ++k_height) {
        for (int i = 0; i < copy_size; ++i) {
          half_work_reg = vshl_n_u64(half_work_reg, 8);
          half_work_reg = vld1_lane_s8(
              reinterpret_cast<const int8*>(
                  input_block_data + input_block_offset + copy_size - 1 - i),
              half_work_reg, 0);
        }

        half_work_reg = veor_s8(half_work_reg, vget_low_s8(sign_bit));
        TFLITE_DCHECK_EQ(scratch_data_offset % 4, 0);
        optimized_ops_prefetch_write_l1_keep(scratch_data_base +
                                             scratch_data_offset);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset, half_work_reg,
                      0);

        // Trailing guard.
        optimized_ops_prefetch_write_l1_keep(scratch_data_base +
                                             scratch_data_offset + 8);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 4,
                      half_work_reg, 0);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 8,
                      half_work_reg, 0);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 12,
                      half_work_reg, 0);
        vst1_lane_8x4(scratch_data_base + scratch_data_offset + 16,
                      half_work_reg, 0);

        scratch_data_offset += workspace_height_stride;
        input_block_offset += input_height_stride;
      }
    }

    scratch_data_base += copy_block_height * workspace_height_stride;

    TFLITE_DCHECK_EQ(
        scratch_data_base,
        scratch_block_data + block_height * workspace_height_stride);
  }

  static void __attribute__((noinline))
  Run(int32 height_block_number, int32 width_block_number,
      const uint8* input_block_data, int8* scratch_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    PreloadInputBlock<uint8>(input_block_data, function_params);
    PackMacroBlockNeon(height_block_number, width_block_number,
                       input_block_data, scratch_block_data, function_params);
  }
};

template <>
struct KernelMacroBlock<DepthwiseConvImplementation::kUseNeon3x3DotProduct,
                        DepthwiseConvDepthMultiplication::kNoMultiplication,
                        /*stride=*/1> {
  static inline void KernelMacroBlockNeon(
      const int8* scratch_block_data, const int8* filter_workspace,
      const int32* bias_data, uint8* output_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    // Note that argument registers may be reused after parameter loading.
    // x0 %[scratch_block_data]
    // x1 %[filter_workspace]
    // x2 %[bias_data]
    // x3 %[output_block_data]
    // x4 %[function_params]
#define DC_KERNEL_NO_MULT_1 "1"
#define DC_KERNEL_NO_MULT_2 "2"
#define DC_KERNEL_NO_MULT_3 "3"
#define DC_KERNEL_NO_MULT_4 "4"
#define DC_KERNEL_NO_MULT_5 "5"
#define DC_KERNEL_NO_MULT_6 "6"
#define DC_KERNEL_NO_MULT_7 "7"
#define DC_KERNEL_NO_MULT_8 "8"
#define DC_KERNEL_NO_MULT_9 "9"
#define DC_KERNEL_NO_MULT_10 "10"
#define DC_KERNEL_NO_MULT_11 "11"
#define DC_KERNEL_NO_MULT_12 "12"
#define DC_KERNEL_NO_MULT_13 "13"
#define DC_KERNEL_NO_MULT_14 "14"
#define DC_KERNEL_NO_MULT_15 "15"
#define DC_KERNEL_NO_MULT_16 "16"
#define DC_KERNEL_NO_MULT_17 "17"
#define DC_KERNEL_NO_MULT_18 "18"
#define DC_KERNEL_NO_MULT_19 "19"
#define DC_KERNEL_NO_MULT_20 "20"
#define DC_KERNEL_NO_MULT_21 "21"
#define DC_KERNEL_NO_MULT_22 "22"
#define DC_KERNEL_NO_MULT_23 "23"
#define DC_KERNEL_NO_MULT_24 "24"
#define DC_KERNEL_NO_MULT_25 "25"
#define DC_KERNEL_NO_MULT_26 "26"

    asm volatile(
        "sub    sp, sp, #288\n"  // =448
        "stp    xzr, %[bias_data], [sp, #56]\n"  // 8-byte Folded Spill
        "str    %[filter_workspace], [sp, #32]\n"  // 8-byte Folded Spill
        "str    xzr, [sp, #48]\n"  // 8-byte Folded Spill
        "ldp    w9, w14, [%[function_params], #" STR(DP_OFFSET_OUTPUT_WIDTH_OVERALL_MICRO_REPEATS) "]\n"
        "ldpsw  x12, %[filter_workspace], [%[function_params], #" STR(DP_OFFSET_OUTPUT_HEIGHT_STRIDE) "]\n"
        "ldrsw  x8, [%[function_params], #" STR(DP_OFFSET_INPUT_WIDTH_OVERALL_MICRO_REPEATS) "]\n"
        "ldrsw  x16, [%[function_params]]\n"
        "str    w9, [sp, #252]\n"  // 4-byte Folded Spill
        "ldr    w9, [%[function_params], #" STR(DP_OFFSET_DEPTH_MICRO_REPEATS) "]\n"
        "ldrb   w10, [%[function_params], #" STR(DP_OFFSET_QUANTIZED_ACTIVATION_MAX) "]\n"
        "lsl    x8, x8, #5\n"
        "add    x11, %[function_params], #" STR(DP_OFFSET_OUTPUT_MULTIPLIER) "\n"  // =32
        "str    w9, [sp, #24]\n"  // 4-byte Folded Spill
        "ldr    w9, [%[function_params], #" STR(DP_OFFSET_OUTBOUND_BLOCK_HEIGHT) "]\n"
        "add    x15, %[function_params], #" STR(DP_OFFSET_OUTPUT_SHIFT) "\n"  // =36
        "str    x8, [sp, #16]\n"  // 8-byte Folded Spill
        "add    x8, x12, x12, lsl #1\n"
        "str    w9, [sp, #132]\n"  // 4-byte Folded Spill
        "ldrb   w9, [%[function_params], #" STR(DP_OFFSET_QUANTIZED_ACTIVATION_MIN) "]\n"
        "ldr    w18, [%[function_params], #" STR(DP_OFFSET_OUTPUT_RESIDUAL_WIDTH) "]\n"
        "add    x13, %[function_params], #" STR(DP_OFFSET_OUTPUT_OFFSET) "\n"  // =28
        "ld1r   { v1.4s }, [x11]\n"
        "dup    v3.16b, w9\n"
        "dup    v5.8b, w9\n"
        "add    x9, x16, x16, lsl #1\n"
        "ld1r   { v2.4s }, [x15]\n"
        "add    x11, %[filter_workspace], x1, lsl #2\n"
        "add    x15, x12, x16, lsl #1\n"
        "add    x25, x9, x8\n"
        "add    %[bias_data], x9, x12, lsl #1\n"
        "add    %[function_params], x9, x12\n"
        "add    x9, %[output_block_data], x9\n"
        "ld1r   { v0.8h }, [x13]\n"
        "str    q3, [sp, #272]\n"  // 16-byte Folded Spill
        "dup    v3.16b, w10\n"
        "dup    v6.8b, w10\n"
        "lsl    x10, x16, #1\n"
        "stp    x9, x11, [sp, #104]\n"  // 8-byte Folded Spill
        "add    x9, x8, x16, lsl #1\n"
        "add    x7, %[output_block_data], x15\n"
        "add    x15, x8, x16\n"
        "add    x21, %[output_block_data], x8\n"
        "add    x8, %[scratch_block_data], x11\n"
        "add    x10, x10, x12, lsl #1\n"
        "add    x30, %[output_block_data], x15\n"
        "add    x15, x8, #32\n"  // =32
        "add    x8, %[filter_workspace], x1, lsl #1\n"
        "add    x24, %[scratch_block_data], %[filter_workspace]\n"
        "add    x23, %[scratch_block_data], %[filter_workspace], lsl #1\n"
        "add    x13, %[scratch_block_data], %[filter_workspace], lsl #2\n"
        "add    x17, x16, x12, lsl #1\n"
        "add    x5, x12, x16\n"
        "add    x29, %[output_block_data], x10\n"
        "str    x8, [sp, #96]\n"  // 8-byte Folded Spill
        "add    x8, %[scratch_block_data], x8\n"
        "lsl    x10, %[filter_workspace], #1\n"
        "mov    w6, wzr\n"
        "add    x19, %[output_block_data], x17\n"
        "add    x20, %[output_block_data], x5\n"
        "add    x22, x13, #32\n"  // =32
        "add    x23, x23, #32\n"  // =32
        "add    x24, x24, #32\n"  // =32
        "add    x25, %[output_block_data], x25\n"
        "add    x26, %[output_block_data], %[bias_data]\n"
        "add    x27, %[output_block_data], %[function_params]\n"
        "add    x28, %[output_block_data], x9\n"
        "add    x8, x8, #32\n"  // =32
        "add    x9, %[scratch_block_data], #32\n"  // =32
        "stp    x10, %[filter_workspace], [sp, #176]\n"  // 8-byte Folded Spill
        "lsl    x10, %[filter_workspace], #2\n"
        "lsl    %[bias_data], x16, #2\n"
        "add    %[filter_workspace], %[output_block_data], x16, lsl #1\n"
        "add    %[function_params], %[output_block_data], x16\n"
        "add    x5, %[output_block_data], x12, lsl #1\n"
        "add    x11, %[output_block_data], x12\n"
        "str    q3, [sp, #256]\n"  // 16-byte Folded Spill
        "stp    %[scratch_block_data], %[output_block_data], [sp, #136]\n"  // 8-byte Folded Spill
        "str    x10, [sp, #88]\n"  // 8-byte Folded Spill
        "str    x12, [sp, #120]\n"  // 8-byte Folded Spill
        "str    %[output_block_data], [sp, #40]\n"  // 8-byte Folded Spill
        "stp    d6, d5, [sp, #72]\n"  // 8-byte Folded Spill
        "b      " DC_KERNEL_NO_MULT_26 "f\n"
        DC_KERNEL_NO_MULT_1 ":\n"  // in Loop: Header=BB225_26 Depth=1
        "ldr    x10, [sp, #32]\n"  // 8-byte Folded Reload
        "str    w6, [sp, #28]\n"  // 4-byte Folded Spill
        "ldr    w12, [sp, #132]\n"  // 4-byte Folded Reload
        "ldp    q18, q7, [x10]\n"
        "ldp    q19, q16, [x10, #32]\n"
        "ldp    q20, q17, [x10, #64]\n"
        "cmp    w12, #4\n"  // =4
        "add    x10, x10, #96\n"  // =96
        "str    x10, [sp, #32]\n"  // 8-byte Folded Spill
        "b.ne   " DC_KERNEL_NO_MULT_14 "f\n"
        // %bb.2:        // in Loop: Header=BB225_26 Depth=1
        "ldp    %[scratch_block_data], x13, [sp, #48]\n"  // 8-byte Folded Reload
        "ldr    x6, [sp, #64]\n"  // 8-byte Folded Reload
        "mov    x12, xzr\n"
        "b      " DC_KERNEL_NO_MULT_13 "f\n"
        DC_KERNEL_NO_MULT_3 ":\n"  // in Loop: Header=BB225_13 Depth=2
        "ldr    x10, [sp, #136]\n"  // 8-byte Folded Reload
        "str    x12, [sp, #168]\n"  // 8-byte Folded Spill
        "ldr    q21, [x6]\n"
        "shl    v3.4s, v18.4s, #8\n"
        "add    x10, x10, x12, lsl #4\n"
        "ldr    x12, [sp, #184]\n"  // 8-byte Folded Reload
        "ldr    q14, [x10]\n"
        "str    q3, [sp, #224]\n"  // 16-byte Folded Spill
        "shl    v3.4s, v19.4s, #8\n"
        "ldr    q23, [x10, x12]\n"
        "ldr    x12, [sp, #176]\n"  // 8-byte Folded Reload
        "mov    v31.16b, v21.16b\n"
        "mov    v8.16b, v21.16b\n"
        "mov    v9.16b, v21.16b\n"
        "ldr    q24, [x10, x12]\n"
        "ldp    x12, %[output_block_data], [sp, #96]\n"  // 8-byte Folded Reload
        "mov    v10.16b, v21.16b\n"
        "mov    w17, wzr\n"
        "str    q3, [sp, #208]\n"  // 16-byte Folded Spill
        "ldr    q25, [x10, x12]\n"
        "ldr    x12, [sp, #88]\n"  // 8-byte Folded Reload
        "shl    v3.4s, v20.4s, #8\n"
        ".word 0x4e98969f  // sdot   v31.4s, v20.16b, v24.16b\n"
        ".word 0x4e989668  // sdot   v8.4s, v19.16b, v24.16b\n"
        "ldr    q26, [x10, x12]\n"
        "ldr    x12, [sp, #112]\n"  // 8-byte Folded Reload
        ".word 0x4e989649  // sdot   v9.4s, v18.16b, v24.16b\n"
        ".word 0x4e99964a  // sdot   v10.4s, v18.16b, v25.16b\n"
        "str    q3, [sp, #192]\n"  // 16-byte Folded Spill
        "ldr    q27, [x10, x12]\n"
        "stp    %[scratch_block_data], x13, [sp, #152]\n"  // 8-byte Folded Spill
        "mov    x12, x13\n"
        "ldr    x13, [sp, #144]\n"  // 8-byte Folded Reload
        "b      " DC_KERNEL_NO_MULT_5 "f\n"
        DC_KERNEL_NO_MULT_4 ":\n"  // in Loop: Header=BB225_5 Depth=3
        ".word 0x4e8e965f  // sdot   v31.4s, v18.16b, v14.16b\n"
        ".word 0x4e979648  // sdot   v8.4s, v18.16b, v23.16b\n"
        ".word 0x4e999669  // sdot   v9.4s, v19.16b, v25.16b\n"
        ".word 0x4e97967f  // sdot   v31.4s, v19.16b, v23.16b\n"
        ".word 0x4e9a966a  // sdot   v10.4s, v19.16b, v26.16b\n"
        ".word 0x4e999688  // sdot   v8.4s, v20.16b, v25.16b\n"
        ".word 0x4e9a9689  // sdot   v9.4s, v20.16b, v26.16b\n"
        "sqrdmulh        v31.4s, v31.4s, v1.4s\n"
        ".word 0x4e9b968a  // sdot   v10.4s, v20.16b, v27.16b\n"
        "sqrdmulh        v8.4s, v8.4s, v1.4s\n"
        "sqrdmulh        v9.4s, v9.4s, v1.4s\n"
        "sqrshl v31.4s, v31.4s, v2.4s\n"
        "sqrdmulh        v10.4s, v10.4s, v1.4s\n"
        "sqrshl v8.4s, v8.4s, v2.4s\n"
        "sqrshl v9.4s, v9.4s, v2.4s\n"
        "sqxtn  v31.4h, v31.4s\n"
        "sqrshl v10.4s, v10.4s, v2.4s\n"
        "sqxtn  v9.4h, v9.4s\n"
        "sqxtn2 v31.8h, v8.4s\n"
        "sqxtn2 v9.8h, v10.4s\n"
        "sqadd  v31.8h, v31.8h, v0.8h\n"
        "sqadd  v8.8h, v9.8h, v0.8h\n"
        "sqxtun v31.8b, v31.8h\n"
        "sqxtun2        v31.16b, v8.8h\n"
        "ldp    q28, q6, [sp, #256]\n"  // 16-byte Folded Reload
        "add    x10, x11, %[scratch_block_data]\n"
        "ldr    q5, [sp, #192]\n"  // 16-byte Folded Reload
        "mov    v8.16b, v21.16b\n"
        "umax   v31.16b, v31.16b, v6.16b\n"
        "umin   v31.16b, v31.16b, v28.16b\n"
        "str    s31, [x13, %[scratch_block_data]]\n"
        "st1    { v31.s }[1], [x10]\n"
        "add    x10, x5, %[scratch_block_data]\n"
        "st1    { v31.s }[2], [x10]\n"
        "add    x10, x21, %[scratch_block_data]\n"
        "st1    { v31.s }[3], [x10]\n"
        "ldp    q30, q29, [sp, #208]\n"  // 16-byte Folded Reload
        "mov    v9.16b, v21.16b\n"
        "mov    v10.16b, v21.16b\n"
        "mov    v11.16b, v21.16b\n"
        ".word 0x4e8e97a8  // sdot   v8.4s, v29.16b, v14.16b\n"
        ".word 0x4e9797a9  // sdot   v9.4s, v29.16b, v23.16b\n"
        ".word 0x4e9897aa  // sdot   v10.4s, v29.16b, v24.16b\n"
        ".word 0x4e9797c8  // sdot   v8.4s, v30.16b, v23.16b\n"
        ".word 0x4e9997ab  // sdot   v11.4s, v29.16b, v25.16b\n"
        ".word 0x4e9897c9  // sdot   v9.4s, v30.16b, v24.16b\n"
        ".word 0x4e9997ca  // sdot   v10.4s, v30.16b, v25.16b\n"
        ".word 0x4e9894a8  // sdot   v8.4s, v5.16b, v24.16b\n"
        ".word 0x4e9a97cb  // sdot   v11.4s, v30.16b, v26.16b\n"
        ".word 0x4e9994a9  // sdot   v9.4s, v5.16b, v25.16b\n"
        ".word 0x4e9a94aa  // sdot   v10.4s, v5.16b, v26.16b\n"
        "sqrdmulh        v22.4s, v8.4s, v1.4s\n"
        "rev32  v12.8h, v23.8h\n"
        "rev32  v13.8h, v24.8h\n"
        ".word 0x4e9b94ab  // sdot   v11.4s, v5.16b, v27.16b\n"
        "sqrdmulh        v23.4s, v9.4s, v1.4s\n"
        "sqrdmulh        v24.4s, v10.4s, v1.4s\n"
        "sqrshl v22.4s, v22.4s, v2.4s\n"
        "rev32  v4.8h, v25.8h\n"
        "sqrdmulh        v25.4s, v11.4s, v1.4s\n"
        "sqrshl v8.4s, v23.4s, v2.4s\n"
        "sqrshl v23.4s, v24.4s, v2.4s\n"
        "sqxtn  v10.4h, v22.4s\n"
        "rev32  v15.8h, v26.8h\n"
        "rev32  v3.8h, v27.8h\n"
        "sqrshl v9.4s, v25.4s, v2.4s\n"
        "sqxtn  v11.4h, v23.4s\n"
        "ldr    q22, [x9, x12]\n"
        "ldr    q23, [x24, x12]\n"
        "ldr    q24, [x23, x12]\n"
        "ldr    q25, [x8, x12]\n"
        "ldr    q26, [x22, x12]\n"
        "ldr    q27, [x15, x12]\n"
        "sqxtn2 v10.8h, v8.4s\n"
        "sqxtn2 v11.8h, v9.4s\n"
        "sqadd  v8.8h, v10.8h, v0.8h\n"
        "sqadd  v9.8h, v11.8h, v0.8h\n"
        "sqxtun v8.8b, v8.8h\n"
        "sqxtun2        v8.16b, v9.8h\n"
        "umax   v8.16b, v8.16b, v6.16b\n"
        "add    x10, x20, %[scratch_block_data]\n"
        "rev32  v31.8h, v14.8h\n"
        "umin   v8.16b, v8.16b, v28.16b\n"
        "str    s8, [%[function_params], %[scratch_block_data]]\n"
        "st1    { v8.s }[1], [x10]\n"
        "add    x10, x19, %[scratch_block_data]\n"
        "mov    v9.16b, v21.16b\n"
        "trn1   v31.8h, v31.8h, v22.8h\n"
        "st1    { v8.s }[2], [x10]\n"
        "add    x10, x30, %[scratch_block_data]\n"
        "mov    v10.16b, v21.16b\n"
        "mov    v11.16b, v21.16b\n"
        "trn1   v12.8h, v12.8h, v23.8h\n"
        "trn1   v13.8h, v13.8h, v24.8h\n"
        ".word 0x4e9f9649  // sdot   v9.4s, v18.16b, v31.16b\n"
        "st1    { v8.s }[3], [x10]\n"
        "mov    v8.16b, v21.16b\n"
        "trn1   v14.8h, v4.8h, v25.8h\n"
        ".word 0x4e8c964a  // sdot   v10.4s, v18.16b, v12.16b\n"
        ".word 0x4e8d964b  // sdot   v11.4s, v18.16b, v13.16b\n"
        ".word 0x4e8c9669  // sdot   v9.4s, v19.16b, v12.16b\n"
        "trn1   v15.8h, v15.8h, v26.8h\n"
        ".word 0x4e8e9648  // sdot   v8.4s, v18.16b, v14.16b\n"
        ".word 0x4e8d966a  // sdot   v10.4s, v19.16b, v13.16b\n"
        ".word 0x4e8e966b  // sdot   v11.4s, v19.16b, v14.16b\n"
        ".word 0x4e8d9689  // sdot   v9.4s, v20.16b, v13.16b\n"
        "trn1   v3.8h, v3.8h, v27.8h\n"
        ".word 0x4e8f9668  // sdot   v8.4s, v19.16b, v15.16b\n"
        ".word 0x4e8e968a  // sdot   v10.4s, v20.16b, v14.16b\n"
        ".word 0x4e8f968b  // sdot   v11.4s, v20.16b, v15.16b\n"
        "sqrdmulh        v9.4s, v9.4s, v1.4s\n"
        ".word 0x4e839688  // sdot   v8.4s, v20.16b, v3.16b\n"
        "sqrdmulh        v10.4s, v10.4s, v1.4s\n"
        "sqrdmulh        v11.4s, v11.4s, v1.4s\n"
        "sqrshl v9.4s, v9.4s, v2.4s\n"
        "sqrdmulh        v8.4s, v8.4s, v1.4s\n"
        "sqrshl v10.4s, v10.4s, v2.4s\n"
        "sqrshl v11.4s, v11.4s, v2.4s\n"
        "sqxtn  v9.4h, v9.4s\n"
        "sqrshl v8.4s, v8.4s, v2.4s\n"
        "sqxtn  v11.4h, v11.4s\n"
        "sqxtn2 v9.8h, v10.4s\n"
        "sqxtn2 v11.8h, v8.4s\n"
        "sqadd  v8.8h, v9.8h, v0.8h\n"
        "sqadd  v9.8h, v11.8h, v0.8h\n"
        "sqxtun v8.8b, v8.8h\n"
        "sqxtun2        v8.16b, v9.8h\n"
        "umax   v8.16b, v8.16b, v6.16b\n"
        "add    x10, x7, %[scratch_block_data]\n"
        "umin   v8.16b, v8.16b, v28.16b\n"
        "str    s8, [%[filter_workspace], %[scratch_block_data]]\n"
        "st1    { v8.s }[1], [x10]\n"
        "add    x10, x29, %[scratch_block_data]\n"
        "st1    { v8.s }[2], [x10]\n"
        "add    x10, x28, %[scratch_block_data]\n"
        "mov    v9.16b, v21.16b\n"
        "mov    v10.16b, v21.16b\n"
        "mov    v11.16b, v21.16b\n"
        "st1    { v8.s }[3], [x10]\n"
        "mov    v8.16b, v21.16b\n"
        ".word 0x4e9f97a9  // sdot   v9.4s, v29.16b, v31.16b\n"
        ".word 0x4e8c97aa  // sdot   v10.4s, v29.16b, v12.16b\n"
        ".word 0x4e8d97ab  // sdot   v11.4s, v29.16b, v13.16b\n"
        ".word 0x4e8e97a8  // sdot   v8.4s, v29.16b, v14.16b\n"
        ".word 0x4e8c97c9  // sdot   v9.4s, v30.16b, v12.16b\n"
        ".word 0x4e8d97ca  // sdot   v10.4s, v30.16b, v13.16b\n"
        ".word 0x4e8e97cb  // sdot   v11.4s, v30.16b, v14.16b\n"
        ".word 0x4e8f97c8  // sdot   v8.4s, v30.16b, v15.16b\n"
        ".word 0x4e8d94a9  // sdot   v9.4s, v5.16b, v13.16b\n"
        ".word 0x4e8e94aa  // sdot   v10.4s, v5.16b, v14.16b\n"
        ".word 0x4e8f94ab  // sdot   v11.4s, v5.16b, v15.16b\n"
        ".word 0x4e8394a8  // sdot   v8.4s, v5.16b, v3.16b\n"
        "sqrdmulh        v3.4s, v9.4s, v1.4s\n"
        "sqrdmulh        v31.4s, v10.4s, v1.4s\n"
        "sqrdmulh        v9.4s, v11.4s, v1.4s\n"
        "sqrshl v3.4s, v3.4s, v2.4s\n"
        "sqrdmulh        v8.4s, v8.4s, v1.4s\n"
        "sqrshl v31.4s, v31.4s, v2.4s\n"
        "sqrshl v9.4s, v9.4s, v2.4s\n"
        "sqxtn  v3.4h, v3.4s\n"
        "sqrshl v8.4s, v8.4s, v2.4s\n"
        "sqxtn  v9.4h, v9.4s\n"
        "sqxtn2 v3.8h, v31.4s\n"
        "sqxtn2 v9.8h, v8.4s\n"
        "sqadd  v3.8h, v3.8h, v0.8h\n"
        "sqadd  v31.8h, v9.8h, v0.8h\n"
        "sqxtun v3.8b, v3.8h\n"
        "sqxtun2        v3.16b, v31.8h\n"
        "umax   v3.16b, v3.16b, v6.16b\n"
        "add    x10, x27, %[scratch_block_data]\n"
        "umin   v3.16b, v3.16b, v28.16b\n"
        "str    s3, [%[output_block_data], %[scratch_block_data]]\n"
        "st1    { v3.s }[1], [x10]\n"
        "add    x10, x26, %[scratch_block_data]\n"
        "st1    { v3.s }[2], [x10]\n"
        "add    x10, x25, %[scratch_block_data]\n"
        "mov    v31.16b, v21.16b\n"
        "mov    v8.16b, v21.16b\n"
        "mov    v9.16b, v21.16b\n"
        "mov    v10.16b, v21.16b\n"
        "add    w17, w17, #1\n"  // =1
        ".word 0x4e98969f  // sdot   v31.4s, v20.16b, v24.16b\n"
        ".word 0x4e989668  // sdot   v8.4s, v19.16b, v24.16b\n"
        ".word 0x4e989649  // sdot   v9.4s, v18.16b, v24.16b\n"
        ".word 0x4e99964a  // sdot   v10.4s, v18.16b, v25.16b\n"
        "st1    { v3.s }[3], [x10]\n"
        "add    %[scratch_block_data], x0, %[bias_data]\n"
        "add    x12, x12, #32\n"  // =32
        "mov    v14.16b, v22.16b\n"
        DC_KERNEL_NO_MULT_5 ":\n"  // Parent Loop BB225_26 Depth=1
        // Parent Loop BB225_13 Depth=2
        // =>  This Inner Loop Header: Depth=3
        "cmp    w17, w14\n"
        "b.lt   " DC_KERNEL_NO_MULT_4 "b\n"
        // %bb.6:        // in Loop: Header=BB225_13 Depth=2
        "ldp    d6, d5, [sp, #72]\n"  // 8-byte Folded Reload
        "cmp    w18, #0\n"  // =0
        "add    x6, x6, #16\n"  // =16
        "str    x6, [sp, #224]\n"  // 8-byte Folded Spill
        "b.le   " DC_KERNEL_NO_MULT_12 "f\n"
        // %bb.7:        // in Loop: Header=BB225_13 Depth=2
        "movi   v28.16b, #0\n"
        "cmp    w18, #3\n"  // =3
        "movi   v29.16b, #0\n"
        "movi   v30.16b, #0\n"
        "movi   v11.16b, #0\n"
        "movi   v12.16b, #0\n"
        "movi   v13.16b, #0\n"
        "b.lt   " DC_KERNEL_NO_MULT_9 "f\n"
        // %bb.8:        // in Loop: Header=BB225_13 Depth=2
        "ldr    q28, [x9, x12]\n"
        "ldr    q29, [x24, x12]\n"
        "ldr    q30, [x23, x12]\n"
        "ldr    q11, [x8, x12]\n"
        "ldr    q12, [x22, x12]\n"
        "ldr    q13, [x15, x12]\n"
        DC_KERNEL_NO_MULT_9 ":\n"  // in Loop: Header=BB225_13 Depth=2
        "ldr    x6, [sp, #144]\n"  // 8-byte Folded Reload
        "mov    x12, xzr\n"
        "mov    w17, wzr\n"
        "add    x10, x21, %[scratch_block_data]\n"
        "add    x13, x5, %[scratch_block_data]\n"
        "add    %[output_block_data], x11, %[scratch_block_data]\n"
        "add    %[scratch_block_data], x6, x0\n"
        "b      " DC_KERNEL_NO_MULT_11 "f\n"
        DC_KERNEL_NO_MULT_10 ":\n"  // in Loop: Header=BB225_11 Depth=3
        ".word 0x4e8e965f  // sdot   v31.4s, v18.16b, v14.16b\n"
        ".word 0x4e979648  // sdot   v8.4s, v18.16b, v23.16b\n"
        ".word 0x4e999669  // sdot   v9.4s, v19.16b, v25.16b\n"
        ".word 0x4e97967f  // sdot   v31.4s, v19.16b, v23.16b\n"
        ".word 0x4e9a966a  // sdot   v10.4s, v19.16b, v26.16b\n"
        ".word 0x4e999688  // sdot   v8.4s, v20.16b, v25.16b\n"
        ".word 0x4e9a9689  // sdot   v9.4s, v20.16b, v26.16b\n"
        "sqrdmulh        v3.4s, v31.4s, v1.4s\n"
        ".word 0x4e9b968a  // sdot   v10.4s, v20.16b, v27.16b\n"
        "sqrdmulh        v31.4s, v8.4s, v1.4s\n"
        "sqrdmulh        v8.4s, v9.4s, v1.4s\n"
        "sqrshl v3.4s, v3.4s, v2.4s\n"
        "sqrdmulh        v9.4s, v10.4s, v1.4s\n"
        "sqrshl v31.4s, v31.4s, v2.4s\n"
        "sqrshl v8.4s, v8.4s, v2.4s\n"
        "sqxtn  v3.4h, v3.4s\n"
        "sqrshl v9.4s, v9.4s, v2.4s\n"
        "sqxtn  v8.4h, v8.4s\n"
        "sqxtn2 v3.8h, v31.4s\n"
        "sqxtn2 v8.8h, v9.4s\n"
        "sqadd  v3.8h, v3.8h, v0.8h\n"
        "sqadd  v31.8h, v8.8h, v0.8h\n"
        "sqxtun v3.8b, v3.8h\n"
        "sqxtun2        v3.16b, v31.8h\n"
        "ldr    q4, [sp, #272]\n"  // 16-byte Folded Reload
        "add    x6, %[output_block_data], x12\n"
        "ushr   v24.4s, v24.4s, #8\n"
        "ushr   v25.4s, v25.4s, #8\n"
        "umax   v3.16b, v3.16b, v4.16b\n"
        "ldr    q4, [sp, #256]\n"  // 16-byte Folded Reload
        "ushr   v14.4s, v14.4s, #8\n"
        "ushr   v23.4s, v23.4s, #8\n"
        "sli    v24.4s, v30.4s, #24\n"
        "umin   v3.16b, v3.16b, v4.16b\n"
        "str    s3, [%[scratch_block_data], x12]\n"
        "st1    { v3.s }[1], [x6]\n"
        "add    x6, x13, x12\n"
        "st1    { v3.s }[2], [x6]\n"
        "add    x6, x10, x12\n"
        "ushr   v26.4s, v26.4s, #8\n"
        "ushr   v27.4s, v27.4s, #8\n"
        "sli    v25.4s, v11.4s, #24\n"
        "mov    v31.16b, v21.16b\n"
        "mov    v8.16b, v21.16b\n"
        "mov    v9.16b, v21.16b\n"
        "mov    v10.16b, v21.16b\n"
        "add    w17, w17, #1\n"  // =1
        "sli    v14.4s, v28.4s, #24\n"
        "ushr   v28.4s, v28.4s, #8\n"
        "ushr   v30.4s, v30.4s, #8\n"
        "sli    v23.4s, v29.4s, #24\n"
        "ushr   v29.4s, v29.4s, #8\n"
        "ushr   v11.4s, v11.4s, #8\n"
        "sli    v26.4s, v12.4s, #24\n"
        "ushr   v12.4s, v12.4s, #8\n"
        "sli    v27.4s, v13.4s, #24\n"
        "ushr   v13.4s, v13.4s, #8\n"
        "st1    { v3.s }[3], [x6]\n"
        ".word 0x4e98969f  // sdot   v31.4s, v20.16b, v24.16b\n"
        ".word 0x4e989668  // sdot   v8.4s, v19.16b, v24.16b\n"
        ".word 0x4e989649  // sdot   v9.4s, v18.16b, v24.16b\n"
        ".word 0x4e99964a  // sdot   v10.4s, v18.16b, v25.16b\n"
        "add    x12, x12, x16\n"
        DC_KERNEL_NO_MULT_11 ":\n"  // Parent Loop BB225_26 Depth=1
        // Parent Loop BB225_13 Depth=2
        // =>  This Inner Loop Header: Depth=3
        "cmp    w17, w18\n"
        "b.lt   " DC_KERNEL_NO_MULT_10 "b\n"
        DC_KERNEL_NO_MULT_12 ":\n"  // in Loop: Header=BB225_13 Depth=2
        "ldp    x13, x12, [sp, #160]\n"  // 8-byte Folded Reload
        "ldr    %[scratch_block_data], [sp, #152]\n"  // 8-byte Folded Reload
        "ldr    x6, [sp, #224]\n"  // 8-byte Folded Reload
        "mov    v20.16b, v17.16b\n"
        "add    x12, x12, #1\n"  // =1
        "add    %[scratch_block_data], x0, #4\n"  // =4
        "add    x13, x13, #16\n"  // =16
        "mov    v19.16b, v16.16b\n"
        "mov    v18.16b, v7.16b\n"
        DC_KERNEL_NO_MULT_13 ":\n"  // Parent Loop BB225_26 Depth=1
        // =>  This Loop Header: Depth=2
        // Child Loop BB225_5 Depth 3
        // Child Loop BB225_11 Depth 3
        "cmp    x12, #2\n"  // =2
        "b.ne   " DC_KERNEL_NO_MULT_3 "b\n"
        "b      " DC_KERNEL_NO_MULT_25 "f\n"
        DC_KERNEL_NO_MULT_14 ":\n"  // in Loop: Header=BB225_26 Depth=1
        "ldr    x10, [sp, #64]\n"  // 8-byte Folded Reload
        "ldr    x17, [sp, #40]\n"  // 8-byte Folded Reload
        "ldr    %[output_block_data], [sp, #136]\n"  // 8-byte Folded Reload
        "mov    w12, wzr\n"
        "ldp    q21, q22, [x10]\n"
        "b      " DC_KERNEL_NO_MULT_24 "f\n"
        DC_KERNEL_NO_MULT_15 ":\n"  // in Loop: Header=BB225_24 Depth=2
        "ldr    x10, [sp, #184]\n"  // 8-byte Folded Reload
        "str    w12, [sp, #224]\n"  // 4-byte Folded Spill
        "ldp    q23, q24, [%[output_block_data]]\n"
        "mov    w12, wzr\n"
        "add    x13, %[output_block_data], x10\n"
        "ldr    x10, [sp, #176]\n"  // 8-byte Folded Reload
        "ldp    q25, q26, [x13]\n"
        "str    x13, [sp, #192]\n"  // 8-byte Folded Spill
        "add    x10, %[output_block_data], x10\n"
        "ldp    q27, q28, [x10]\n"
        "str    x17, [sp, #208]\n"  // 8-byte Folded Spill
        "b      " DC_KERNEL_NO_MULT_22 "f\n"
        DC_KERNEL_NO_MULT_16 ":\n"  // in Loop: Header=BB225_22 Depth=3
        "cmp    w12, w14\n"
        "orr    w13, wzr, #0x4\n"
        "csel   w13, w18, w13, eq\n"
        "add    x10, %[output_block_data], #32\n"  // =32
        "movi   v29.16b, #0\n"
        "movi   v30.16b, #0\n"
        "movi   v8.16b, #0\n"
        "movi   v31.16b, #0\n"
        "cmp    w13, #3\n"  // =3
        "movi   v9.16b, #0\n"
        "movi   v10.16b, #0\n"
        "b.lt   " DC_KERNEL_NO_MULT_18 "f\n"
        // %bb.17:        // in Loop: Header=BB225_22 Depth=3
        "ldr    %[scratch_block_data], [sp, #184]\n"  // 8-byte Folded Reload
        "ldp    q29, q31, [%[output_block_data], #32]\n"
        "add    x6, x10, %[scratch_block_data]\n"
        "ldr    %[scratch_block_data], [sp, #176]\n"  // 8-byte Folded Reload
        "ldp    q30, q9, [x6]\n"
        "add    %[scratch_block_data], x10, x0\n"
        "ldp    q8, q10, [%[scratch_block_data]]\n"
        DC_KERNEL_NO_MULT_18 ":\n"  // in Loop: Header=BB225_22 Depth=3
        "mov    w3, wzr\n"
        "b      " DC_KERNEL_NO_MULT_20 "f\n"
        DC_KERNEL_NO_MULT_19 ":\n"  // in Loop: Header=BB225_20 Depth=4
        "mov    v3.16b, v21.16b\n"
        "mov    v11.16b, v22.16b\n"
        ".word 0x4e979643  // sdot   v3.4s, v18.16b, v23.16b\n"
        ".word 0x4e9894eb  // sdot   v11.4s, v7.16b, v24.16b\n"
        ".word 0x4e999663  // sdot   v3.4s, v19.16b, v25.16b\n"
        ".word 0x4e9a960b  // sdot   v11.4s, v16.16b, v26.16b\n"
        ".word 0x4e9b9683  // sdot   v3.4s, v20.16b, v27.16b\n"
        ".word 0x4e9c962b  // sdot   v11.4s, v17.16b, v28.16b\n"
        "sqrdmulh        v3.4s, v3.4s, v1.4s\n"
        "sqrdmulh        v11.4s, v11.4s, v1.4s\n"
        "sqrshl v3.4s, v3.4s, v2.4s\n"
        "sqrshl v11.4s, v11.4s, v2.4s\n"
        "sqxtn  v3.4h, v3.4s\n"
        "sqxtn2 v3.8h, v11.4s\n"
        "sqadd  v3.8h, v3.8h, v0.8h\n"
        "sqxtun v3.8b, v3.8h\n"
        "umax   v3.8b, v3.8b, v5.8b\n"
        "ushr   v23.4s, v23.4s, #8\n"
        "ushr   v24.4s, v24.4s, #8\n"
        "ushr   v25.4s, v25.4s, #8\n"
        "ushr   v26.4s, v26.4s, #8\n"
        "ushr   v27.4s, v27.4s, #8\n"
        "ushr   v28.4s, v28.4s, #8\n"
        "umin   v3.8b, v3.8b, v6.8b\n"
        "sli    v23.4s, v29.4s, #24\n"
        "ushr   v29.4s, v29.4s, #8\n"
        "sli    v24.4s, v31.4s, #24\n"
        "ushr   v31.4s, v31.4s, #8\n"
        "sli    v25.4s, v30.4s, #24\n"
        "ushr   v30.4s, v30.4s, #8\n"
        "sli    v26.4s, v9.4s, #24\n"
        "ushr   v9.4s, v9.4s, #8\n"
        "sli    v27.4s, v8.4s, #24\n"
        "ushr   v8.4s, v8.4s, #8\n"
        "sli    v28.4s, v10.4s, #24\n"
        "ushr   v10.4s, v10.4s, #8\n"
        "str    d3, [x17]\n"
        "add    x17, x17, x16\n"
        "add    w3, w3, #1\n"  // =1
        DC_KERNEL_NO_MULT_20 ":\n"  // Parent Loop BB225_26 Depth=1
        // Parent Loop BB225_24 Depth=2
        // Parent Loop BB225_22 Depth=3
        // =>  This Inner Loop Header: Depth=4
        "cmp    w3, w13\n"
        "b.lt   " DC_KERNEL_NO_MULT_19 "b\n"
        // %bb.21:        // in Loop: Header=BB225_22 Depth=3
        "add    w12, w12, #1\n"  // =1
        "mov    %[output_block_data], x10\n"
        DC_KERNEL_NO_MULT_22 ":\n"  // Parent Loop BB225_26 Depth=1
        // Parent Loop BB225_24 Depth=2
        // =>  This Loop Header: Depth=3
        // Child Loop BB225_20 Depth 4
        "ldr    w10, [sp, #252]\n"  // 4-byte Folded Reload
        "cmp    w12, w10\n"
        "b.lt   " DC_KERNEL_NO_MULT_16 "b\n"
        // %bb.23:        // in Loop: Header=BB225_24 Depth=2
        "ldr    x10, [sp, #120]\n"  // 8-byte Folded Reload
        "ldr    x17, [sp, #208]\n"  // 8-byte Folded Reload
        "ldr    w12, [sp, #224]\n"  // 4-byte Folded Reload
        "ldr    %[output_block_data], [sp, #192]\n"  // 8-byte Folded Reload
        "add    x17, x17, x10\n"
        "add    w12, w12, #1\n"  // =1
        DC_KERNEL_NO_MULT_24 ":\n"  // Parent Loop BB225_26 Depth=1
        // =>  This Loop Header: Depth=2
        // Child Loop BB225_22 Depth 3
        // Child Loop BB225_20 Depth 4
        "ldr    w10, [sp, #132]\n"  // 4-byte Folded Reload
        "cmp    w12, w10\n"
        "b.lt   " DC_KERNEL_NO_MULT_15 "b\n"
        DC_KERNEL_NO_MULT_25 ":\n"  // in Loop: Header=BB225_26 Depth=1
        "ldr    x10, [sp, #64]\n"  // 8-byte Folded Reload
        "ldr    x12, [sp, #16]\n"  // 8-byte Folded Reload
        "ldr    w6, [sp, #28]\n"  // 4-byte Folded Reload
        "add    x10, x10, #32\n"  // =32
        "str    x10, [sp, #64]\n"  // 8-byte Folded Spill
        "ldr    x10, [sp, #136]\n"  // 8-byte Folded Reload
        "add    w6, w6, #1\n"  // =1
        "add    x10, x10, x12\n"
        "str    x10, [sp, #136]\n"  // 8-byte Folded Spill
        "ldr    x10, [sp, #40]\n"  // 8-byte Folded Reload
        "add    x10, x10, #8\n"  // =8
        "str    x10, [sp, #40]\n"  // 8-byte Folded Spill
        "ldr    x10, [sp, #48]\n"  // 8-byte Folded Reload
        "add    x10, x10, #8\n"  // =8
        "str    x10, [sp, #48]\n"  // 8-byte Folded Spill
        "ldr    x10, [sp, #56]\n"  // 8-byte Folded Reload
        "add    x10, x10, x12\n"
        "str    x10, [sp, #56]\n"  // 8-byte Folded Spill
        DC_KERNEL_NO_MULT_26 ":\n"  // =>This Loop Header: Depth=1
        // Child Loop BB225_24 Depth 2
        // Child Loop BB225_22 Depth 3
        // Child Loop BB225_20 Depth 4
        // Child Loop BB225_13 Depth 2
        // Child Loop BB225_5 Depth 3
        // Child Loop BB225_11 Depth 3
        "ldr    w10, [sp, #24]\n"  // 4-byte Folded Reload
        "cmp    w6, w10\n"
        "b.lt   " DC_KERNEL_NO_MULT_1 "b\n"
        // %bb.27:
        "add    sp, sp, #288\n"  // =448
        :
        // Outputs.
        [ scratch_block_data ] "+r"(scratch_block_data),
        [ filter_workspace ] "+r"(filter_workspace),
        [ bias_data ] "+r"(bias_data),
        [ output_block_data ] "+r"(output_block_data)
        :
        // Inputs.
        [ function_params ] "r"(function_params)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10",
        "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20",
        "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30",
        "v31",
        // We use these general-purpose registers.
        "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
        "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25",
        "x26", "x27", "x28", "x29", "x30");
  }  // NOLINT(readability/fn_size) Manually unrolled.

#undef DC_KERNEL_NO_MULT_1
#undef DC_KERNEL_NO_MULT_2
#undef DC_KERNEL_NO_MULT_3
#undef DC_KERNEL_NO_MULT_4
#undef DC_KERNEL_NO_MULT_5
#undef DC_KERNEL_NO_MULT_6
#undef DC_KERNEL_NO_MULT_7
#undef DC_KERNEL_NO_MULT_8
#undef DC_KERNEL_NO_MULT_9
#undef DC_KERNEL_NO_MULT_10
#undef DC_KERNEL_NO_MULT_11
#undef DC_KERNEL_NO_MULT_12
#undef DC_KERNEL_NO_MULT_13
#undef DC_KERNEL_NO_MULT_14
#undef DC_KERNEL_NO_MULT_15
#undef DC_KERNEL_NO_MULT_16
#undef DC_KERNEL_NO_MULT_17
#undef DC_KERNEL_NO_MULT_18
#undef DC_KERNEL_NO_MULT_19
#undef DC_KERNEL_NO_MULT_20
#undef DC_KERNEL_NO_MULT_21
#undef DC_KERNEL_NO_MULT_22
#undef DC_KERNEL_NO_MULT_23
#undef DC_KERNEL_NO_MULT_24
#undef DC_KERNEL_NO_MULT_25
#undef DC_KERNEL_NO_MULT_26

  static void __attribute__((noinline))
  Run(const int8* scratch_block_data, const int8* filter_workspace,
      const int32* bias_data, uint8* output_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    KernelMacroBlockNeon(scratch_block_data, filter_workspace, bias_data,
                         output_block_data, function_params);
  }
};

template <>
struct KernelMacroBlock<DepthwiseConvImplementation::kUseNeon3x3DotProduct,
                        DepthwiseConvDepthMultiplication::kNoMultiplication,
                        /*stride=*/2> {
  static inline void KernelMacroBlockNeon(
      const int8* scratch_block_data, const int8* filter_workspace,
      const int32* bias_data, uint8* output_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    // Note that argument registers may be reused after parameter loading.
    // x0 %[scratch_block_data]
    // x1 %[filter_workspace]
    // x2 %[bias_data]
    // x3 %[output_block_data]
    // x4 %[function_params]
#define DC_KERNEL_NO_MULT_STRIDE_1 "1"
#define DC_KERNEL_NO_MULT_STRIDE_2 "2"
#define DC_KERNEL_NO_MULT_STRIDE_3 "3"
#define DC_KERNEL_NO_MULT_STRIDE_4 "4"
#define DC_KERNEL_NO_MULT_STRIDE_5 "5"
#define DC_KERNEL_NO_MULT_STRIDE_6 "6"
#define DC_KERNEL_NO_MULT_STRIDE_7 "7"
#define DC_KERNEL_NO_MULT_STRIDE_8 "8"
#define DC_KERNEL_NO_MULT_STRIDE_9 "9"
#define DC_KERNEL_NO_MULT_STRIDE_10 "10"
#define DC_KERNEL_NO_MULT_STRIDE_11 "11"
#define DC_KERNEL_NO_MULT_STRIDE_12 "12"
#define DC_KERNEL_NO_MULT_STRIDE_13 "13"
#define DC_KERNEL_NO_MULT_STRIDE_14 "14"
#define DC_KERNEL_NO_MULT_STRIDE_15 "15"
#define DC_KERNEL_NO_MULT_STRIDE_16 "16"
#define DC_KERNEL_NO_MULT_STRIDE_17 "17"
#define DC_KERNEL_NO_MULT_STRIDE_18 "18"
#define DC_KERNEL_NO_MULT_STRIDE_19 "19"

    asm volatile(
        "sub    sp, sp, #32\n"  // =192
        "ldp    w11, w12, [%[function_params], #" STR(DP_OFFSET_OUTPUT_WIDTH_OVERALL_MICRO_REPEATS) "]\n"
        "ldp    w13, w14, [%[function_params], #" STR(DP_OFFSET_OUTPUT_RESIDUAL_WIDTH) "]\n"
        "ldrsw  x15, [%[function_params], #" STR(DP_OFFSET_INPUT_WIDTH_OVERALL_MICRO_REPEATS) "]\n"
        "ldr    x18, [%[function_params]]\n"
        "ldpsw  x9, x10, [%[function_params], #" STR(DP_OFFSET_OUTPUT_HEIGHT_STRIDE) "]\n"
        "ldrsw  x24, [%[function_params], #" STR(DP_OFFSET_DEPTH_MICRO_REPEATS) "]\n"
        "ldr    w25, [%[function_params], #" STR(DP_OFFSET_OUTBOUND_BLOCK_HEIGHT) "]\n"
        "add    x16, %[function_params], #" STR(DP_OFFSET_QUANTIZED_ACTIVATION_MIN) "\n"  // =40
        "add    x17, %[function_params], #" STR(DP_OFFSET_QUANTIZED_ACTIVATION_MAX) "\n"  // =44
        "add    x5, %[function_params], #" STR(DP_OFFSET_OUTPUT_MULTIPLIER) "\n"  // =32
        "add    x6, %[function_params], #" STR(DP_OFFSET_OUTPUT_SHIFT) "\n"  // =36
        "add    %[function_params], %[function_params], #" STR(DP_OFFSET_OUTPUT_OFFSET) "\n"  // =28
        "sxtw   x11, w11\n"
        "ld1r   { v0.8h }, [%[function_params]]\n"
        "ld1r   { v1.8b }, [x16]\n"
        "ld1r   { v2.8b }, [x17]\n"
        "ld1r   { v3.4s }, [x5]\n"
        "ld1r   { v4.4s }, [x6]\n"
        "cmp    w13, #1\n"  // =1
        "lsl    x26, x15, #5\n"
        "lsl    w15, w18, #1\n"
        "ccmp   w12, w11, #0, eq\n"
        "sxtw   x6, w15\n"
        "csel   w15, w12, w11, lt\n"
        "mov    x8, xzr\n"
        "lsl    x17, x10, #1\n"
        "sxtw   x18, w18\n"
        "add    %[function_params], x10, x10, lsl #1\n"
        "lsl    x5, x10, #2\n"
        "sxtw   x7, w15\n"
        "lsl    x19, x12, #5\n"
        // implicit-def: $q19
        // implicit-def: $q20
        // implicit-def: $q21
        // implicit-def: $q22
        // implicit-def: $q23
        // implicit-def: $q5
        // implicit-def: $q6
        // implicit-def: $q16
        // implicit-def: $q7
        // implicit-def: $q17
        // implicit-def: $q18
        "stp    %[filter_workspace], x24, [sp, #16]\n"  // 8-byte Folded Spill
        "str    w25, [sp, #12]\n"  // 4-byte Folded Spill
        "str    %[scratch_block_data], [sp]\n"  // 8-byte Folded Spill
        "b      " DC_KERNEL_NO_MULT_STRIDE_19 "f\n"
        DC_KERNEL_NO_MULT_STRIDE_1 ":\n"  // in Loop: Header=BB227_19 Depth=1
        "and    x15, x8, #0x1fffffff\n"
        "add    w16, w8, w8, lsl #1\n"
        "add    x20, %[output_block_data], x15, lsl #3\n"
        "lsl    w15, w16, #5\n"
        "cmp    w25, #2\n"  // =2
        "add    x21, %[filter_workspace], x15\n"
        "mov    x22, xzr\n"
        "mov    x23, xzr\n"
        "b.ne   " DC_KERNEL_NO_MULT_STRIDE_11 "f\n"
        // %bb.2:        // in Loop: Header=BB227_19 Depth=1
        "sxtw   x15, w8\n"
        "ubfiz  %[filter_workspace], x8, #3, #29\n"
        "mov    x12, x26\n"
        "madd   x24, x26, x15, %[scratch_block_data]\n"
        "mov    x16, %[output_block_data]\n"
        "add    x25, %[output_block_data], %[filter_workspace]\n"
        "add    x26, x20, x9\n"
        "mov    x27, %[bias_data]\n"
        "b      " DC_KERNEL_NO_MULT_STRIDE_9 "f\n"
        DC_KERNEL_NO_MULT_STRIDE_3 ":\n"  // in Loop: Header=BB227_9 Depth=2
        "add    %[filter_workspace], x24, x23, lsl #4\n"
        "add    x15, x21, x23, lsl #4\n"
        "ldr    q8, [%[filter_workspace], x5]\n"
        "ldr    q24, [x27]\n"
        "ldr    q25, [x15]\n"
        "ldr    q26, [x15, #32]\n"
        "ldr    q27, [x15, #64]\n"
        "ldr    q30, [%[filter_workspace]]\n"
        "ldr    q29, [%[filter_workspace], x10]\n"
        "ldr    q28, [%[filter_workspace], x17]\n"
        "ldr    q31, [%[filter_workspace], %[function_params]]\n"
        "mov    x28, xzr\n"
        "add    x30, %[filter_workspace], #32\n"  // =32
        "add    x15, x25, x23, lsl #2\n"
        "mov    x29, x22\n"
        "mov    v9.16b, v8.16b\n"
        "b      " DC_KERNEL_NO_MULT_STRIDE_5 "f\n"
        DC_KERNEL_NO_MULT_STRIDE_4 ":\n"  // in Loop: Header=BB227_5 Depth=3
        "mov    v23.16b, v24.16b\n"
        "mov    v10.16b, v24.16b\n"
        ".word 0x4e9e9737  // sdot   v23.4s, v25.16b, v30.16b\n"
        ".word 0x4e9c972a  // sdot   v10.4s, v25.16b, v28.16b\n"
        ".word 0x4e9d9757  // sdot   v23.4s, v26.16b, v29.16b\n"
        ".word 0x4e9f974a  // sdot   v10.4s, v26.16b, v31.16b\n"
        ".word 0x4e9c9777  // sdot   v23.4s, v27.16b, v28.16b\n"
        ".word 0x4e88976a  // sdot   v10.4s, v27.16b, v8.16b\n"
        "sqrdmulh        v23.4s, v23.4s, v3.4s\n"
        "ubfiz  %[scratch_block_data], x28, #5, #27\n"
        "rev32  v13.8h, v28.8h\n"
        "sqrdmulh        v28.4s, v10.4s, v3.4s\n"
        "sqrshl v23.4s, v23.4s, v4.4s\n"
        "add    %[scratch_block_data], x30, x0\n"
        "sqrshl v28.4s, v28.4s, v4.4s\n"
        "sqxtn  v23.4h, v23.4s\n"
        "ldr    q19, [%[scratch_block_data]]\n"
        "ldr    q20, [%[scratch_block_data], x10]\n"
        "ldr    q21, [%[scratch_block_data], x17]\n"
        "ldr    q22, [%[scratch_block_data], %[function_params]]\n"
        "ldr    q8, [%[scratch_block_data], x5]\n"
        "sqxtn2 v23.8h, v28.4s\n"
        "sqadd  v23.8h, v23.8h, v0.8h\n"
        "sqxtun v23.8b, v23.8h\n"
        "madd   %[filter_workspace], x28, x6, x15\n"
        "rev32  v11.8h, v30.8h\n"
        "umax   v23.8b, v23.8b, v1.8b\n"
        "rev32  v12.8h, v29.8h\n"
        "mov    v28.16b, v24.16b\n"
        "add    %[scratch_block_data], %[filter_workspace], x9\n"
        "umin   v23.8b, v23.8b, v2.8b\n"
        "trn1   v29.8h, v11.8h, v19.8h\n"
        "rev32  v14.8h, v31.8h\n"
        "str    s23, [%[filter_workspace]]\n"
        "st1    { v23.s }[1], [%[scratch_block_data]]\n"
        "mov    v23.16b, v24.16b\n"
        "trn1   v30.8h, v12.8h, v20.8h\n"
        "trn1   v31.8h, v13.8h, v21.8h\n"
        ".word 0x4e9d973c  // sdot   v28.4s, v25.16b, v29.16b\n"
        "rev32  v9.8h, v9.8h\n"
        "trn1   v10.8h, v14.8h, v22.8h\n"
        ".word 0x4e9f9737  // sdot   v23.4s, v25.16b, v31.16b\n"
        ".word 0x4e9e975c  // sdot   v28.4s, v26.16b, v30.16b\n"
        "trn1   v9.8h, v9.8h, v8.8h\n"
        ".word 0x4e8a9757  // sdot   v23.4s, v26.16b, v10.16b\n"
        ".word 0x4e9f977c  // sdot   v28.4s, v27.16b, v31.16b\n"
        ".word 0x4e899777  // sdot   v23.4s, v27.16b, v9.16b\n"
        "sqrdmulh        v28.4s, v28.4s, v3.4s\n"
        "sqrdmulh        v23.4s, v23.4s, v3.4s\n"
        "sqrshl v28.4s, v28.4s, v4.4s\n"
        "sqrshl v23.4s, v23.4s, v4.4s\n"
        "sqxtn  v28.4h, v28.4s\n"
        "sqxtn2 v28.8h, v23.4s\n"
        "sqadd  v23.8h, v28.8h, v0.8h\n"
        "sqxtun v23.8b, v23.8h\n"
        "add    %[scratch_block_data], %[filter_workspace], x18\n"
        "umax   v23.8b, v23.8b, v1.8b\n"
        "add    %[filter_workspace], %[scratch_block_data], x9\n"
        "umin   v23.8b, v23.8b, v2.8b\n"
        "add    x28, x28, #1\n"  // =1
        "str    s23, [%[scratch_block_data]]\n"
        "st1    { v23.s }[1], [%[filter_workspace]]\n"
        "add    x29, x29, x6\n"
        "mov    v30.16b, v19.16b\n"
        "mov    v29.16b, v20.16b\n"
        "mov    v28.16b, v21.16b\n"
        "mov    v31.16b, v22.16b\n"
        "mov    v9.16b, v8.16b\n"
        "mov    v23.16b, v8.16b\n"
        DC_KERNEL_NO_MULT_STRIDE_5 ":\n"  // Parent Loop BB227_19 Depth=1
        // Parent Loop BB227_9 Depth=2
        // =>  This Inner Loop Header: Depth=3
        "cmp    x28, x7\n"
        "b.lt   " DC_KERNEL_NO_MULT_STRIDE_4 "b\n"
        "b      " DC_KERNEL_NO_MULT_STRIDE_7 "f\n"
        DC_KERNEL_NO_MULT_STRIDE_6 ":\n"  // in Loop: Header=BB227_7 Depth=3
        "mov    v8.16b, v24.16b\n"
        "mov    v10.16b, v24.16b\n"
        ".word 0x4e9e9728  // sdot   v8.4s, v25.16b, v30.16b\n"
        ".word 0x4e9d9748  // sdot   v8.4s, v26.16b, v29.16b\n"
        ".word 0x4e9c972a  // sdot   v10.4s, v25.16b, v28.16b\n"
        ".word 0x4e9c9768  // sdot   v8.4s, v27.16b, v28.16b\n"
        ".word 0x4e9f974a  // sdot   v10.4s, v26.16b, v31.16b\n"
        ".word 0x4e89976a  // sdot   v10.4s, v27.16b, v9.16b\n"
        "sqrdmulh        v8.4s, v8.4s, v3.4s\n"
        "sqrdmulh        v10.4s, v10.4s, v3.4s\n"
        "sqrshl v8.4s, v8.4s, v4.4s\n"
        "sqrshl v10.4s, v10.4s, v4.4s\n"
        "sqxtn  v8.4h, v8.4s\n"
        "sqxtn2 v8.8h, v10.4s\n"
        "sqadd  v8.8h, v8.8h, v0.8h\n"
        "sqxtun v8.8b, v8.8h\n"
        "umax   v8.8b, v8.8b, v1.8b\n"
        "add    x15, x26, x29\n"
        "rev32  v30.8h, v30.8h\n"
        "rev32  v29.8h, v29.8h\n"
        "rev32  v28.8h, v28.8h\n"
        "rev32  v31.8h, v31.8h\n"
        "rev32  v9.8h, v9.8h\n"
        "umin   v8.8b, v8.8b, v2.8b\n"
        "add    x28, x28, #1\n"  // =1
        "trn1   v30.8h, v30.8h, v19.8h\n"
        "trn1   v29.8h, v29.8h, v20.8h\n"
        "trn1   v31.8h, v31.8h, v22.8h\n"
        "trn1   v28.8h, v28.8h, v21.8h\n"
        "trn1   v9.8h, v9.8h, v23.8h\n"
        "str    s8, [x20, x29]\n"
        "st1    { v8.s }[1], [x15]\n"
        "add    x29, x29, x6\n"
        DC_KERNEL_NO_MULT_STRIDE_7 ":\n"  // Parent Loop BB227_19 Depth=1
        // Parent Loop BB227_9 Depth=2
        // =>  This Inner Loop Header: Depth=3
        "cmp    x28, x11\n"
        "b.lt   " DC_KERNEL_NO_MULT_STRIDE_6 "b\n"
        // %bb.8:        // in Loop: Header=BB227_9 Depth=2
        "add    x27, x27, #16\n"  // =16
        "add    x23, x23, #1\n"  // =1
        "add    x22, x22, #4\n"  // =4
        DC_KERNEL_NO_MULT_STRIDE_9 ":\n"  // Parent Loop BB227_19 Depth=1
        // =>  This Loop Header: Depth=2
        // Child Loop BB227_5 Depth 3
        // Child Loop BB227_7 Depth 3
        "cmp    x23, #2\n"  // =2
        "b.ne   " DC_KERNEL_NO_MULT_STRIDE_3 "b\n"
        // %bb.10:        // in Loop: Header=BB227_19 Depth=1
        "ldp    %[filter_workspace], x24, [sp, #16]\n"  // 8-byte Folded Reload
        "ldr    %[scratch_block_data], [sp]\n"  // 8-byte Folded Reload
        "ldr    w25, [sp, #12]\n"  // 4-byte Folded Reload
        "mov    %[output_block_data], x16\n"
        "mov    x26, x12\n"
        "b      " DC_KERNEL_NO_MULT_STRIDE_18 "f\n"
        DC_KERNEL_NO_MULT_STRIDE_11 ":\n"  // in Loop: Header=BB227_19 Depth=1
        "mul    w15, w26, w8\n"
        "add    x15, %[scratch_block_data], w15, sxtw\n"
        "add    x12, x15, x10\n"
        "ldp    q8, q9, [x12]\n"
        "add    x12, x15, x17\n"
        "ldp    q24, q25, [x21]\n"
        "ldp    q26, q27, [x21, #32]\n"
        "ldp    q28, q29, [x21, #64]\n"
        "ldp    q10, q12, [x12]\n"
        "ldp    q30, q31, [%[bias_data]]\n"
        "ldp    q13, q11, [x15]\n"
        "add    x21, x15, #32\n"  // =32
        "b      " DC_KERNEL_NO_MULT_STRIDE_17 "f\n"
        DC_KERNEL_NO_MULT_STRIDE_12 ":\n"  // in Loop: Header=BB227_17 Depth=2
        "cmp    w11, w14\n"
        "ccmp   x19, x22, #0, eq\n"
        "b.eq   " DC_KERNEL_NO_MULT_STRIDE_14 "f\n"
        // %bb.13:        // in Loop: Header=BB227_17 Depth=2
        "and    x15, x22, #0xffffffe0\n"
        "add    x15, x21, x15\n"
        "add    x12, x15, x10\n"
        "add    x16, x15, x17\n"
        "ldp    q5, q7, [x15]\n"
        "ldp    q6, q17, [x12]\n"
        "ldp    q16, q18, [x16]\n"
        DC_KERNEL_NO_MULT_STRIDE_14 ":\n"  // in Loop: Header=BB227_17 Depth=2
        "mov    v14.16b, v30.16b\n"
        "mov    v15.16b, v31.16b\n"
        ".word 0x4e8d970e  // sdot   v14.4s, v24.16b, v13.16b\n"
        ".word 0x4e88974e  // sdot   v14.4s, v26.16b, v8.16b\n"
        ".word 0x4e8b972f  // sdot   v15.4s, v25.16b, v11.16b\n"
        ".word 0x4e8a978e  // sdot   v14.4s, v28.16b, v10.16b\n"
        ".word 0x4e89976f  // sdot   v15.4s, v27.16b, v9.16b\n"
        ".word 0x4e8c97af  // sdot   v15.4s, v29.16b, v12.16b\n"
        "sqrdmulh        v14.4s, v14.4s, v3.4s\n"
        "sqrdmulh        v15.4s, v15.4s, v3.4s\n"
        "sqrshl v14.4s, v14.4s, v4.4s\n"
        "sqrshl v15.4s, v15.4s, v4.4s\n"
        "sqxtn  v14.4h, v14.4s\n"
        "sqxtn2 v14.8h, v15.4s\n"
        "sqadd  v14.8h, v14.8h, v0.8h\n"
        "sqxtun v14.8b, v14.8h\n"
        "rev32  v13.8h, v13.8h\n"
        "rev32  v8.8h, v8.8h\n"
        "rev32  v10.8h, v10.8h\n"
        "rev32  v11.8h, v11.8h\n"
        "rev32  v9.8h, v9.8h\n"
        "rev32  v12.8h, v12.8h\n"
        "cmp    w13, #1\n"  // =1
        "umax   v14.8b, v14.8b, v1.8b\n"
        "trn1   v13.8h, v13.8h, v5.8h\n"
        "trn1   v11.8h, v11.8h, v7.8h\n"
        "ccmp   x19, x22, #0, le\n"
        "trn1   v8.8h, v8.8h, v6.8h\n"
        "trn1   v9.8h, v9.8h, v17.8h\n"
        "trn1   v10.8h, v10.8h, v16.8h\n"
        "umin   v14.8b, v14.8b, v2.8b\n"
        "trn1   v12.8h, v12.8h, v18.8h\n"
        "str    d14, [x20]\n"
        "b.eq   " DC_KERNEL_NO_MULT_STRIDE_16 "f\n"
        // %bb.15:        // in Loop: Header=BB227_17 Depth=2
        "mov    v14.16b, v30.16b\n"
        ".word 0x4e8d970e  // sdot   v14.4s, v24.16b, v13.16b\n"
        "mov    v13.16b, v31.16b\n"
        ".word 0x4e8b972d  // sdot   v13.4s, v25.16b, v11.16b\n"
        ".word 0x4e88974e  // sdot   v14.4s, v26.16b, v8.16b\n"
        ".word 0x4e89976d  // sdot   v13.4s, v27.16b, v9.16b\n"
        ".word 0x4e8a978e  // sdot   v14.4s, v28.16b, v10.16b\n"
        ".word 0x4e8c97ad  // sdot   v13.4s, v29.16b, v12.16b\n"
        "sqrdmulh        v8.4s, v14.4s, v3.4s\n"
        "sqrdmulh        v9.4s, v13.4s, v3.4s\n"
        "sqrshl v8.4s, v8.4s, v4.4s\n"
        "sqrshl v9.4s, v9.4s, v4.4s\n"
        "sqxtn  v8.4h, v8.4s\n"
        "sqxtn2 v8.8h, v9.4s\n"
        "sqadd  v8.8h, v8.8h, v0.8h\n"
        "sqxtun v8.8b, v8.8h\n"
        "umax   v8.8b, v8.8b, v1.8b\n"
        "umin   v8.8b, v8.8b, v2.8b\n"
        "str    d8, [x20, x18]\n"
        "mov    v13.16b, v5.16b\n"
        "mov    v8.16b, v6.16b\n"
        "mov    v10.16b, v16.16b\n"
        "mov    v11.16b, v7.16b\n"
        "mov    v9.16b, v17.16b\n"
        "mov    v12.16b, v18.16b\n"
        DC_KERNEL_NO_MULT_STRIDE_16 ":\n"  // in Loop: Header=BB227_17 Depth=2
        "add    x23, x23, #1\n"  // =1
        "add    x20, x20, x6\n"
        "add    x22, x22, #32\n"  // =32
        DC_KERNEL_NO_MULT_STRIDE_17 ":\n"  // Parent Loop BB227_19 Depth=1
        // =>  This Inner Loop Header: Depth=2
        "cmp    x23, x11\n"
        "b.lt   " DC_KERNEL_NO_MULT_STRIDE_12 "b\n"
        DC_KERNEL_NO_MULT_STRIDE_18 ":\n"  // in Loop: Header=BB227_19 Depth=1
        "add    %[bias_data], x2, #32\n"  // =32
        "add    x8, x8, #1\n"  // =1
        DC_KERNEL_NO_MULT_STRIDE_19 ":\n"  // =>This Loop Header: Depth=1
        // Child Loop BB227_17 Depth 2
        // Child Loop BB227_9 Depth 2
        // Child Loop BB227_5 Depth 3
        // Child Loop BB227_7 Depth 3
        "cmp    x8, x24\n"
        "b.lt   " DC_KERNEL_NO_MULT_STRIDE_1 "b\n"
        // %bb.20:
        "add    sp, sp, #32\n"  // =192
        :
        // Outputs.
        [ scratch_block_data ] "+r"(scratch_block_data),
        [ filter_workspace ] "+r"(filter_workspace),
        [ bias_data ] "+r"(bias_data),
        [ output_block_data ] "+r"(output_block_data)
        :
        // Inputs.
        [ function_params ] "r"(function_params)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10",
        "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20",
        "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30",
        "v31",
        // We use these general-purpose registers.
        "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
        "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25",
        "x26", "x27", "x28", "x29", "x30");
  }  // NOLINT(readability/fn_size) Manually unrolled.

#undef DC_KERNEL_NO_MULT_STRIDE_1
#undef DC_KERNEL_NO_MULT_STRIDE_2
#undef DC_KERNEL_NO_MULT_STRIDE_3
#undef DC_KERNEL_NO_MULT_STRIDE_4
#undef DC_KERNEL_NO_MULT_STRIDE_5
#undef DC_KERNEL_NO_MULT_STRIDE_6
#undef DC_KERNEL_NO_MULT_STRIDE_7
#undef DC_KERNEL_NO_MULT_STRIDE_8
#undef DC_KERNEL_NO_MULT_STRIDE_9
#undef DC_KERNEL_NO_MULT_STRIDE_10
#undef DC_KERNEL_NO_MULT_STRIDE_11
#undef DC_KERNEL_NO_MULT_STRIDE_12
#undef DC_KERNEL_NO_MULT_STRIDE_13
#undef DC_KERNEL_NO_MULT_STRIDE_14
#undef DC_KERNEL_NO_MULT_STRIDE_15
#undef DC_KERNEL_NO_MULT_STRIDE_16
#undef DC_KERNEL_NO_MULT_STRIDE_17
#undef DC_KERNEL_NO_MULT_STRIDE_18
#undef DC_KERNEL_NO_MULT_STRIDE_19

  static void __attribute__((noinline))
  Run(const int8* scratch_block_data, const int8* filter_workspace,
      const int32* bias_data, uint8* output_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    KernelMacroBlockNeon(scratch_block_data, filter_workspace, bias_data,
                         output_block_data, function_params);
  }
};

template <>
struct KernelMacroBlock<DepthwiseConvImplementation::kUseNeon3x3DotProduct,
                        DepthwiseConvDepthMultiplication::kUnitInputDepth,
                        /*stride=*/1> {
  static inline void KernelMacroBlockNeon(
      const int8* scratch_block_data, const int8* filter_workspace,
      const int32* bias_data, uint8* output_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    // Note that argument registers may be reused after parameter loading.
    // x0 %[scratch_block_data]
    // x1 %[filter_workspace]
    // x2 %[bias_data]
    // x3 %[output_block_data]
    // x4 %[function_params]
#define DC_KERNEL_MULT_1 "1"
#define DC_KERNEL_MULT_2 "2"
#define DC_KERNEL_MULT_3 "3"
#define DC_KERNEL_MULT_4 "4"
#define DC_KERNEL_MULT_5 "5"
#define DC_KERNEL_MULT_6 "6"
#define DC_KERNEL_MULT_7 "7"
#define DC_KERNEL_MULT_8 "8"
#define DC_KERNEL_MULT_9 "9"
#define DC_KERNEL_MULT_10 "10"
#define DC_KERNEL_MULT_11 "11"
#define DC_KERNEL_MULT_12 "12"
#define DC_KERNEL_MULT_13 "13"
#define DC_KERNEL_MULT_14 "14"
#define DC_KERNEL_MULT_15 "15"
#define DC_KERNEL_MULT_16 "16"
#define DC_KERNEL_MULT_17 "17"
#define DC_KERNEL_MULT_18 "18"
#define DC_KERNEL_MULT_19 "19"
#define DC_KERNEL_MULT_20 "20"
#define DC_KERNEL_MULT_21 "21"
#define DC_KERNEL_MULT_22 "22"

    asm volatile(
        "sub    sp, sp, #160\n"  // =288
        "stp    xzr, %[bias_data], [sp, #32]\n"  // 16-byte Folded Spill
        "ldr    w8, [%[function_params], #" STR(DP_OFFSET_DEPTH_MICRO_REPEATS) "]\n"
        "str    %[filter_workspace], [sp, #16]\n"  // 8-byte Folded Spill
        "ldpsw  x19, x11, [%[function_params], #" STR(DP_OFFSET_OUTPUT_HEIGHT_STRIDE) "]\n"
        "ldp    w20, w26, [%[function_params], #" STR(DP_OFFSET_OUTPUT_WIDTH_OVERALL_MICRO_REPEATS) "]\n"
        "str    w8, [sp, #8]\n"  // 4-byte Folded Spill
        "ldrsw  x8, [%[function_params], #" STR(DP_OFFSET_OUTBOUND_BLOCK_HEIGHT) "]\n"
        "ldrsw  x15, [%[function_params], #" STR(DP_OFFSET_OUTPUT_DEPTH) "]\n"
        "add    x12, %[function_params], #" STR(DP_OFFSET_OUTPUT_SHIFT) "\n"  // =36
        "add    x14, x11, x11, lsl #2\n"
        "stp    x8, %[output_block_data], [sp, #88]\n"  // 16-byte Folded Spill
        "ldrb   w8, [%[function_params], #" STR(DP_OFFSET_QUANTIZED_ACTIVATION_MIN) "]\n"
        "ldrb   w9, [%[function_params], #" STR(DP_OFFSET_QUANTIZED_ACTIVATION_MAX) "]\n"
        "ldr    w17, [%[function_params], #" STR(DP_OFFSET_OUTPUT_RESIDUAL_WIDTH) "]\n"
        "add    %[filter_workspace], x19, x15, lsl #1\n"
        "dup    v3.16b, w8\n"
        "dup    v4.16b, w9\n"
        "dup    v5.8b, w8\n"
        "dup    v6.8b, w9\n"
        "add    x8, x19, x19, lsl #1\n"
        "add    x9, x15, x15, lsl #1\n"
        "add    %[bias_data], x15, x19, lsl #1\n"
        "add    x10, %[function_params], #" STR(DP_OFFSET_OUTPUT_MULTIPLIER) "\n"  // =32
        "add    x13, %[function_params], #" STR(DP_OFFSET_OUTPUT_OFFSET) "\n"  // =28
        "ld1r   { v2.4s }, [x12]\n"
        "add    x6, x19, x15\n"
        "add    x12, %[scratch_block_data], x14\n"
        "add    x14, x9, x8\n"
        "add    %[function_params], x9, x19, lsl #1\n"
        "add    x5, x9, x19\n"
        "add    x9, %[output_block_data], x9\n"
        "add    %[filter_workspace], %[output_block_data], x1\n"
        "add    %[bias_data], %[output_block_data], x2\n"
        "ld1r   { v0.8h }, [x13]\n"
        "ld1r   { v1.4s }, [x10]\n"
        "str    x9, [sp, #56]\n"  // 8-byte Folded Spill
        "add    x9, x8, x15, lsl #1\n"
        "str    %[filter_workspace], [sp, #168]\n"  // 8-byte Folded Spill
        "add    %[filter_workspace], x8, x15\n"
        "str    %[bias_data], [sp, #152]\n"  // 8-byte Folded Spill
        "add    %[bias_data], %[output_block_data], x6\n"
        "add    x21, %[output_block_data], x8\n"
        "add    x8, %[output_block_data], x14\n"
        "add    x25, x11, x11, lsl #1\n"
        "cmp    w17, #4\n"  // =4
        "lsl    x16, x15, #1\n"
        "stp    x8, %[bias_data], [sp, #136]\n"  // 16-byte Folded Spill
        "add    x8, %[output_block_data], %[function_params]\n"
        "add    x28, %[output_block_data], x9\n"
        "lsl    x9, x11, #2\n"
        "add    x10, %[scratch_block_data], x11\n"
        "add    x23, %[scratch_block_data], x11, lsl #1\n"
        "add    x13, %[scratch_block_data], x11, lsl #2\n"
        "ccmp   w26, w20, #0, lt\n"
        "add    x16, x16, x19, lsl #1\n"
        "str    x8, [sp, #128]\n"  // 8-byte Folded Spill
        "add    x8, %[scratch_block_data], x25\n"
        "str    x9, [sp, #48]\n"  // 8-byte Folded Spill
        "mov    w9, w26\n"
        "mov    w7, wzr\n"
        "add    x22, x13, #4\n"  // =4
        "add    x23, x23, #4\n"  // =4
        "add    x24, x10, #4\n"  // =4
        "add    x27, %[output_block_data], x5\n"
        "add    x29, %[output_block_data], x16\n"
        "add    x30, %[output_block_data], %[filter_workspace]\n"
        "stp    x25, x19, [sp, #72]\n"  // 16-byte Folded Spill
        "add    x14, x8, #4\n"  // =4
        "lsl    x8, x11, #1\n"
        "lsl    x10, x15, #2\n"
        "add    %[bias_data], %[output_block_data], x15, lsl #1\n"
        "add    %[filter_workspace], %[output_block_data], x15\n"
        "add    %[function_params], %[output_block_data], x19, lsl #1\n"
        "add    x5, %[output_block_data], x19\n"
        "mov    w25, w20\n"
        "csel   w9, w26, w20, lt\n"
        "add    x16, x12, #4\n"  // =4
        "str    x12, [sp, #64]\n"  // 8-byte Folded Spill
        "str    %[output_block_data], [sp, #24]\n"  // 8-byte Folded Spill
        "b      " DC_KERNEL_MULT_22 "f\n"
        DC_KERNEL_MULT_1 ":\n"  // in Loop: Header=BB205_22 Depth=1
        "ldr    x12, [sp, #16]\n"  // 8-byte Folded Reload
        "str    w7, [sp, #12]\n"  // 4-byte Folded Spill
        "ldr    x13, [sp, #88]\n"  // 8-byte Folded Reload
        "ldp    q18, q7, [x12]\n"
        "ldp    q19, q16, [x12, #32]\n"
        "ldp    q20, q17, [x12, #64]\n"
        "add    x12, x12, #96\n"  // =96
        "cmp    w13, #4\n"  // =4
        "str    x12, [sp, #16]\n"  // 8-byte Folded Spill
        "mov    x12, xzr\n"
        "b.ne   " DC_KERNEL_MULT_12 "f\n"
        // %bb.2:        // in Loop: Header=BB205_22 Depth=1
        "ldp    x19, x13, [sp, #32]\n"  // 16-byte Folded Reload
        "str    x13, [sp, #120]\n"  // 8-byte Folded Spill
        "b      " DC_KERNEL_MULT_11 "f\n"
        DC_KERNEL_MULT_3 ":\n"  // in Loop: Header=BB205_11 Depth=2
        "str    x12, [sp, #112]\n"  // 8-byte Folded Spill
        "ldr    w12, [%[scratch_block_data]]\n"
        "add    %[output_block_data], %[scratch_block_data], x11\n"
        "ldr    x7, [sp, #72]\n"  // 8-byte Folded Reload
        "ldr    w6, [%[scratch_block_data], x8]\n"
        "fmov   s21, w12\n"
        "mov    v21.s[1], w12\n"
        "ld1    { v21.s }[2], [%[output_block_data]]\n"
        "ldr    %[output_block_data], [sp, #120]\n"  // 8-byte Folded Reload
        "ldr    w7, [%[scratch_block_data], x7]\n"
        "fmov   s23, w6\n"
        "mov    v23.s[1], w6\n"
        "ldr    q22, [%[output_block_data]]\n"
        "ldr    %[output_block_data], [sp, #48]\n"  // 8-byte Folded Reload
        "mov    v23.s[2], w7\n"
        "dup    v8.4s, w7\n"
        "dup    v31.4s, w6\n"
        "ldr    w3, [%[scratch_block_data], %[output_block_data]]\n"
        "mov    v23.s[3], w6\n"
        "ldp    x7, x6, [sp, #56]\n"  // 16-byte Folded Reload
        "mov    v28.16b, v22.16b\n"
        "fmov   s24, w3\n"
        "mov    v24.s[1], w3\n"
        "ld1    { v24.s }[2], [x6]\n"
        "ldr    x6, [sp, #96]\n"  // 8-byte Folded Reload
        "mov    v29.16b, v22.16b\n"
        "mov    v30.16b, v22.16b\n"
        ".word 0x4e9f969c  // sdot   v28.4s, v20.16b, v31.16b\n"
        ".word 0x4e9f967d  // sdot   v29.4s, v19.16b, v31.16b\n"
        ".word 0x4e9f965e  // sdot   v30.4s, v18.16b, v31.16b\n"
        "mov    v31.16b, v22.16b\n"
        "mov    x13, xzr\n"
        "shl    v25.4s, v18.4s, #8\n"
        "shl    v26.4s, v19.4s, #8\n"
        "shl    v27.4s, v20.4s, #8\n"
        "mov    v21.s[3], w12\n"
        "mov    v24.s[3], w3\n"
        ".word 0x4e88965f  // sdot   v31.4s, v18.16b, v8.16b\n"
        "mov    x12, x19\n"
        "b      " DC_KERNEL_MULT_5 "f\n"
        DC_KERNEL_MULT_4 ":\n"  // in Loop: Header=BB205_5 Depth=3
        ".word 0x4f95e25c  // sdot   v28.4s, v18.16b, v21.4b[0]\n"
        ".word 0x4f95ea5d  // sdot   v29.4s, v18.16b, v21.4b[2]\n"
        ".word 0x4f97ea7e  // sdot   v30.4s, v19.16b, v23.4b[2]\n"
        ".word 0x4f95ea7c  // sdot   v28.4s, v19.16b, v21.4b[2]\n"
        ".word 0x4f98e27f  // sdot   v31.4s, v19.16b, v24.4b[0]\n"
        ".word 0x4f97ea9d  // sdot   v29.4s, v20.16b, v23.4b[2]\n"
        ".word 0x4f98e29e  // sdot   v30.4s, v20.16b, v24.4b[0]\n"
        "sqrdmulh        v28.4s, v28.4s, v1.4s\n"
        ".word 0x4f98ea9f  // sdot   v31.4s, v20.16b, v24.4b[2]\n"
        "sqrdmulh        v29.4s, v29.4s, v1.4s\n"
        "sqrdmulh        v30.4s, v30.4s, v1.4s\n"
        "sqrshl v28.4s, v28.4s, v2.4s\n"
        "sqrdmulh        v31.4s, v31.4s, v1.4s\n"
        "sqrshl v29.4s, v29.4s, v2.4s\n"
        "sqrshl v30.4s, v30.4s, v2.4s\n"
        "sqxtn  v28.4h, v28.4s\n"
        "sqrshl v31.4s, v31.4s, v2.4s\n"
        "sqxtn  v30.4h, v30.4s\n"
        "sqxtn2 v28.8h, v29.4s\n"
        "sqxtn2 v30.8h, v31.4s\n"
        "sqadd  v28.8h, v28.8h, v0.8h\n"
        "sqadd  v29.8h, v30.8h, v0.8h\n"
        "sqxtun v28.8b, v28.8h\n"
        "sqxtun2        v28.16b, v29.8h\n"
        "umax   v28.16b, v28.16b, v3.16b\n"
        "add    %[output_block_data], x5, x12\n"
        "umin   v28.16b, v28.16b, v4.16b\n"
        "str    s28, [x6, x12]\n"
        "st1    { v28.s }[1], [%[output_block_data]]\n"
        "add    %[output_block_data], %[function_params], x12\n"
        "st1    { v28.s }[2], [%[output_block_data]]\n"
        "add    %[output_block_data], x21, x12\n"
        "st1    { v28.s }[3], [%[output_block_data]]\n"
        "add    %[output_block_data], %[scratch_block_data], x13, lsl #2\n"
        "add    %[output_block_data], x3, #4\n"  // =4
        "ld1    { v21.s }[1], [%[output_block_data]]\n"
        "add    %[output_block_data], x23, x13, lsl #2\n"
        "ld1    { v23.s }[1], [%[output_block_data]]\n"
        "add    %[output_block_data], x22, x13, lsl #2\n"
        "ld1    { v24.s }[1], [%[output_block_data]]\n"
        "add    %[output_block_data], x24, x13, lsl #2\n"
        "ld1    { v21.s }[3], [%[output_block_data]]\n"
        "add    %[output_block_data], x14, x13, lsl #2\n"
        "ld1    { v23.s }[3], [%[output_block_data]]\n"
        "add    %[output_block_data], x16, x13, lsl #2\n"
        "mov    v28.16b, v22.16b\n"
        "ld1    { v24.s }[3], [%[output_block_data]]\n"
        "mov    v29.16b, v22.16b\n"
        "mov    v30.16b, v22.16b\n"
        ".word 0x4f95e33c  // sdot   v28.4s, v25.16b, v21.4b[0]\n"
        "mov    v31.16b, v22.16b\n"
        ".word 0x4f95eb3d  // sdot   v29.4s, v25.16b, v21.4b[2]\n"
        ".word 0x4f97e33e  // sdot   v30.4s, v25.16b, v23.4b[0]\n"
        ".word 0x4f95eb5c  // sdot   v28.4s, v26.16b, v21.4b[2]\n"
        ".word 0x4f97eb3f  // sdot   v31.4s, v25.16b, v23.4b[2]\n"
        ".word 0x4f97e35d  // sdot   v29.4s, v26.16b, v23.4b[0]\n"
        ".word 0x4f97eb5e  // sdot   v30.4s, v26.16b, v23.4b[2]\n"
        ".word 0x4f97e37c  // sdot   v28.4s, v27.16b, v23.4b[0]\n"
        ".word 0x4f98e35f  // sdot   v31.4s, v26.16b, v24.4b[0]\n"
        ".word 0x4f97eb7d  // sdot   v29.4s, v27.16b, v23.4b[2]\n"
        ".word 0x4f98e37e  // sdot   v30.4s, v27.16b, v24.4b[0]\n"
        "sqrdmulh        v28.4s, v28.4s, v1.4s\n"
        ".word 0x4f98eb7f  // sdot   v31.4s, v27.16b, v24.4b[2]\n"
        "sqrdmulh        v29.4s, v29.4s, v1.4s\n"
        "sqrdmulh        v30.4s, v30.4s, v1.4s\n"
        "sqrshl v28.4s, v28.4s, v2.4s\n"
        "sqrdmulh        v31.4s, v31.4s, v1.4s\n"
        "sqrshl v29.4s, v29.4s, v2.4s\n"
        "sqrshl v30.4s, v30.4s, v2.4s\n"
        "sqxtn  v28.4h, v28.4s\n"
        "ldr    %[output_block_data], [sp, #144]\n"  // 8-byte Folded Reload
        "sqrshl v31.4s, v31.4s, v2.4s\n"
        "sqxtn  v30.4h, v30.4s\n"
        "sqxtn2 v28.8h, v29.4s\n"
        "sqxtn2 v30.8h, v31.4s\n"
        "sqadd  v28.8h, v28.8h, v0.8h\n"
        "sqadd  v29.8h, v30.8h, v0.8h\n"
        "sqxtun v28.8b, v28.8h\n"
        "sqxtun2        v28.16b, v29.8h\n"
        "umax   v28.16b, v28.16b, v3.16b\n"
        "add    %[output_block_data], x3, x12\n"
        "umin   v28.16b, v28.16b, v4.16b\n"
        "str    s28, [%[filter_workspace], x12]\n"
        "st1    { v28.s }[1], [%[output_block_data]]\n"
        "ldr    %[output_block_data], [sp, #152]\n"  // 8-byte Folded Reload
        "mov    v29.16b, v22.16b\n"
        "mov    v30.16b, v22.16b\n"
        "mov    v31.16b, v22.16b\n"
        "add    %[output_block_data], x3, x12\n"
        "st1    { v28.s }[2], [%[output_block_data]]\n"
        "add    %[output_block_data], x30, x12\n"
        "st1    { v28.s }[3], [%[output_block_data]]\n"
        "ushr   v28.2d, v21.2d, #16\n"
        "ushr   v9.2d, v23.2d, #16\n"
        ".word 0x4f9ce25d  // sdot   v29.4s, v18.16b, v28.4b[0]\n"
        "mov    v8.16b, v22.16b\n"
        ".word 0x4f9cea5e  // sdot   v30.4s, v18.16b, v28.4b[2]\n"
        ".word 0x4f89e25f  // sdot   v31.4s, v18.16b, v9.4b[0]\n"
        ".word 0x4f9cea7d  // sdot   v29.4s, v19.16b, v28.4b[2]\n"
        "ushr   v10.2d, v24.2d, #16\n"
        ".word 0x4f89ea48  // sdot   v8.4s, v18.16b, v9.4b[2]\n"
        ".word 0x4f89e27e  // sdot   v30.4s, v19.16b, v9.4b[0]\n"
        ".word 0x4f89ea7f  // sdot   v31.4s, v19.16b, v9.4b[2]\n"
        ".word 0x4f89e29d  // sdot   v29.4s, v20.16b, v9.4b[0]\n"
        ".word 0x4f8ae268  // sdot   v8.4s, v19.16b, v10.4b[0]\n"
        ".word 0x4f89ea9e  // sdot   v30.4s, v20.16b, v9.4b[2]\n"
        ".word 0x4f8ae29f  // sdot   v31.4s, v20.16b, v10.4b[0]\n"
        "sqrdmulh        v29.4s, v29.4s, v1.4s\n"
        ".word 0x4f8aea88  // sdot   v8.4s, v20.16b, v10.4b[2]\n"
        "sqrdmulh        v30.4s, v30.4s, v1.4s\n"
        "sqrdmulh        v31.4s, v31.4s, v1.4s\n"
        "sqrshl v29.4s, v29.4s, v2.4s\n"
        "sqrdmulh        v8.4s, v8.4s, v1.4s\n"
        "sqrshl v30.4s, v30.4s, v2.4s\n"
        "sqrshl v31.4s, v31.4s, v2.4s\n"
        "sqxtn  v29.4h, v29.4s\n"
        "ldr    %[output_block_data], [sp, #168]\n"  // 8-byte Folded Reload
        "sqrshl v8.4s, v8.4s, v2.4s\n"
        "sqxtn  v31.4h, v31.4s\n"
        "sqxtn2 v29.8h, v30.4s\n"
        "sqxtn2 v31.8h, v8.4s\n"
        "sqadd  v29.8h, v29.8h, v0.8h\n"
        "sqadd  v30.8h, v31.8h, v0.8h\n"
        "sqxtun v29.8b, v29.8h\n"
        "sqxtun2        v29.16b, v30.8h\n"
        "umax   v29.16b, v29.16b, v3.16b\n"
        "add    %[output_block_data], x3, x12\n"
        "umin   v29.16b, v29.16b, v4.16b\n"
        "str    s29, [%[bias_data], x12]\n"
        "st1    { v29.s }[1], [%[output_block_data]]\n"
        "add    %[output_block_data], x29, x12\n"
        "mov    v30.16b, v22.16b\n"
        "st1    { v29.s }[2], [%[output_block_data]]\n"
        "add    %[output_block_data], x28, x12\n"
        "mov    v31.16b, v22.16b\n"
        "mov    v8.16b, v22.16b\n"
        ".word 0x4f9ce33e  // sdot   v30.4s, v25.16b, v28.4b[0]\n"
        "st1    { v29.s }[3], [%[output_block_data]]\n"
        "mov    v29.16b, v22.16b\n"
        ".word 0x4f9ceb3f  // sdot   v31.4s, v25.16b, v28.4b[2]\n"
        ".word 0x4f89e328  // sdot   v8.4s, v25.16b, v9.4b[0]\n"
        ".word 0x4f9ceb5e  // sdot   v30.4s, v26.16b, v28.4b[2]\n"
        ".word 0x4f89eb3d  // sdot   v29.4s, v25.16b, v9.4b[2]\n"
        ".word 0x4f89e35f  // sdot   v31.4s, v26.16b, v9.4b[0]\n"
        ".word 0x4f89eb48  // sdot   v8.4s, v26.16b, v9.4b[2]\n"
        ".word 0x4f89e37e  // sdot   v30.4s, v27.16b, v9.4b[0]\n"
        ".word 0x4f8ae35d  // sdot   v29.4s, v26.16b, v10.4b[0]\n"
        ".word 0x4f89eb7f  // sdot   v31.4s, v27.16b, v9.4b[2]\n"
        ".word 0x4f8ae368  // sdot   v8.4s, v27.16b, v10.4b[0]\n"
        "sqrdmulh        v28.4s, v30.4s, v1.4s\n"
        ".word 0x4f8aeb7d  // sdot   v29.4s, v27.16b, v10.4b[2]\n"
        "sqrdmulh        v30.4s, v31.4s, v1.4s\n"
        "sqrdmulh        v31.4s, v8.4s, v1.4s\n"
        "sqrshl v28.4s, v28.4s, v2.4s\n"
        "sqrdmulh        v29.4s, v29.4s, v1.4s\n"
        "sqrshl v30.4s, v30.4s, v2.4s\n"
        "sqrshl v31.4s, v31.4s, v2.4s\n"
        "sqxtn  v28.4h, v28.4s\n"
        "sqrshl v29.4s, v29.4s, v2.4s\n"
        "sqxtn  v31.4h, v31.4s\n"
        "sqxtn2 v28.8h, v30.4s\n"
        "sqxtn2 v31.8h, v29.4s\n"
        "sqadd  v28.8h, v28.8h, v0.8h\n"
        "sqadd  v29.8h, v31.8h, v0.8h\n"
        "sqxtun v28.8b, v28.8h\n"
        "sqxtun2        v28.16b, v29.8h\n"
        "umax   v28.16b, v28.16b, v3.16b\n"
        "add    %[output_block_data], x27, x12\n"
        "umin   v8.16b, v28.16b, v4.16b\n"
        "str    s8, [x7, x12]\n"
        "st1    { v8.s }[1], [%[output_block_data]]\n"
        "ldr    %[output_block_data], [sp, #128]\n"  // 8-byte Folded Reload
        "mov    v28.16b, v22.16b\n"
        "mov    v29.16b, v22.16b\n"
        "mov    v30.16b, v22.16b\n"
        "add    %[output_block_data], x3, x12\n"
        "st1    { v8.s }[2], [%[output_block_data]]\n"
        "ldr    %[output_block_data], [sp, #136]\n"  // 8-byte Folded Reload
        "mov    v31.16b, v22.16b\n"
        "ushr   v23.2d, v23.2d, #32\n"
        "add    x13, x13, #1\n"  // =1
        "add    %[output_block_data], x3, x12\n"
        "ushr   v21.2d, v21.2d, #32\n"
        "ushr   v24.2d, v24.2d, #32\n"
        ".word 0x4f97e29c  // sdot   v28.4s, v20.16b, v23.4b[0]\n"
        ".word 0x4f97e27d  // sdot   v29.4s, v19.16b, v23.4b[0]\n"
        ".word 0x4f97e25e  // sdot   v30.4s, v18.16b, v23.4b[0]\n"
        ".word 0x4f97ea5f  // sdot   v31.4s, v18.16b, v23.4b[2]\n"
        "st1    { v8.s }[3], [%[output_block_data]]\n"
        "add    x12, x12, x10\n"
        DC_KERNEL_MULT_5 ":\n"  // Parent Loop BB205_22 Depth=1
        // Parent Loop BB205_11 Depth=2
        // =>  This Inner Loop Header: Depth=3
        "cmp    w13, w9\n"
        "b.lt   " DC_KERNEL_MULT_4 "b\n"
        // %bb.6:        // in Loop: Header=BB205_11 Depth=2
        "ldr    %[output_block_data], [sp, #120]\n"  // 8-byte Folded Reload
        "cmp    w13, w25\n"
        "str    x19, [sp, #104]\n"  // 8-byte Folded Spill
        "add    %[output_block_data], x3, #16\n"  // =16
        "str    %[output_block_data], [sp, #120]\n"  // 8-byte Folded Spill
        "b.ge   " DC_KERNEL_MULT_10 "f\n"
        // %bb.7:        // in Loop: Header=BB205_11 Depth=2
        "add    x7, %[scratch_block_data], x13, lsl #2\n"
        "add    x19, x23, x13, lsl #2\n"
        "ld1    { v23.s }[1], [x19]\n"
        "add    x19, x22, x13, lsl #2\n"
        "add    x7, x7, #4\n"  // =4
        "ld1    { v24.s }[1], [x19]\n"
        "ld1    { v21.s }[1], [x7]\n"
        "add    x19, x24, x13, lsl #2\n"
        "add    x7, x14, x13, lsl #2\n"
        "add    x13, x16, x13, lsl #2\n"
        "ldr    x20, [sp, #96]\n"  // 8-byte Folded Reload
        "ld1    { v23.s }[3], [x7]\n"
        "ld1    { v24.s }[3], [x13]\n"
        "ld1    { v21.s }[3], [x19]\n"
        "mov    %[output_block_data], xzr\n"
        "mov    w6, wzr\n"
        "add    x13, x21, x12\n"
        "add    x7, %[function_params], x12\n"
        "add    x19, x5, x12\n"
        "add    x12, x20, x12\n"
        "b      " DC_KERNEL_MULT_9 "f\n"
        DC_KERNEL_MULT_8 ":\n"  // in Loop: Header=BB205_9 Depth=3
        ".word 0x4f95e25c  // sdot   v28.4s, v18.16b, v21.4b[0]\n"
        ".word 0x4f95ea5d  // sdot   v29.4s, v18.16b, v21.4b[2]\n"
        ".word 0x4f97ea7e  // sdot   v30.4s, v19.16b, v23.4b[2]\n"
        ".word 0x4f95ea7c  // sdot   v28.4s, v19.16b, v21.4b[2]\n"
        ".word 0x4f98e27f  // sdot   v31.4s, v19.16b, v24.4b[0]\n"
        ".word 0x4f97ea9d  // sdot   v29.4s, v20.16b, v23.4b[2]\n"
        ".word 0x4f98e29e  // sdot   v30.4s, v20.16b, v24.4b[0]\n"
        "sqrdmulh        v25.4s, v28.4s, v1.4s\n"
        ".word 0x4f98ea9f  // sdot   v31.4s, v20.16b, v24.4b[2]\n"
        "sqrdmulh        v26.4s, v29.4s, v1.4s\n"
        "sqrdmulh        v27.4s, v30.4s, v1.4s\n"
        "sqrshl v25.4s, v25.4s, v2.4s\n"
        "sqrdmulh        v28.4s, v31.4s, v1.4s\n"
        "sqrshl v26.4s, v26.4s, v2.4s\n"
        "sqrshl v27.4s, v27.4s, v2.4s\n"
        "sqxtn  v25.4h, v25.4s\n"
        "sqrshl v28.4s, v28.4s, v2.4s\n"
        "sqxtn  v27.4h, v27.4s\n"
        "sqxtn2 v25.8h, v26.4s\n"
        "sqxtn2 v27.8h, v28.4s\n"
        "sqadd  v25.8h, v25.8h, v0.8h\n"
        "sqadd  v26.8h, v27.8h, v0.8h\n"
        "sqxtun v25.8b, v25.8h\n"
        "sqxtun2        v25.16b, v26.8h\n"
        "umax   v25.16b, v25.16b, v3.16b\n"
        "add    x20, x19, %[output_block_data]\n"
        "umin   v25.16b, v25.16b, v4.16b\n"
        "str    s25, [x12, %[output_block_data]]\n"
        "st1    { v25.s }[1], [x20]\n"
        "add    x20, x7, %[output_block_data]\n"
        "st1    { v25.s }[2], [x20]\n"
        "add    x20, x13, %[output_block_data]\n"
        "ushr   v23.2d, v23.2d, #8\n"
        "mov    v28.16b, v22.16b\n"
        "mov    v29.16b, v22.16b\n"
        "mov    v30.16b, v22.16b\n"
        "mov    v31.16b, v22.16b\n"
        "add    w6, w6, #1\n"  // =1
        "ushr   v21.2d, v21.2d, #8\n"
        "ushr   v24.2d, v24.2d, #8\n"
        "st1    { v25.s }[3], [x20]\n"
        ".word 0x4f97e29c  // sdot   v28.4s, v20.16b, v23.4b[0]\n"
        ".word 0x4f97e27d  // sdot   v29.4s, v19.16b, v23.4b[0]\n"
        ".word 0x4f97e25e  // sdot   v30.4s, v18.16b, v23.4b[0]\n"
        ".word 0x4f97ea5f  // sdot   v31.4s, v18.16b, v23.4b[2]\n"
        "add    %[output_block_data], x3, x15\n"
        DC_KERNEL_MULT_9 ":\n"  // Parent Loop BB205_22 Depth=1
        // Parent Loop BB205_11 Depth=2
        // =>  This Inner Loop Header: Depth=3
        "cmp    w6, w17\n"
        "b.lt   " DC_KERNEL_MULT_8 "b\n"
        DC_KERNEL_MULT_10 ":\n"  // in Loop: Header=BB205_11 Depth=2
        "ldp    x19, x12, [sp, #104]\n"  // 16-byte Folded Reload
        "mov    v20.16b, v17.16b\n"
        "mov    v19.16b, v16.16b\n"
        "mov    v18.16b, v7.16b\n"
        "add    x12, x12, #1\n"  // =1
        "add    x19, x19, #4\n"  // =4
        DC_KERNEL_MULT_11 ":\n"  // Parent Loop BB205_22 Depth=1
        // =>  This Loop Header: Depth=2
        // Child Loop BB205_5 Depth 3
        // Child Loop BB205_9 Depth 3
        "cmp    x12, #2\n"  // =2
        "b.ne   " DC_KERNEL_MULT_3 "b\n"
        "b      " DC_KERNEL_MULT_21 "f\n"
        DC_KERNEL_MULT_12 ":\n"  // in Loop: Header=BB205_22 Depth=1
        "ldr    x13, [sp, #40]\n"  // 8-byte Folded Reload
        "ldp    q21, q22, [x13]\n"
        "ldr    x13, [sp, #24]\n"  // 8-byte Folded Reload
        "str    x13, [sp, #120]\n"  // 8-byte Folded Spill
        "b      " DC_KERNEL_MULT_20 "f\n"
        DC_KERNEL_MULT_13 ":\n"  // in Loop: Header=BB205_20 Depth=2
        "madd   x6, x12, x11, %[scratch_block_data]\n"
        "ldr    w13, [x6]\n"
        "add    x7, x6, x11\n"
        "mov    w3, wzr\n"
        "fmov   s23, w13\n"
        "mov    v23.s[1], w13\n"
        "ld1    { v23.s }[2], [x7]\n"
        "add    x7, x6, x8\n"
        "ld1r   { v24.4s }, [x7]\n"
        "ldr    x7, [sp, #120]\n"  // 8-byte Folded Reload
        "mov    v23.s[3], w13\n"
        "b      " DC_KERNEL_MULT_18 "f\n"
        DC_KERNEL_MULT_14 ":\n"  // in Loop: Header=BB205_18 Depth=3
        "add    x6, x6, #4\n"  // =4
        "mov    x13, x6\n"
        "ld1    { v23.s }[1], [x13], x8\n"
        "add    x20, x6, x11\n"
        "cmp    w3, w26\n"
        "mov    w19, wzr\n"
        "ld1    { v23.s }[3], [x20]\n"
        "ld1    { v24.s }[1], [x13]\n"
        "orr    w13, wzr, #0x4\n"
        "csel   w13, w17, w13, eq\n"
        "b      " DC_KERNEL_MULT_16 "f\n"
        DC_KERNEL_MULT_15 ":\n"  // in Loop: Header=BB205_16 Depth=4
        "mov    v25.16b, v21.16b\n"
        "mov    v26.16b, v22.16b\n"
        ".word 0x4f97e259  // sdot   v25.4s, v18.16b, v23.4b[0]\n"
        ".word 0x4f97e0fa  // sdot   v26.4s, v7.16b, v23.4b[0]\n"
        ".word 0x4f97ea79  // sdot   v25.4s, v19.16b, v23.4b[2]\n"
        ".word 0x4f97ea1a  // sdot   v26.4s, v16.16b, v23.4b[2]\n"
        ".word 0x4f98e299  // sdot   v25.4s, v20.16b, v24.4b[0]\n"
        ".word 0x4f98e23a  // sdot   v26.4s, v17.16b, v24.4b[0]\n"
        "sqrdmulh        v25.4s, v25.4s, v1.4s\n"
        "sqrdmulh        v26.4s, v26.4s, v1.4s\n"
        "sqrshl v25.4s, v25.4s, v2.4s\n"
        "sqrshl v26.4s, v26.4s, v2.4s\n"
        "sqxtn  v25.4h, v25.4s\n"
        "sqxtn2 v25.8h, v26.4s\n"
        "sqadd  v25.8h, v25.8h, v0.8h\n"
        "sqxtun v25.8b, v25.8h\n"
        "umax   v25.8b, v25.8b, v5.8b\n"
        "umin   v25.8b, v25.8b, v6.8b\n"
        "ushr   v23.2d, v23.2d, #8\n"
        "ushr   v24.2d, v24.2d, #8\n"
        "str    d25, [x7]\n"
        "add    x7, x7, x15\n"
        "add    w19, w19, #1\n"  // =1
        DC_KERNEL_MULT_16 ":\n"  // Parent Loop BB205_22 Depth=1
        // Parent Loop BB205_20 Depth=2
        // Parent Loop BB205_18 Depth=3
        // =>  This Inner Loop Header: Depth=4
        "cmp    w19, w13\n"
        "b.lt   " DC_KERNEL_MULT_15 "b\n"
        // %bb.17:        // in Loop: Header=BB205_18 Depth=3
        "add    w3, w3, #1\n"  // =1
        DC_KERNEL_MULT_18 ":\n"  // Parent Loop BB205_22 Depth=1
        // Parent Loop BB205_20 Depth=2
        // =>  This Loop Header: Depth=3
        // Child Loop BB205_16 Depth 4
        "cmp    w3, w25\n"
        "b.lt   " DC_KERNEL_MULT_14 "b\n"
        // %bb.19:        // in Loop: Header=BB205_20 Depth=2
        "ldr    x13, [sp, #80]\n"  // 8-byte Folded Reload
        "ldr    %[output_block_data], [sp, #120]\n"  // 8-byte Folded Reload
        "add    x12, x12, #1\n"  // =1
        "add    %[output_block_data], x3, x13\n"
        "str    %[output_block_data], [sp, #120]\n"  // 8-byte Folded Spill
        DC_KERNEL_MULT_20 ":\n"  // Parent Loop BB205_22 Depth=1
        // =>  This Loop Header: Depth=2
        // Child Loop BB205_18 Depth 3
        // Child Loop BB205_16 Depth 4
        "ldr    x13, [sp, #88]\n"  // 8-byte Folded Reload
        "cmp    x12, x13\n"
        "b.lt   " DC_KERNEL_MULT_13 "b\n"
        DC_KERNEL_MULT_21 ":\n"  // in Loop: Header=BB205_22 Depth=1
        "ldr    x12, [sp, #40]\n"  // 8-byte Folded Reload
        "ldr    w7, [sp, #12]\n"  // 4-byte Folded Reload
        "add    x12, x12, #32\n"  // =32
        "str    x12, [sp, #40]\n"  // 8-byte Folded Spill
        "ldr    x12, [sp, #24]\n"  // 8-byte Folded Reload
        "add    w7, w7, #1\n"  // =1
        "add    x12, x12, #8\n"  // =8
        "str    x12, [sp, #24]\n"  // 8-byte Folded Spill
        "ldr    x12, [sp, #32]\n"  // 8-byte Folded Reload
        "add    x12, x12, #8\n"  // =8
        "str    x12, [sp, #32]\n"  // 8-byte Folded Spill
        DC_KERNEL_MULT_22 ":\n"  // =>This Loop Header: Depth=1
        // Child Loop BB205_20 Depth 2
        // Child Loop BB205_18 Depth 3
        // Child Loop BB205_16 Depth 4
        // Child Loop BB205_11 Depth 2
        // Child Loop BB205_5 Depth 3
        // Child Loop BB205_9 Depth 3
        "ldr    w12, [sp, #8]\n"  // 4-byte Folded Reload
        "cmp    w7, w12\n"
        "b.lt   " DC_KERNEL_MULT_1 "b\n"
        // %bb.23:
        "add    sp, sp, #160\n"  // =288
        :
        // Outputs.
        [ scratch_block_data ] "+r"(scratch_block_data),
        [ filter_workspace ] "+r"(filter_workspace),
        [ bias_data ] "+r"(bias_data),
        [ output_block_data ] "+r"(output_block_data)
        :
        // Inputs.
        [ function_params ] "r"(function_params)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10",
        "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20",
        "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30",
        "v31",
        // We use these general-purpose registers.
        "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
        "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25",
        "x26", "x27", "x28", "x29", "x30");
  }  // NOLINT(readability/fn_size) Manually unrolled.

#undef DC_KERNEL_MULT_1
#undef DC_KERNEL_MULT_2
#undef DC_KERNEL_MULT_3
#undef DC_KERNEL_MULT_4
#undef DC_KERNEL_MULT_5
#undef DC_KERNEL_MULT_6
#undef DC_KERNEL_MULT_7
#undef DC_KERNEL_MULT_8
#undef DC_KERNEL_MULT_9
#undef DC_KERNEL_MULT_10
#undef DC_KERNEL_MULT_11
#undef DC_KERNEL_MULT_12
#undef DC_KERNEL_MULT_13
#undef DC_KERNEL_MULT_14
#undef DC_KERNEL_MULT_15
#undef DC_KERNEL_MULT_16
#undef DC_KERNEL_MULT_17
#undef DC_KERNEL_MULT_18
#undef DC_KERNEL_MULT_19
#undef DC_KERNEL_MULT_20
#undef DC_KERNEL_MULT_21
#undef DC_KERNEL_MULT_22

  static void __attribute__((noinline))
  Run(const int8* scratch_block_data, const int8* filter_workspace,
      const int32* bias_data, uint8* output_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    KernelMacroBlockNeon(scratch_block_data, filter_workspace, bias_data,
                         output_block_data, function_params);
  }
};

template <>
struct KernelMacroBlock<DepthwiseConvImplementation::kUseNeon3x3DotProduct,
                        DepthwiseConvDepthMultiplication::kUnitInputDepth,
                        /*stride=*/2> {
  static inline void KernelMacroBlockNeon(
      const int8* scratch_block_data, const int8* filter_workspace,
      const int32* bias_data, uint8* output_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    // Note that argument registers may be reused after parameter loading.
    // x0 %[scratch_block_data]
    // x1 %[filter_workspace]
    // x2 %[bias_data]
    // x3 %[output_block_data]
    // x4 %[function_params]
#define DC_KERNEL_MULT_STRIDE_1 "1"
#define DC_KERNEL_MULT_STRIDE_2 "2"
#define DC_KERNEL_MULT_STRIDE_3 "3"
#define DC_KERNEL_MULT_STRIDE_4 "4"
#define DC_KERNEL_MULT_STRIDE_5 "5"
#define DC_KERNEL_MULT_STRIDE_6 "6"
#define DC_KERNEL_MULT_STRIDE_7 "7"
#define DC_KERNEL_MULT_STRIDE_8 "8"
#define DC_KERNEL_MULT_STRIDE_9 "9"
#define DC_KERNEL_MULT_STRIDE_10 "10"
#define DC_KERNEL_MULT_STRIDE_11 "11"
#define DC_KERNEL_MULT_STRIDE_12 "12"
#define DC_KERNEL_MULT_STRIDE_13 "13"

    asm volatile(
        "ldr    w15, [%[function_params], #" STR(DP_OFFSET_OUTPUT_RESIDUAL_WIDTH) "]\n"
        "ldp    w11, w6, [%[function_params], #" STR(DP_OFFSET_OUTPUT_WIDTH_OVERALL_MICRO_REPEATS) "]\n"
        "ldpsw  x9, x10, [%[function_params], #" STR(DP_OFFSET_OUTPUT_HEIGHT_STRIDE) "]\n"
        "ldrsw  x12, [%[function_params], #" STR(DP_OFFSET_DEPTH_MICRO_REPEATS) "]\n"
        "ldrsw  x13, [%[function_params], #" STR(DP_OFFSET_OUTPUT_DEPTH) "]\n"
        "ldr    w14, [%[function_params], #" STR(DP_OFFSET_OUTBOUND_BLOCK_HEIGHT) "]\n"
        "add    x17, %[function_params], #" STR(DP_OFFSET_QUANTIZED_ACTIVATION_MIN) "\n"  // =40
        "add    x5, %[function_params], #" STR(DP_OFFSET_QUANTIZED_ACTIVATION_MAX) "\n"  // =44
        "add    x7, %[function_params], #" STR(DP_OFFSET_OUTPUT_MULTIPLIER) "\n"  // =32
        "add    x19, %[function_params], #" STR(DP_OFFSET_OUTPUT_SHIFT) "\n"  // =36
        "add    %[function_params], %[function_params], #" STR(DP_OFFSET_OUTPUT_OFFSET) "\n"  // =28
        "sxtw   x11, w11\n"
        "ld1r   { v0.8h }, [%[function_params]]\n"
        "ld1r   { v1.4s }, [x7]\n"
        "ld1r   { v2.4s }, [x19]\n"
        "ld1r   { v3.8b }, [x17]\n"
        "ld1r   { v4.8b }, [x5]\n"
        "cmp    w15, #2\n"  // =2
        "ccmp   w6, w11, #0, lt\n"
        "lsl    x5, x6, #2\n"
        "csel   w6, w6, w11, lt\n"
        "mov    x8, xzr\n"
        "add    x16, %[scratch_block_data], #4\n"  // =4
        "lsl    x17, x10, #1\n"
        "add    %[function_params], x10, x10, lsl #1\n"
        "sxtw   x6, w6\n"
        "add    x7, x9, x13\n"
        "b      " DC_KERNEL_MULT_STRIDE_13 "f\n"
        DC_KERNEL_MULT_STRIDE_1 ":\n"  // in Loop: Header=BB206_13 Depth=1
        "ldr    w20, [%[scratch_block_data]]\n"
        "add    x21, %[scratch_block_data], x10\n"
        "ldp    q5, q6, [%[filter_workspace]]\n"
        "ldp    q7, q16, [%[filter_workspace], #32]\n"
        "fmov   s21, w20\n"
        "mov    v21.s[1], w20\n"
        "ld1    { v21.s }[2], [x21]\n"
        "ldp    q17, q18, [%[filter_workspace], #64]\n"
        "ldp    q19, q20, [%[bias_data]], #32\n"
        "ldr    s22, [%[scratch_block_data], x17]\n"
        "ubfiz  x19, x8, #3, #29\n"
        "add    %[filter_workspace], %[filter_workspace], #96\n"  // =96
        "add    x19, %[output_block_data], x19\n"
        "cmp    w14, #2\n"  // =2
        "mov    v21.s[3], w20\n"
        "mov    x20, xzr\n"
        "b.ne   " DC_KERNEL_MULT_STRIDE_7 "f\n"
        // %bb.2:        // in Loop: Header=BB206_13 Depth=1
        "dup    v22.4s, v22.s[0]\n"
        "add    x21, %[scratch_block_data], %[function_params]\n"
        "add    x22, %[scratch_block_data], x10, lsl #2\n"
        "ld1    { v22.s }[2], [x21]\n"
        "ld1r   { v23.4s }, [x22]\n"
        "mov    x21, xzr\n"
        "b      " DC_KERNEL_MULT_STRIDE_4 "f\n"
        DC_KERNEL_MULT_STRIDE_3 ":\n"  // in Loop: Header=BB206_4 Depth=2
        "and    x22, x20, #0xfffffffc\n"
        "add    x23, x16, x22\n"
        "lsl    x24, x10, #2\n"
        "mov    x22, x23\n"
        "ld1    { v21.s }[1], [x22], x24\n"
        "add    x24, x23, x17\n"
        "ld1    { v22.s }[1], [x24]\n"
        "add    x24, x23, x10\n"
        "ld1    { v21.s }[3], [x24]\n"
        "add    x23, x23, %[function_params]\n"
        "ld1    { v22.s }[3], [x23]\n"
        "mov    v25.16b, v19.16b\n"
        "mov    v27.16b, v20.16b\n"
        "ld1    { v23.s }[1], [x22]\n"
        "ushr   v29.2d, v21.2d, #16\n"
        ".word 0x4f9de0b9  // sdot   v25.4s, v5.16b, v29.4b[0]\n"
        ".word 0x4f9de0db  // sdot   v27.4s, v6.16b, v29.4b[0]\n"
        "mov    v26.16b, v19.16b\n"
        "mov    v28.16b, v20.16b\n"
        ".word 0x4f9de8f9  // sdot   v25.4s, v7.16b, v29.4b[2]\n"
        ".word 0x4f9dea1b  // sdot   v27.4s, v16.16b, v29.4b[2]\n"
        "ushr   v29.2d, v22.2d, #16\n"
        ".word 0x4f9de0ba  // sdot   v26.4s, v5.16b, v29.4b[0]\n"
        ".word 0x4f9de0dc  // sdot   v28.4s, v6.16b, v29.4b[0]\n"
        "mov    v24.16b, v19.16b\n"
        ".word 0x4f9de8fa  // sdot   v26.4s, v7.16b, v29.4b[2]\n"
        ".word 0x4f9dea1c  // sdot   v28.4s, v16.16b, v29.4b[2]\n"
        ".word 0x4f9de239  // sdot   v25.4s, v17.16b, v29.4b[0]\n"
        ".word 0x4f9de25b  // sdot   v27.4s, v18.16b, v29.4b[0]\n"
        "ushr   v29.2d, v23.2d, #16\n"
        ".word 0x4f9de23a  // sdot   v26.4s, v17.16b, v29.4b[0]\n"
        ".word 0x4f9de25c  // sdot   v28.4s, v18.16b, v29.4b[0]\n"
        "mov    v29.16b, v19.16b\n"
        ".word 0x4f95e0b8  // sdot   v24.4s, v5.16b, v21.4b[0]\n"
        ".word 0x4f96e0bd  // sdot   v29.4s, v5.16b, v22.4b[0]\n"
        ".word 0x4f95e8f8  // sdot   v24.4s, v7.16b, v21.4b[2]\n"
        ".word 0x4f96e8fd  // sdot   v29.4s, v7.16b, v22.4b[2]\n"
        ".word 0x4f96e238  // sdot   v24.4s, v17.16b, v22.4b[0]\n"
        ".word 0x4f97e23d  // sdot   v29.4s, v17.16b, v23.4b[0]\n"
        "sqrdmulh        v24.4s, v24.4s, v1.4s\n"
        "sqrdmulh        v29.4s, v29.4s, v1.4s\n"
        "sqrshl v24.4s, v24.4s, v2.4s\n"
        "sqrshl v29.4s, v29.4s, v2.4s\n"
        "sqxtn  v24.4h, v24.4s\n"
        "sqxtn2 v24.8h, v29.4s\n"
        "sqadd  v24.8h, v24.8h, v0.8h\n"
        "sqxtun v24.8b, v24.8h\n"
        "umax   v24.8b, v24.8b, v3.8b\n"
        "add    x22, x19, x9\n"
        "mov    v29.16b, v20.16b\n"
        "umin   v24.8b, v24.8b, v4.8b\n"
        "str    s24, [x19]\n"
        "st1    { v24.s }[1], [x22]\n"
        "mov    v24.16b, v20.16b\n"
        ".word 0x4f95e0dd  // sdot   v29.4s, v6.16b, v21.4b[0]\n"
        ".word 0x4f96e0d8  // sdot   v24.4s, v6.16b, v22.4b[0]\n"
        ".word 0x4f95ea1d  // sdot   v29.4s, v16.16b, v21.4b[2]\n"
        ".word 0x4f96ea18  // sdot   v24.4s, v16.16b, v22.4b[2]\n"
        ".word 0x4f96e25d  // sdot   v29.4s, v18.16b, v22.4b[0]\n"
        ".word 0x4f97e258  // sdot   v24.4s, v18.16b, v23.4b[0]\n"
        "sqrdmulh        v29.4s, v29.4s, v1.4s\n"
        "sqrdmulh        v24.4s, v24.4s, v1.4s\n"
        "sqrshl v29.4s, v29.4s, v2.4s\n"
        "sqrshl v24.4s, v24.4s, v2.4s\n"
        "sqxtn  v29.4h, v29.4s\n"
        "sqxtn2 v29.8h, v24.4s\n"
        "sqadd  v24.8h, v29.8h, v0.8h\n"
        "sqxtun v24.8b, v24.8h\n"
        "sqrdmulh        v25.4s, v25.4s, v1.4s\n"
        "umax   v24.8b, v24.8b, v3.8b\n"
        "sqrdmulh        v26.4s, v26.4s, v1.4s\n"
        "sqrshl v25.4s, v25.4s, v2.4s\n"
        "add    x22, x22, #4\n"  // =4
        "umin   v24.8b, v24.8b, v4.8b\n"
        "sqrshl v26.4s, v26.4s, v2.4s\n"
        "sqxtn  v25.4h, v25.4s\n"
        "str    s24, [x19, #4]\n"
        "st1    { v24.s }[1], [x22]\n"
        "sqxtn2 v25.8h, v26.4s\n"
        "sqadd  v24.8h, v25.8h, v0.8h\n"
        "sqrdmulh        v27.4s, v27.4s, v1.4s\n"
        "sqxtun v24.8b, v24.8h\n"
        "sqrdmulh        v28.4s, v28.4s, v1.4s\n"
        "sqrshl v27.4s, v27.4s, v2.4s\n"
        "umax   v24.8b, v24.8b, v3.8b\n"
        "add    x23, x19, x13\n"
        "add    x24, x19, x7\n"
        "sqrshl v28.4s, v28.4s, v2.4s\n"
        "sqxtn  v27.4h, v27.4s\n"
        "umin   v24.8b, v24.8b, v4.8b\n"
        "str    s24, [x23]\n"
        "st1    { v24.s }[1], [x24]\n"
        "sqxtn2 v27.8h, v28.4s\n"
        "sqadd  v24.8h, v27.8h, v0.8h\n"
        "sqxtun v24.8b, v24.8h\n"
        "umax   v24.8b, v24.8b, v3.8b\n"
        "add    x25, x24, #4\n"  // =4
        "umin   v24.8b, v24.8b, v4.8b\n"
        "add    x21, x21, #1\n"  // =1
        "ushr   v21.2d, v21.2d, #32\n"
        "ushr   v22.2d, v22.2d, #32\n"
        "ushr   v23.2d, v23.2d, #32\n"
        "add    x19, x23, x13\n"
        "str    s24, [x23, #4]\n"
        "st1    { v24.s }[1], [x25]\n"
        "add    x20, x20, #4\n"  // =4
        DC_KERNEL_MULT_STRIDE_4 ":\n"  // Parent Loop BB206_13 Depth=1
        // =>  This Inner Loop Header: Depth=2
        "cmp    x21, x6\n"
        "b.lt   " DC_KERNEL_MULT_STRIDE_3 "b\n"
        "b      " DC_KERNEL_MULT_STRIDE_6 "f\n"
        DC_KERNEL_MULT_STRIDE_5 ":\n"  // in Loop: Header=BB206_6 Depth=2
        "and    x22, x20, #0xfffffffc\n"
        "add    x22, x16, x22\n"
        "lsl    x23, x10, #2\n"
        "mov    x25, x22\n"
        "add    x24, x22, x17\n"
        "ld1    { v21.s }[1], [x25], x23\n"
        "ld1    { v22.s }[1], [x24]\n"
        "add    x23, x22, x10\n"
        "add    x22, x22, %[function_params]\n"
        "ld1    { v21.s }[3], [x23]\n"
        "ld1    { v22.s }[3], [x22]\n"
        "mov    v24.16b, v19.16b\n"
        "ld1    { v23.s }[1], [x25]\n"
        "mov    v25.16b, v19.16b\n"
        ".word 0x4f95e0b8  // sdot   v24.4s, v5.16b, v21.4b[0]\n"
        ".word 0x4f96e0b9  // sdot   v25.4s, v5.16b, v22.4b[0]\n"
        ".word 0x4f95e8f8  // sdot   v24.4s, v7.16b, v21.4b[2]\n"
        ".word 0x4f96e8f9  // sdot   v25.4s, v7.16b, v22.4b[2]\n"
        ".word 0x4f96e238  // sdot   v24.4s, v17.16b, v22.4b[0]\n"
        ".word 0x4f97e239  // sdot   v25.4s, v17.16b, v23.4b[0]\n"
        "sqrdmulh        v24.4s, v24.4s, v1.4s\n"
        "sqrdmulh        v25.4s, v25.4s, v1.4s\n"
        "sqrshl v24.4s, v24.4s, v2.4s\n"
        "sqrshl v25.4s, v25.4s, v2.4s\n"
        "sqxtn  v24.4h, v24.4s\n"
        "sqxtn2 v24.8h, v25.4s\n"
        "sqadd  v24.8h, v24.8h, v0.8h\n"
        "sqxtun v24.8b, v24.8h\n"
        "umax   v24.8b, v24.8b, v3.8b\n"
        "add    x22, x19, x9\n"
        "mov    v25.16b, v20.16b\n"
        "umin   v24.8b, v24.8b, v4.8b\n"
        "str    s24, [x19]\n"
        "st1    { v24.s }[1], [x22]\n"
        "mov    v24.16b, v20.16b\n"
        ".word 0x4f95e0d9  // sdot   v25.4s, v6.16b, v21.4b[0]\n"
        ".word 0x4f96e0d8  // sdot   v24.4s, v6.16b, v22.4b[0]\n"
        ".word 0x4f95ea19  // sdot   v25.4s, v16.16b, v21.4b[2]\n"
        ".word 0x4f96ea18  // sdot   v24.4s, v16.16b, v22.4b[2]\n"
        ".word 0x4f96e259  // sdot   v25.4s, v18.16b, v22.4b[0]\n"
        ".word 0x4f97e258  // sdot   v24.4s, v18.16b, v23.4b[0]\n"
        "sqrdmulh        v25.4s, v25.4s, v1.4s\n"
        "sqrdmulh        v24.4s, v24.4s, v1.4s\n"
        "sqrshl v25.4s, v25.4s, v2.4s\n"
        "sqrshl v24.4s, v24.4s, v2.4s\n"
        "sqxtn  v25.4h, v25.4s\n"
        "sqxtn2 v25.8h, v24.4s\n"
        "sqadd  v24.8h, v25.8h, v0.8h\n"
        "sqxtun v24.8b, v24.8h\n"
        "umax   v24.8b, v24.8b, v3.8b\n"
        "add    x22, x22, #4\n"  // =4
        "umin   v24.8b, v24.8b, v4.8b\n"
        "add    x21, x21, #1\n"  // =1
        "ushr   v21.2d, v21.2d, #16\n"
        "ushr   v22.2d, v22.2d, #16\n"
        "ushr   v23.2d, v23.2d, #16\n"
        "str    s24, [x19, #4]\n"
        "st1    { v24.s }[1], [x22]\n"
        "add    x19, x19, x13\n"
        "add    x20, x20, #4\n"  // =4
        DC_KERNEL_MULT_STRIDE_6 ":\n"  // Parent Loop BB206_13 Depth=1
        // =>  This Inner Loop Header: Depth=2
        "cmp    x21, x11\n"
        "b.lt   " DC_KERNEL_MULT_STRIDE_5 "b\n"
        "b      " DC_KERNEL_MULT_STRIDE_12 "f\n"
        DC_KERNEL_MULT_STRIDE_7 ":\n"  // in Loop: Header=BB206_13 Depth=1
        "mov    x21, xzr\n"
        "dup    v22.4s, v22.s[0]\n"
        "b      " DC_KERNEL_MULT_STRIDE_11 "f\n"
        DC_KERNEL_MULT_STRIDE_8 ":\n"  // in Loop: Header=BB206_11 Depth=2
        "and    x22, x20, #0xfffffffc\n"
        "add    x22, x16, x22\n"
        "mov    x23, x22\n"
        "ld1    { v21.s }[1], [x23], x17\n"
        "add    x22, x22, x10\n"
        "mov    v23.16b, v19.16b\n"
        "mov    v24.16b, v20.16b\n"
        "ld1    { v22.s }[1], [x23]\n"
        "ld1    { v21.s }[3], [x22]\n"
        "cmp    w15, #2\n"  // =2
        "ccmp   x5, x20, #0, ne\n"
        ".word 0x4f96e237  // sdot   v23.4s, v17.16b, v22.4b[0]\n"
        ".word 0x4f96e258  // sdot   v24.4s, v18.16b, v22.4b[0]\n"
        ".word 0x4f95e0b7  // sdot   v23.4s, v5.16b, v21.4b[0]\n"
        ".word 0x4f95e0d8  // sdot   v24.4s, v6.16b, v21.4b[0]\n"
        ".word 0x4f95e8f7  // sdot   v23.4s, v7.16b, v21.4b[2]\n"
        ".word 0x4f95ea18  // sdot   v24.4s, v16.16b, v21.4b[2]\n"
        "sqrdmulh        v23.4s, v23.4s, v1.4s\n"
        "sqrdmulh        v24.4s, v24.4s, v1.4s\n"
        "sqrshl v23.4s, v23.4s, v2.4s\n"
        "sqrshl v24.4s, v24.4s, v2.4s\n"
        "sqxtn  v25.4h, v23.4s\n"
        "sqxtn2 v25.8h, v24.4s\n"
        "sqadd  v24.8h, v25.8h, v0.8h\n"
        "sqxtun v24.8b, v24.8h\n"
        "umax   v24.8b, v24.8b, v3.8b\n"
        "umin   v24.8b, v24.8b, v4.8b\n"
        "ushr   v23.2d, v21.2d, #16\n"
        "str    d24, [x19]\n"
        "ushr   v24.2d, v22.2d, #16\n"
        "add    x19, x19, x13\n"
        "b.eq   " DC_KERNEL_MULT_STRIDE_10 "f\n"
        // %bb.9:        // in Loop: Header=BB206_11 Depth=2
        "mov    v25.16b, v19.16b\n"
        "mov    v26.16b, v20.16b\n"
        ".word 0x4f98e239  // sdot   v25.4s, v17.16b, v24.4b[0]\n"
        ".word 0x4f98e25a  // sdot   v26.4s, v18.16b, v24.4b[0]\n"
        ".word 0x4f97e0b9  // sdot   v25.4s, v5.16b, v23.4b[0]\n"
        ".word 0x4f97e0da  // sdot   v26.4s, v6.16b, v23.4b[0]\n"
        ".word 0x4f97e8f9  // sdot   v25.4s, v7.16b, v23.4b[2]\n"
        ".word 0x4f97ea1a  // sdot   v26.4s, v16.16b, v23.4b[2]\n"
        "ushr   v23.2d, v21.2d, #32\n"
        "sqrdmulh        v21.4s, v25.4s, v1.4s\n"
        "ushr   v24.2d, v22.2d, #32\n"
        "sqrdmulh        v22.4s, v26.4s, v1.4s\n"
        "sqrshl v21.4s, v21.4s, v2.4s\n"
        "sqrshl v22.4s, v22.4s, v2.4s\n"
        "sqxtn  v21.4h, v21.4s\n"
        "sqxtn2 v21.8h, v22.4s\n"
        "sqadd  v21.8h, v21.8h, v0.8h\n"
        "sqxtun v21.8b, v21.8h\n"
        "umax   v21.8b, v21.8b, v3.8b\n"
        "umin   v21.8b, v21.8b, v4.8b\n"
        "str    d21, [x19]\n"
        "add    x19, x19, x13\n"
        DC_KERNEL_MULT_STRIDE_10 ":\n"  // in Loop: Header=BB206_11 Depth=2
        "add    x21, x21, #1\n"  // =1
        "add    x20, x20, #4\n"  // =4
        "mov    v22.16b, v24.16b\n"
        "mov    v21.16b, v23.16b\n"
        DC_KERNEL_MULT_STRIDE_11 ":\n"  // Parent Loop BB206_13 Depth=1
        // =>  This Inner Loop Header: Depth=2
        "cmp    x21, x11\n"
        "b.lt   " DC_KERNEL_MULT_STRIDE_8 "b\n"
        DC_KERNEL_MULT_STRIDE_12 ":\n"  // in Loop: Header=BB206_13 Depth=1
        "add    x8, x8, #1\n"  // =1
        DC_KERNEL_MULT_STRIDE_13 ":\n"  // =>This Loop Header: Depth=1
        // Child Loop BB206_11 Depth 2
        // Child Loop BB206_4 Depth 2
        // Child Loop BB206_6 Depth 2
        "cmp    x8, x12\n"
        "b.lt   " DC_KERNEL_MULT_STRIDE_1 "b\n"
        :
        // Outputs.
        [ scratch_block_data ] "+r"(scratch_block_data),
        [ filter_workspace ] "+r"(filter_workspace),
        [ bias_data ] "+r"(bias_data),
        [ output_block_data ] "+r"(output_block_data)
        :
        // Inputs.
        [ function_params ] "r"(function_params)
        :
        // Clobbers.
        "cc", "memory",
        // We use these NEON registers.
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10",
        "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20",
        "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        // We use these general-purpose registers.
        "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
        "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25");
  }

#undef DC_KERNEL_MULT_STRIDE_1
#undef DC_KERNEL_MULT_STRIDE_2
#undef DC_KERNEL_MULT_STRIDE_3
#undef DC_KERNEL_MULT_STRIDE_4
#undef DC_KERNEL_MULT_STRIDE_5
#undef DC_KERNEL_MULT_STRIDE_6
#undef DC_KERNEL_MULT_STRIDE_7
#undef DC_KERNEL_MULT_STRIDE_8
#undef DC_KERNEL_MULT_STRIDE_9
#undef DC_KERNEL_MULT_STRIDE_10
#undef DC_KERNEL_MULT_STRIDE_11
#undef DC_KERNEL_MULT_STRIDE_12
#undef DC_KERNEL_MULT_STRIDE_13

  static void __attribute__((noinline))
  Run(const int8* scratch_block_data, const int8* filter_workspace,
      const int32* bias_data, uint8* output_block_data,
      const DepthwiseConvDotProdParams* function_params) {
    KernelMacroBlockNeon(scratch_block_data, filter_workspace, bias_data,
                         output_block_data, function_params);
  }
};

#undef DP_OFFSET_INPUT_DEPTH
#undef DP_OFFSET_OUTPUT_DEPTH
#undef DP_OFFSET_STRIDE
#undef DP_OFFSET_BIAS_INCREMENT
//
#undef DP_OFFSET_INPUT_OFFSET
#undef DP_OFFSET_OUTPUT_OFFSET
#undef DP_OFFSET_OUTPUT_MULTIPLIER
#undef DP_OFFSET_OUTPUT_SHIFT
#undef DP_OFFSET_QUANTIZED_ACTIVATION_MIN
#undef DP_OFFSET_QUANTIZED_ACTIVATION_MAX
//
#undef DP_OFFSET_PADDING_LEFT
#undef DP_OFFSET_PADDING_RIGHT
#undef DP_OFFSET_PADDING_TOP
#undef DP_OFFSET_PADDING_BOTTOM
//
#undef DP_OFFSET_DEPTH_MICRO_REPEATS
//
#undef DP_OFFSET_WIDTH_MACRO_COUNT
#undef DP_OFFSET_INPUT_WIDTH_OVERALL_MICRO_REPEATS
#undef DP_OFFSET_INPUT_WIDTH_MICRO_REPEATS
#undef DP_OFFSET_RESIDUAL_WIDTH
#undef DP_OFFSET_OUTPUT_WIDTH_OVERALL_MICRO_REPEATS
#undef DP_OFFSET_OUTPUT_WIDTH_MICRO_REPEATS
#undef DP_OFFSET_OUTPUT_RESIDUAL_WIDTH
#undef DP_OFFSET_WORKSPACE_WIDTH_MICRO_REPEATS
//
#undef DP_OFFSET_HEIGHT_MACRO_COUNT
#undef DP_OFFSET_INBOUND_BLOCK_HEIGHT
#undef DP_OFFSET_OUTBOUND_BLOCK_HEIGHT
#undef DP_OFFSET_INPUT_HEIGHT_STRIDE
#undef DP_OFFSET_OUTPUT_HEIGHT_STRIDE
#undef DP_OFFSET_WORKSPACE_HEIGHT_STRIDE
//
#undef DP_OFFSET_FOUR_OVER_STRIDE

#endif  // __aarch64__ && !GOOGLE_L4T - Dot product ops hard-coded

// Top-level implementation function for 3x3 depthwise convolution using NEON
// dot-product instructions.
//
// MACRO & MICRO BLOCKS
//
// The task is divided into macro blocks. Data is copied first into a macro
// block in a workspace. This has two purposes: (a) bringing data into
// cache, and (b) permuting data so that it can be used much more easily in
// a dot-product filter.
//
// When there is no depth multiplication:
//
// The permutations required for dot-products are local, within 4 data points
// down the depth and 4 across the width. We want to pull in input data at least
// 8-bytes at a time, down the depth, and so we divide the macro blocks into
// 1x4x8 (height, width, depth) and further divide the micro blocks into
// sub-blocks with shape (1x4x4).
//
// Each macro-block is constructed from micro-blocks that are internally
// rearranged during loading into the macro-block workspace.
//
// In other words, the micro-block shape is
//     {1, 1, 4, 8}
// Each macro block is typically shape
//     {1, height_block_size, 4 * workspace_width_micro_repeats, 64}
// and workspace_width_micro_repeats is chosen so it fits into the workspace.
//
// However, if depth < 64, we decrease the macro block depth, enabling us to
// increase the macro-block width.
//
// When there is depth multiplication:
//
// We require input-depth = 1 and exploit that instead.  Note that output data
// is still full-depth, *as is the filter and bias data after certain
// adjustments*, and so the filter stage in this case still proceeds in terms of
// sub-blocks.
//
// The Magic of these numbers:
//     4 is the number of input elements used in each dot-product.
//     8 is the number of inputs we load at a time into a register.
//     64 is min amount of data to be loaded in a stretch (when possible).
//
// FILTER DATA PREPARATION
//
// Filter data needs to be permuted in a fashion like that of input data, and
// this is done in a preprocessing stage. In addition, this stage extends the
// filter in the direction of width from 3 to 4. The extra filter taps are set
// to zero so that input data does not have to be zeroed before applying
// dot-products.
//
// OVERALL COUNTS: HANDLING TRAILING ITERATION
//
// Often it is necessary to handle the last iteration in a loop differently,
// generally because the final item is shorter. The logic to detect the
// special case can be a bit expensive. We use a scheme in which there are
// two counts, in a pattern like xxx_yyy_repeats and
// xxx_overall_yyy_repeats. The first gives the count of "normal"
// iterations. The loop iterates over the second count, and the induction
// variable is checked to see if it reaches xxx_yyy_repeats. If there is no
// special trailing iteration, xxx_yyy_repeats = xxx_overall_yyy_repeats,
// and the special code is not executed.
//
// Example:
// Suppose that we characterize a size s as
// f(s) -> (block-4-repetitions, remainder, overall_repetitions):
// f(11) -> (2, 3, 3)
// f(12) -> (3, 0, 3)
// f(13) -> (3, 1, 4)
//
// POINTING OUTSIDE OF INPUT ARRAY.
//
// When there is padding, the input data pointer passed to the fill routines
// points outside of the input array and into a kind-of virtual padded
// margin. It turns out that this simplifies the code and removes
// conditional statements. It is hard to explain why without comparing two
// versions of the code. In summary, this way the adjustment into the margin
// can be made unconditionally, and the correction back into the input array
// is done where there is a conditional already.
//
// OVERLAP
//
// Since this is *depthwise* conv, neither the batch nor the depth have overlap.
// The height and depth overlap by (filter_size - 1). Thus some data is used
// twice on the borders of macro blocks.
//
template <DepthwiseConvImplementation implementation>
inline void DepthwiseConvDotProduct3x3(
    const DepthwiseParams& params, const RuntimeShape& input_shape,
    const uint8* input_data, const RuntimeShape& filter_shape,
    const uint8* filter_data, const RuntimeShape& bias_shape,
    const int32* bias_data, const RuntimeShape& output_shape,
    uint8* output_data) {
  // Check kernel restrictions.
  constexpr int filter_size = 3;
  constexpr int kMaxStride = 2;
  constexpr int kMaxPadding = 1;
  constexpr int kSymmetricZeroPoint = 128;
  TFLITE_DCHECK_EQ(params.weights_offset, -kSymmetricZeroPoint);
  TFLITE_DCHECK_LE(params.stride_width, kMaxStride);
  TFLITE_DCHECK_EQ(params.stride_height, params.stride_width);
  TFLITE_DCHECK_EQ(params.dilation_width_factor, 1);
  TFLITE_DCHECK_EQ(params.dilation_height_factor, 1);
  TFLITE_DCHECK_LE(params.padding_values.width, kMaxPadding);
  TFLITE_DCHECK_LE(params.padding_values.height, kMaxPadding);
  TFLITE_DCHECK_EQ(input_shape.DimensionsCount(), 4);
  TFLITE_DCHECK_EQ(filter_shape.DimensionsCount(), 4);
  TFLITE_DCHECK_EQ(output_shape.DimensionsCount(), 4);
  TFLITE_DCHECK_LE(params.quantized_activation_min,
                   params.quantized_activation_max);

  // Key kernel parameters (along with padding handled later).
  const int stride = params.stride_width;
  const int depth_multiplier = params.depth_multiplier;
  const bool has_depth_multiplication = depth_multiplier > 1;

  // Extract task dimensions.
  const int input_depth = input_shape.Dims(3);
  const int output_depth = MatchingDim(filter_shape, 3, output_shape, 3);
  const int input_height = input_shape.Dims(1);
  const int input_width = input_shape.Dims(2);
  const int output_height = output_shape.Dims(1);
  const int output_width = output_shape.Dims(2);
  const int batches = MatchingDim(input_shape, 0, output_shape, 0);
  TFLITE_DCHECK(!has_depth_multiplication || input_depth == 1);
  TFLITE_DCHECK(has_depth_multiplication || input_depth == output_depth);
  TFLITE_DCHECK_EQ(bias_shape.FlatSize(), output_depth);
  TFLITE_DCHECK_EQ(input_depth * depth_multiplier, output_depth);
  TFLITE_DCHECK_EQ(MatchingDim(filter_shape, 1, filter_shape, 2), filter_size);

  // Return now if nothing to do.
  if (output_width == 0 || output_height == 0) {
    return;
  }

  // Kernel parameter structure: set basic fields.
  //
  // In asm it is easier to pass a structure than more than, say, 8 parameters.
  DepthwiseConvDotProdParams function_params;
  function_params.input_depth = input_depth;
  function_params.output_depth = output_depth;
  function_params.input_offset = params.input_offset;
  function_params.output_offset = params.output_offset;
  function_params.output_multiplier = params.output_multiplier;
  function_params.output_shift = params.output_shift;
  function_params.quantized_activation_min = params.quantized_activation_min;
  function_params.quantized_activation_max = params.quantized_activation_max;
  function_params.stride = stride;

  // Handle inbound bias data.
  //
  // Note that this data is adjusted in a per-depth process before the main
  // filters. The adjustment accounts for a non-symmetric input offset.
  //
  // Kernel subroutines need to be able to operate consistently on an bias
  // array. Where there is no bias, we provide one filled with zeros.
  constexpr int kMinBiasLoad = 8;
  int32 zero_bias_data[kMinBiasLoad];
  int32 bias_increment;
  if (bias_data) {
    bias_increment = 4;
  } else {
    memset(zero_bias_data, 0, sizeof(zero_bias_data));
    bias_data = &zero_bias_data[0];
    bias_increment = 0;
  }
  function_params.bias_increment = bias_increment;
  TFLITE_DCHECK_LE(2 * function_params.bias_increment, kMinBiasLoad);

  // Process padding.
  //
  // Whether "correct" or not, this matches ComputeConvSizes. When there is
  // stride > 1 there can be padding on the bottom or top, and therefore
  // we need to consider padding. This is true even if one or other of the
  // padding_values is 0.
  const int padded_width = (output_width - 1) * stride + filter_size;
  {
    const int padding_left = params.padding_values.width;
    // Right padding would be -1 if discarding input because of stride.
    const int padding_right =
        std::max(padded_width - input_width - padding_left, 0);
    const int padding_top = params.padding_values.height;
    const int padded_height = (output_height - 1) * stride + filter_size;
    const int padding_bottom =
        std::max(padded_height - input_height - padding_top, 0);

    function_params.padding_left = padding_left;
    function_params.padding_right = padding_right;
    function_params.padding_top = padding_top;
    function_params.padding_bottom = padding_bottom;

    TFLITE_DCHECK_LE(padding_left, padding_right);
    TFLITE_DCHECK_LE(padding_top, padding_bottom);
  }
  // When stride == 1 left or top padding may only be non-zero.
  // This is when padding is specified but not needed on a trailing dimension.
  // When stride == 2 right or bottom padding may only be non-zero.
  // This is a result of the details of the padding calculations.
  const bool padding_required =
      function_params.padding_left > 0 || function_params.padding_top > 0 ||
      function_params.padding_right > 0 || function_params.padding_bottom > 0;

  // Choose parameter-specific kernel subroutines.
  //
  // The main part of the kernel has two stages. First, a temporary workspace is
  // filled with padded and permuted data. Second, the filter is applied to the
  // workspace data to generate output.
  //
  // The workspace fill stage handles padding so that the filter stage does not
  // need to account for it. The workspace fill stage does not need to
  // understand striding, and implicitly handles striding through the parameters
  // that it is given.
  using pack_macro_block_func_t = decltype(
      &PackMacroBlock<implementation,
                      DepthwiseConvDepthMultiplication::kNoMultiplication,
                      0>::Run);
  using kernel_macro_block_func_t = decltype(
      &KernelMacroBlock<implementation,
                        DepthwiseConvDepthMultiplication::kNoMultiplication,
                        1>::Run);
  pack_macro_block_func_t pack_macro_block_func;
  kernel_macro_block_func_t kernel_macro_block_func;
  {
    if (has_depth_multiplication) {
      if (padding_required) {
        pack_macro_block_func =
            PackMacroBlock<implementation,
                           DepthwiseConvDepthMultiplication::kUnitInputDepth,
                           /*max_padding=*/1>::Run;
      } else {
        pack_macro_block_func =
            PackMacroBlock<implementation,
                           DepthwiseConvDepthMultiplication::kUnitInputDepth,
                           /*max_padding=*/0>::Run;
      }
      if (stride == 1) {
        kernel_macro_block_func =
            KernelMacroBlock<implementation,
                             DepthwiseConvDepthMultiplication::kUnitInputDepth,
                             /*stride=*/1>::Run;
      } else {
        kernel_macro_block_func =
            KernelMacroBlock<implementation,
                             DepthwiseConvDepthMultiplication::kUnitInputDepth,
                             /*stride=*/2>::Run;
      }
    } else {
      if (padding_required) {
        pack_macro_block_func =
            PackMacroBlock<implementation,
                           DepthwiseConvDepthMultiplication::kNoMultiplication,
                           /*max_padding=*/1>::Run;
      } else {
        pack_macro_block_func =
            PackMacroBlock<implementation,
                           DepthwiseConvDepthMultiplication::kNoMultiplication,
                           /*max_padding=*/0>::Run;
      }
      if (stride == 1) {
        kernel_macro_block_func = KernelMacroBlock<
            implementation, DepthwiseConvDepthMultiplication::kNoMultiplication,
            /*stride=*/1>::Run;
      } else {
        kernel_macro_block_func = KernelMacroBlock<
            implementation, DepthwiseConvDepthMultiplication::kNoMultiplication,
            /*stride=*/2>::Run;
      }
    }
  }

  // Stride-only variables.
  //
  // stride == 1 ? 4 : 2:
  const int output_height_per_macro = 6 - 2 * stride;
  // output_height_per_macro * stride:
  constexpr int input_height_per_macro = 4;
  // Number of rows per micro block (= rows per macro block) is
  //   (output_height_per_macro - 1) * stride + 1 + (filter_size - 1)
  //   = stride == 1 ? 3 + filter_size : 2 + filter_size:
  const int height_block_size = 4 + filter_size - stride;
  const int input_height_overlap = filter_size - stride;
  // stride == 1 ? 4 : 2:
  function_params.four_over_stride = output_height_per_macro;

  TFLITE_DCHECK_EQ(stride * function_params.four_over_stride, 4);
  TFLITE_DCHECK_EQ(height_block_size,
                   input_height_per_macro + input_height_overlap);

  // Create workspaces.
  //
  // Filter workspace is for shuffle: only first depth/8 is used.
  // indexed as [depth/8][sub-block][height][depth][width].
  TFLITE_DCHECK_EQ(kDepthwiseConvAdjustedBiasLimit % 8, 0);
  int8 macroblock_workspace[kDepthwiseConvScratchWorkspaceSize];
  int32 adjusted_bias_data[kDepthwiseConvAdjustedBiasLimit];
  int8 filter_workspace[kDepthwiseConvAdjustedBiasLimit >> 3][3][2][4][4];

  // Output depth characterization.
  //
  const int depth_macro_count = output_depth / 64;
  const int depth_overall_macro_count = (output_depth + 63) / 64;
  // Number of micro blocks down the depth in a final incomplete macro block.
  const int depth_trailing_micro_repeats = output_depth / 8 % 8;
  // The output_depth may not have a remainder: it must be a multiple of 8.
  TFLITE_DCHECK_EQ(output_depth,
                   64 * depth_macro_count + 8 * depth_trailing_micro_repeats);

  // Characterize the first macro block depth, the largest.
  //
  // We base treatment of the width on the trailing macro block if there are
  // no full blocks, in order to do more work together (that is, increase
  // workspace_width_micro_repeats when largest_macro_depth < 64).
  const int largest_macro_depth =
      has_depth_multiplication
          ? 1
          : (depth_macro_count > 0 ? 64 : 8 * depth_trailing_micro_repeats);

  // Characterize width, consumption of input and generation of output.
  //
  // In the case of depth multiplication, we ensure that some of the workspace
  // at the end remains unused. This enables the filter routines to load the
  // "next" data, of at least 16 bytes, even when at the end of the workspace.
  // It is relatively expensive to detect the end micro block. It is also very
  // difficult to test for (to trigger) erroneous reads (past end of array) in
  // the depth multplication case.
  int workspace_width_micro_repeats =
      (has_depth_multiplication
           ? kDepthwiseConvScratchWorkspaceSize - kWorkspaceExtension
           : kDepthwiseConvScratchWorkspaceSize) /
      (4 * largest_macro_depth * height_block_size);
  // When there is no depth multiplication, the workspace depth is a multiple of
  // 8, which ensures that workspace rows are 16-byte aligned. (Actually 32,
  // because of the micro width of 4.) This is not necessarily the case under
  // depth multiplication, so we adjust now to impose this restriction.
  if (has_depth_multiplication) {
    workspace_width_micro_repeats = (workspace_width_micro_repeats / 4) * 4;
  }
  TFLITE_DCHECK_EQ((workspace_width_micro_repeats * largest_macro_depth) % 4,
                   0);
  // Discount 1 of the micro-block repeats in each macro block to account for
  // overlap.
  const int consumed_width_per_macro_block =
      4 * (workspace_width_micro_repeats - 1);
  const int output_width_per_macro_block =
      function_params.four_over_stride * (workspace_width_micro_repeats - 1);
  TFLITE_DCHECK_GT(workspace_width_micro_repeats, 1);
  TFLITE_DCHECK_EQ(output_width_per_macro_block * stride,
                   consumed_width_per_macro_block);

  // Width repetitions and residuals.
  //
  // Use of the workspace is characterized primarily in terms of *padded input*.
  // Striding only matters in a few places.
  //
  // Simplifications: We require that there always be at least one full
  // micro-block across the width. Since the maximum padding is 1, the trailing
  // padding cannot span two micro blocks.
  const int residual_micro_width = padded_width % 4;
  // We base the count of macro blocks on the amount of padded input data each
  // one consumes.
  int width_overall_macro_count = (padded_width - residual_micro_width +
                                   consumed_width_per_macro_block - 1) /
                                  consumed_width_per_macro_block;
  // Recall that we left a micro block at the end of each macro block for use as
  // overlap. There is a special case in which we can use one fewer macro
  // blocks, with the last one consuming extra input. (But not if the
  // calculation thinks that we can use zero blocks.)
  if (padded_width <=
      ((width_overall_macro_count - 1) * consumed_width_per_macro_block + 4)) {
    width_overall_macro_count -= 1;
  }
  width_overall_macro_count = std::max(width_overall_macro_count, 1);
  // We always have to treat the final macro block along width as trailing,
  // because even if it is full in terms of padded input, it will be incomplete
  // in terms of output.
  const int width_macro_count = width_overall_macro_count - 1;
  // Micro blocks are traversed in terms of input in fill routines.
  const int width_trailing_micro_repeats =
      (padded_width - consumed_width_per_macro_block * width_macro_count) / 4;
  const int width_overall_trailing_micro_repeats =
      (padded_width - consumed_width_per_macro_block * width_macro_count + 3) /
      4;
  // Micro blocks are traversed in terms of output in filtering routines.
  const int residual_output_micro_width =
      (output_width - 1) % function_params.four_over_stride + 1;
  const int output_width_trailing_micro_repeats =
      residual_micro_width > (filter_size - 1)
          ? width_trailing_micro_repeats
          : width_trailing_micro_repeats - 1;
  // Check results.
  TFLITE_DCHECK_GT(width_overall_trailing_micro_repeats, 0);
  TFLITE_DCHECK_EQ(padded_width,
                   residual_micro_width +
                       consumed_width_per_macro_block * width_macro_count +
                       4 * width_trailing_micro_repeats);
  TFLITE_DCHECK_LE(width_overall_macro_count, width_macro_count + 1);
  TFLITE_DCHECK_GE(width_overall_macro_count, width_macro_count);

  // Height repetitions and residuals.
  //
  const int height_macro_count = output_height / output_height_per_macro;
  const int residual_output_height = output_height % output_height_per_macro;
  const int height_overall_macro_count =
      (output_height + output_height_per_macro - 1) / output_height_per_macro;
  TFLITE_DCHECK_EQ(
      output_height,
      residual_output_height + output_height_per_macro * height_macro_count);
  TFLITE_DCHECK_LE(height_overall_macro_count, height_macro_count + 1);
  TFLITE_DCHECK_GE(height_overall_macro_count, height_macro_count);

  // Data strides.
  //
  const int input_height_stride = input_width * input_depth;
  const int output_height_stride = output_width * output_depth;
  const int input_batch_stride = input_height_stride * input_height;
  const int output_batch_stride = output_height_stride * output_height;
  const int input_depth_macro_stride = has_depth_multiplication ? 0 : 64;
  const int input_width_macro_stride =
      input_depth * consumed_width_per_macro_block;
  const int output_width_macro_stride =
      output_depth * output_width_per_macro_block;

  // Store parameters that do not vary across macro blocks.
  //
  function_params.workspace_width_micro_repeats = workspace_width_micro_repeats;
  function_params.height_macro_count = height_overall_macro_count;
  function_params.width_macro_count = width_overall_macro_count;
  function_params.input_height_stride = input_height_stride;
  function_params.output_height_stride = output_height_stride;
  function_params.residual_width = residual_micro_width;

  // Prefetch workspace for write, along with any necessary dummy writes.
  const int max_workspace_height_stride =
      16 * ((workspace_width_micro_repeats + 3) >> 2) * largest_macro_depth;
  const int workspace_fill_size = std::min(
      kDepthwiseConvScratchWorkspaceSize,
      height_block_size * max_workspace_height_stride + kWorkspaceExtension);
  WorkspacePrefetchWrite<implementation>::Run(
      params.weights_offset, workspace_fill_size, macroblock_workspace);

  // Main process.
  //
  // Most kernels are nested batch-height-width-depth. Here we proceed over
  // macro blocks batch-width-depth-height.
  //
  // Example of handling of trailing iteration: when there is trailing depth,
  // depth_overall_macro_count = depth_macro_count + 1, so we can adjust the
  // dimensions for trailing macro blocks by looking for
  // j_depth == depth_macro_count.
  for (int b = 0; b < batches; ++b) {
    for (int k_width = 0; k_width < width_overall_macro_count; ++k_width) {
      // Figure out the work to be done for this macro block. If it trails in
      // any dimension, the work in that dimension is adjusted.
      // The work to be done across widths has 3 cases:
      // (a) A full macro block,
      // (b) Partial terminal macro block, with input and output ending in
      //     same micro block, and
      // (c) Partial terminal macro block, with output corresponding to one
      //     fewer micro blocks, because filter extends across micro-block
      //     boundary.
      if (k_width != width_macro_count) {
        function_params.output_residual_width = 0;
        function_params.input_width_micro_repeats =
            workspace_width_micro_repeats;
        function_params.input_width_overall_micro_repeats =
            workspace_width_micro_repeats;
        function_params.output_width_micro_repeats =
            workspace_width_micro_repeats - 1;
      } else {
        function_params.output_residual_width = residual_output_micro_width;
        function_params.input_width_micro_repeats =
            width_trailing_micro_repeats;
        function_params.input_width_overall_micro_repeats =
            width_overall_trailing_micro_repeats;
        function_params.output_width_micro_repeats =
            output_width_trailing_micro_repeats;
      }
      function_params.output_width_overall_micro_repeats =
          function_params.output_residual_width == 0
              ? function_params.output_width_micro_repeats
              : function_params.output_width_micro_repeats + 1;

      for (int j_depth = 0; j_depth < depth_overall_macro_count; ++j_depth) {
        // Process filter and bias data.
        //
        function_params.depth_micro_repeats =
            j_depth == depth_macro_count ? depth_trailing_micro_repeats : 8;
        ProcessPerDepth<implementation>::Run(
            filter_data + 64 * j_depth,
            bias_data + 8 * 2 * bias_increment * j_depth,
            filter_workspace[0][0][0][0], adjusted_bias_data, &function_params);

        const uint8* input_data_block =
            input_data + b * input_batch_stride +
            j_depth * input_depth_macro_stride +
            k_width * input_width_macro_stride -
            function_params.padding_left * input_depth -
            function_params.padding_top * input_height_stride;
        uint8* output_data_block = output_data + b * output_batch_stride +
                                   j_depth * 64 +
                                   k_width * output_width_macro_stride;

        // Under depth multiplication the workspace_height_stride does not have
        // to depend on input_width_overall_micro_repeats, but this improves the
        // compactness of workspace use.
        const int workspace_height_stride =
            has_depth_multiplication
                ? 16 * ((function_params.input_width_overall_micro_repeats +
                         3) >>
                        2)
                : 4 * function_params.input_width_overall_micro_repeats * 8 *
                      function_params.depth_micro_repeats;
        TFLITE_DCHECK_EQ(workspace_height_stride % 16, 0);
        function_params.workspace_height_stride = workspace_height_stride;

        // For the first macro block for output rows we fill in the first few
        // rows.  After this we will copy them (see below in loop.)
        function_params.inbound_block_height = input_height_overlap;
        pack_macro_block_func(-1, k_width, input_data_block,
                              macroblock_workspace, &function_params);
        input_data_block += input_height_stride * input_height_overlap;

        for (int i_height = 0; i_height < height_overall_macro_count;
             ++i_height) {
          if (i_height != height_macro_count) {
            function_params.inbound_block_height = input_height_per_macro;
            function_params.outbound_block_height = output_height_per_macro;
          } else {
            function_params.inbound_block_height =
                residual_output_height * stride;
            function_params.outbound_block_height = residual_output_height;
          }
          TFLITE_DCHECK_LT(i_height * output_height_per_macro, output_height);
          TFLITE_DCHECK_LT(i_height * input_height_per_macro, input_height);
          TFLITE_DCHECK_LT(k_width * output_width_per_macro_block,
                           output_width);
          TFLITE_DCHECK_LT(k_width * consumed_width_per_macro_block,
                           input_width);

          // Macro blocks overlap by input_height_overlap rows, so we copy
          // those instead of filling in afresh.  The first macro block across
          // output rows was filled in outside of the loop (above).
          if (i_height > 0) {
            memcpy(macroblock_workspace,
                   macroblock_workspace +
                       input_height_per_macro * workspace_height_stride,
                   input_height_overlap * workspace_height_stride);
          }

          pack_macro_block_func(
              i_height, k_width, input_data_block,
              macroblock_workspace +
                  input_height_overlap * workspace_height_stride,
              &function_params);

          kernel_macro_block_func(
              macroblock_workspace, filter_workspace[0][0][0][0],
              adjusted_bias_data, output_data_block, &function_params);

          input_data_block += input_height_stride * input_height_per_macro;
          output_data_block += output_height_stride * output_height_per_macro;
        }
      }
    }
  }
}

#undef vst1_lane_8x4
#undef vst1q_lane_8x4
#undef vld1q_lane_s8x8
#undef vld1_lane_8x4
#undef vld1q_lane_8x4
#undef vld1q_dup_s8x4

#undef STR
#undef STR_UNEXPANDED

}  // namespace depthwise_conv
}  // namespace optimized_ops
}  // namespace tflite

#endif  // TENSORFLOW_LITE_KERNELS_INTERNAL_OPTIMIZED_DEPTHWISECONV_UINT8_3X3_FILTER_H_
