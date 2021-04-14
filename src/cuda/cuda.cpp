////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Lawrence Livermore National Security, LLC.  Produced at the
// Lawrence Livermore National Laboratory in collaboration with University of
// Illinois Urbana-Champaign.
//
// Written by the LBANN Research Team (N. Dryden, N. Maruyama, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-756777.
// All rights reserved.
//
// This file is part of Aluminum GPU-aware Communication Library. For details, see
// http://software.llnl.gov/Aluminum or https://github.com/LLNL/Aluminum.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <mutex>
#include "Al.hpp"
#include "aluminum/cuda/cuda.hpp"
#include "aluminum/cuda/sync_memory.hpp"
#include "aluminum/cuda/events.hpp"
#include "aluminum/mempool.hpp"

namespace Al {
namespace internal {
namespace cuda {

// Define resource pools.
Al::internal::LockedResourcePool<int32_t*, CacheLinePinnedMemoryAllocator> sync_pool;
Al::internal::LockedResourcePool<cudaEvent_t, CUDAEventAllocator> event_pool;

namespace {
// Internal CUDA streams.
constexpr int num_internal_streams = 5;
cudaStream_t internal_streams[num_internal_streams];
// Whether stream memory operations are supported.
bool stream_mem_ops_supported = false;
// Whether we're using external streams (these are not freed).
bool using_external_streams = false;
}

void init(int&, char**&) {
  // Initialize internal streams.
  for (int i = 0; i < num_internal_streams; ++i) {
    // Set highest priority if instructed
    if (std::getenv("AL_USE_PRIORITY_STREAM")) {
      int least_priority, greatest_priority;
      AL_CHECK_CUDA(cudaDeviceGetStreamPriorityRange(
          &least_priority, &greatest_priority));
      AL_CHECK_CUDA(cudaStreamCreateWithPriority(
          &internal_streams[i], cudaStreamDefault, greatest_priority));
    } else {
      AL_CHECK_CUDA(cudaStreamCreate(&internal_streams[i]));
    }
    profiling::name_stream(internal_streams[i],
                           "al_internal_" + std::to_string(i));
  }
#ifndef AL_HAS_ROCM
  // Check whether stream memory operations are supported.
  CUdevice dev;
  AL_CHECK_CUDA_DRV(cuCtxGetDevice(&dev));
  int attr;
  AL_CHECK_CUDA_DRV(cuDeviceGetAttribute(
                      &attr, CU_DEVICE_ATTRIBUTE_CAN_USE_STREAM_MEM_OPS, dev));
  stream_mem_ops_supported = attr;
#else
  stream_mem_ops_supported = false;
#endif
  // Preallocate memory for synchronization operations.
  sync_pool.preallocate(AL_SYNC_MEM_PREALLOC);
}

void finalize() {
  sync_pool.clear();
  event_pool.clear();
  if (!using_external_streams) {
    for (int i = 0; i < num_internal_streams; ++i) {
      AL_CHECK_CUDA(cudaStreamDestroy(internal_streams[i]));
    }
  }
}

cudaStream_t get_internal_stream() {
  static size_t cur_stream = 0;
  return internal_streams[cur_stream++ % num_internal_streams];
}

cudaStream_t get_internal_stream(size_t id) {
  return internal_streams[id];
}

void replace_internal_streams(std::function<cudaStream_t()> stream_getter) {
  // Clean up our streams if needed.
  if (!using_external_streams) {
    for (int i = 0; i < num_internal_streams; ++i) {
      AL_CHECK_CUDA(cudaStreamDestroy(internal_streams[i]));
    }
  }
  for (int i = 0; i < num_internal_streams; ++i) {
    internal_streams[i] = stream_getter();
  }
  using_external_streams = true;
}

bool stream_memory_operations_supported() {
  return stream_mem_ops_supported;
}

}  // namespace cuda
}  // namespace internal
}  // namespace Al