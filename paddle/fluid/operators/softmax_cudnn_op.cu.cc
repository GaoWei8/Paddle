/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/fluid/operators/math/softmax.h"
#include "paddle/fluid/framework/op_registry.h"
#include "paddle/fluid/operators/softmax_op.h"
#include "paddle/fluid/platform/cudnn_desc.h"
#include "paddle/fluid/platform/cudnn_helper.h"

namespace paddle {
namespace operators {

using Tensor = framework::Tensor;

template <typename T>
class SoftmaxCUDNNKernel : public framework::OpKernel<T> {
 public:
  void Compute(const framework::ExecutionContext& ctx) const override {
    auto* X = ctx.Input<Tensor>("X");
    auto* Out = ctx.Output<Tensor>("Out");
    //    auto axis = ctx.Attr<int>("axis");
    auto dims = X->dims();
    // const int N = SizeToAxis(axis, dims);
    // const int D = SizeFromAxis(axis, dims);

    // allocate memory on device.
    Out->mutable_data<T>(ctx.GetPlace());
    // Tensor x = *X;
    // x.Resize({N, D, 1, 1});
    auto* out_data = Out->data<T>();
    cudnnTensorDescriptor_t desc_;
    cudnnDataType_t type = platform::ToCudnnDataType(
        framework::ToDataType(std::type_index(typeid(T))));

    PADDLE_ENFORCE_CUDA_SUCCESS(
        platform::dynload::cudnnCreateTensorDescriptor(&desc_));
    PADDLE_ENFORCE_CUDA_SUCCESS(platform::dynload::cudnnSetTensor4dDescriptor(
        desc_, CUDNN_TENSOR_NCHW, type, dims[0], dims[1], dims[2], dims[3]));

    auto& dev_ctx = ctx.template device_context<platform::CUDADeviceContext>();
    auto handle = dev_ctx.cudnn_handle();

    PADDLE_ENFORCE_CUDA_SUCCESS(platform::dynload::cudnnSoftmaxForward(
        handle, CUDNN_SOFTMAX_ACCURATE, CUDNN_SOFTMAX_MODE_INSTANCE,
        platform::CudnnDataType<T>::kOne(), desc_, X->data<T>(),
        platform::CudnnDataType<T>::kZero(), desc_, out_data));
  }
};

template <typename T>
class SoftmaxGradCUDNNKernel : public framework::OpKernel<T> {
 public:
  void Compute(const framework::ExecutionContext& ctx) const override {
    auto* Out = ctx.Input<Tensor>("Out");
    auto* dOut = ctx.Input<Tensor>(framework::GradVarName("Out"));
    auto* dX = ctx.Output<Tensor>(framework::GradVarName("X"));

    //  auto axis = ctx.Attr<int>("axis");
    auto dims = Out->dims();
    // const int N = SizeToAxis(axis, dims);
    // const int D = SizeFromAxis(axis, dims);
    dX->mutable_data<T>(ctx.GetPlace());
    auto* dx_data = dX->data<T>();
    cudnnTensorDescriptor_t desc_;
    cudnnDataType_t type = platform::ToCudnnDataType(
        framework::ToDataType(std::type_index(typeid(T))));

    PADDLE_ENFORCE_CUDA_SUCCESS(
        platform::dynload::cudnnCreateTensorDescriptor(&desc_));
    PADDLE_ENFORCE_CUDA_SUCCESS(platform::dynload::cudnnSetTensor4dDescriptor(
        desc_, CUDNN_TENSOR_NCHW, type, dims[0], dims[1], dims[2], dims[3]));

    auto& dev_ctx = ctx.template device_context<platform::CUDADeviceContext>();
    auto handle = dev_ctx.cudnn_handle();

    PADDLE_ENFORCE_CUDA_SUCCESS(platform::dynload::cudnnSoftmaxBackward(
        handle, CUDNN_SOFTMAX_ACCURATE, CUDNN_SOFTMAX_MODE_INSTANCE,
        platform::CudnnDataType<T>::kOne(), desc_, Out->data<T>(), desc_,
        dOut->data<T>(), platform::CudnnDataType<T>::kZero(), desc_, dx_data));
  }
};

}  // namespace operators
}  // namespace paddle

namespace ops = paddle::operators;
namespace plat = paddle::platform;
REGISTER_OP_KERNEL(softmax, CUDNN, plat::CUDAPlace,
                   ops::SoftmaxCUDNNKernel<float>,
                   ops::SoftmaxCUDNNKernel<double>,
                   ops::SoftmaxCUDNNKernel<plat::float16>);
REGISTER_OP_KERNEL(softmax_grad, CUDNN, plat::CUDAPlace,
                   ops::SoftmaxGradCUDNNKernel<float>,
                   ops::SoftmaxGradCUDNNKernel<double>,
                   ops::SoftmaxGradCUDNNKernel<plat::float16>);
