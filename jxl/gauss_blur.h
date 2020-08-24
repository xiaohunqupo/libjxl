// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef JXL_GAUSS_BLUR_H_
#define JXL_GAUSS_BLUR_H_

#include <stddef.h>

#include <cmath>
#include <hwy/aligned_allocator.h>
#include <vector>

#include "jxl/base/data_parallel.h"
#include "jxl/base/status.h"
#include "jxl/image.h"

namespace jxl {

template <typename T>
std::vector<T> GaussianKernel(int radius, T sigma) {
  JXL_ASSERT(sigma > 0.0);
  std::vector<T> kernel(2 * radius + 1);
  const T scaler = -1.0 / (2 * sigma * sigma);
  double sum = 0.0;
  for (int i = -radius; i <= radius; ++i) {
    const T val = std::exp(scaler * i * i);
    kernel[i + radius] = val;
    sum += val;
  }
  for (size_t i = 0; i < kernel.size(); ++i) {
    kernel[i] /= sum;
  }
  return kernel;
}

// All convolution functions below apply mirroring of the input on the borders
// in the following way:
//
//     input: [a0 a1 a2 ...  aN]
//     mirrored input: [aR ... a1 | a0 a1 a2 .... aN | aN-1 ... aN-R]
//
// where R is the radius of the kernel (i.e. kernel size is 2*R+1).

// REQUIRES: in.xsize() and in.ysize() are integer multiples of res.
ImageF ConvolveAndSample(const ImageF& in, const std::vector<float>& kernel,
                         const size_t res);

// Private, used by test.
void ExtrapolateBorders(const float* const JXL_RESTRICT row_in,
                        float* const JXL_RESTRICT row_out, const int xsize,
                        const int radius);

// Only for use by CreateRecursiveGaussian and FastGaussian*.
#pragma pack(push, 1)
struct RecursiveGaussian {
  // For k={1,3,5} in that order, each broadcasted 4x for LoadDup128. Used only
  // for vertical passes.
  float n2[3 * 4];
  float d1[3 * 4];

  // We unroll horizontal passes 4x - one output per lane. These are each lane's
  // multiplier for the previous output (relative to the first of the four
  // outputs). Indexing: 4 * 0..2 (for {1,3,5}) + 0..3 for the lane index.
  float mul_prev[3 * 4];
  // Ditto for the second to last output.
  float mul_prev2[3 * 4];

  // We multiply a vector of inputs 0..3 by a vector shifted from this array.
  // in=0 uses all 4 (nonzero) terms; for in=3, the lower three lanes are 0.
  float mul_in[3 * 4];

  size_t radius;
};
#pragma pack(pop)

// Precomputation for FastGaussian*; users may use the same pointer/storage in
// subsequent calls to FastGaussian* with the same sigma.
hwy::AlignedUniquePtr<RecursiveGaussian> CreateRecursiveGaussian(double sigma);

// 1D Gaussian with zero-pad boundary handling and runtime independent of sigma.
void FastGaussian1D(const hwy::AlignedUniquePtr<RecursiveGaussian>& rg,
                    const float* JXL_RESTRICT in, intptr_t width,
                    float* JXL_RESTRICT out);

// 2D Gaussian with zero-pad boundary handling and runtime independent of sigma.
void FastGaussian(const hwy::AlignedUniquePtr<RecursiveGaussian>& rg,
                  const ImageF& in, ThreadPool* pool, ImageF* JXL_RESTRICT temp,
                  ImageF* JXL_RESTRICT out);

}  // namespace jxl

#endif  // JXL_GAUSS_BLUR_H_
