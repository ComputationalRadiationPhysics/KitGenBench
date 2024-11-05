#pragma once

#include <alpaka/acc/Tag.hpp>
#include <alpaka/core/Common.hpp>
#include <chrono>

#ifdef alpaka_ACC_GPU_CUDA_ENABLED
#  include <cuda_runtime.h>
#endif  //  alpaka_ACC_GPU_CUDA_ENABLE

namespace membenchmc {
  template <typename TAccTag> struct DeviceClock;

  template <> struct DeviceClock<alpaka::TagCpuSerial> {
    using DurationType = float;
    ALPAKA_FN_INLINE ALPAKA_FN_ACC static auto clock() {
      return std::chrono::high_resolution_clock::now();
    }

    ALPAKA_FN_INLINE ALPAKA_FN_ACC static auto duration(auto start, auto end) {
      // returning milliseconds
      return static_cast<float>(
                 std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count())
             / 1000000;
    }
  };

#ifdef ALPAKA_ACC_GPU_CUDA_ENABLED

  template <> struct DeviceClock<alpaka::TagGpuCudaRt> {
    using DurationType = float;
    ALPAKA_FN_INLINE __device__ static auto clock() { return clock64(); }

    ALPAKA_FN_INLINE ALPAKA_FN_ACC static auto duration(auto start, auto end) {
      return start <= end ? end - start
                          : std::numeric_limits<decltype(clock64())>::max() - start + end;
    }
  };

#endif
}  // namespace membenchmc
