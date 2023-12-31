/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "tensorflow/compiler/tf2xla/shape_util.h"
#include "tensorflow/compiler/tf2xla/xla_op_kernel.h"
#include "tensorflow/compiler/tf2xla/xla_op_registry.h"
#include "xla/client/lib/constants.h"
#include "xla/client/xla_builder.h"
#include "tensorflow/core/framework/kernel_def_builder.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/tpu/tpu_defs.h"

namespace tensorflow {

class TpuCustomResizeOp : public XlaOpKernel {
 public:
  explicit TpuCustomResizeOp(OpKernelConstruction* ctx) : XlaOpKernel(ctx) {
    OP_REQUIRES_OK(ctx, ctx->GetAttr("align_corners", &align_corners_));
    OP_REQUIRES_OK(ctx,
                   ctx->GetAttr("half_pixel_centers", &half_pixel_centers_));
  }

  xla::Shape GetOutputShape(XlaOpKernelContext* ctx) const {
    std::vector<int64_t> out_size;
    auto status = ctx->ConstantInputAsIntVector(1, &out_size);
    CHECK_EQ(out_size.size(), 2) << status;
    xla::Shape output_shape =
        TensorShapeToXLAShape(ctx->output_xla_type(0), ctx->InputShape(0));
    output_shape.mutable_dimensions()[1] = out_size[0];
    output_shape.mutable_dimensions()[2] = out_size[1];
    return output_shape;
  }

  string OpaqueField() const {
    return absl::StrCat("\"", align_corners_, half_pixel_centers_, "\"");
  }

  void CompileGrad(XlaOpKernelContext* ctx, const char* target,
                   const xla::Shape& output_shape) {
    auto input_shape =
        TensorShapeToXLAShape(ctx->output_xla_type(0), ctx->InputShape(0));
    if (ctx->InputShape(1).dim_sizes() == ctx->InputShape(0).dim_sizes()) {
      ctx->SetOutput(
          0, xla::ConvertElementType(ctx->Input(0), ctx->output_xla_type(0)));
      return;
    }
    // The gradient should be done in two phases for large resizes.
    auto input = ctx->Input(0);
    if (input_shape.dimensions(1) / output_shape.dimensions(1) > 3 &&
        input_shape.dimensions(2) / output_shape.dimensions(2) > 3) {
      auto intermediate_shape = output_shape;
      intermediate_shape.mutable_dimensions()[1] = input_shape.dimensions(1);
      input = xla::CustomCall(ctx->builder(), target, {ctx->Input(0)},
                              intermediate_shape, OpaqueField());
    }
    ctx->SetOutput(0, xla::CustomCall(ctx->builder(), target, {input},
                                      output_shape, OpaqueField()));
  }

  void CompileForward(XlaOpKernelContext* ctx, const char* target) {
    auto output_shape = GetOutputShape(ctx);
    if (ctx->InputShape(0).dim_size(1) == output_shape.dimensions(1) &&
        ctx->InputShape(0).dim_size(2) == output_shape.dimensions(2)) {
      ctx->SetOutput(
          0, xla::ConvertElementType(ctx->Input(0), ctx->output_xla_type(0)));
      return;
    }
    if (ctx->InputShape(0).dim_size(1) == 1 &&
        ctx->InputShape(0).dim_size(2) == 1) {
      ctx->SetOutput(0,
                     ctx->Input(0) + xla::Zeros(ctx->builder(), output_shape));
      return;
    }
    ctx->SetOutput(0, xla::CustomCall(ctx->builder(), target, {ctx->Input(0)},
                                      output_shape, OpaqueField()));
  }

 private:
  bool align_corners_;
  bool half_pixel_centers_;
};

class TpuResizeNearestNeighborOp : public TpuCustomResizeOp {
 public:
  explicit TpuResizeNearestNeighborOp(OpKernelConstruction* ctx)
      : TpuCustomResizeOp(ctx) {}
  void Compile(XlaOpKernelContext* ctx) override {
    CompileForward(ctx, "ResizeNearest");
  }
};

class TpuResizeBilinearOp : public TpuCustomResizeOp {
 public:
  explicit TpuResizeBilinearOp(OpKernelConstruction* ctx)
      : TpuCustomResizeOp(ctx) {}
  void Compile(XlaOpKernelContext* ctx) override {
    CompileForward(ctx, "ResizeBilinear");
  }
};

class TpuResizeNearestNeighborGradOp : public TpuCustomResizeOp {
 public:
  explicit TpuResizeNearestNeighborGradOp(OpKernelConstruction* ctx)
      : TpuCustomResizeOp(ctx) {}
  void Compile(XlaOpKernelContext* ctx) override {
    CompileGrad(ctx, "ResizeNearestGrad", GetOutputShape(ctx));
  }
};

class TpuResizeBilinearGradOp : public TpuCustomResizeOp {
 public:
  explicit TpuResizeBilinearGradOp(OpKernelConstruction* ctx)
      : TpuCustomResizeOp(ctx) {}
  void Compile(XlaOpKernelContext* ctx) override {
    auto output_shape =
        TensorShapeToXLAShape(ctx->output_xla_type(0), ctx->InputShape(1));
    CompileGrad(ctx, "ResizeBilinearGrad", output_shape);
  }
};

REGISTER_XLA_OP(Name("ResizeNearestNeighbor")
                    .CompileTimeConstantInput("size")
                    .Device(DEVICE_TPU_XLA_JIT),
                TpuResizeNearestNeighborOp);

REGISTER_XLA_OP(Name("ResizeNearestNeighborGrad")
                    .CompileTimeConstantInput("size")
                    .Device(DEVICE_TPU_XLA_JIT),
                TpuResizeNearestNeighborGradOp);

REGISTER_XLA_OP(Name("ResizeBilinear")
                    .CompileTimeConstantInput("size")
                    .Device(DEVICE_TPU_XLA_JIT),
                TpuResizeBilinearOp);

REGISTER_XLA_OP(Name("ResizeBilinearGrad").Device(DEVICE_TPU_XLA_JIT),
                TpuResizeBilinearGradOp);

}  // namespace tensorflow
