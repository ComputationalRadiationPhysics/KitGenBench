#include <kitgenbench/DeviceClock.h>
#include <kitgenbench/kitgenbench.h>
#include <kitgenbench/setup.h>
#include <kitgenbench/version.h>

#include <alpaka/workdiv/WorkDivMembers.hpp>
#include <cstdint>
#include <limits>
#include <tuple>
#include <utility>
#include <variant>
#ifdef ALPAKA_ACC_GPU_CUDA_ENABLED
#  include <cuda_runtime.h>
#endif  //  alpaka_ACC_GPU_CUDA_ENABLE

#include <alpaka/acc/AccCpuSerial.hpp>
#include <alpaka/acc/Tag.hpp>
#include <alpaka/atomic/Traits.hpp>
#include <alpaka/core/Common.hpp>
#include <alpaka/mem/buf/Traits.hpp>
#include <alpaka/mem/view/Traits.hpp>
#include <cstdlib>
#include <type_traits>
#include <vector>

#include "nlohmann/json_fwd.hpp"

using nlohmann::json;
using namespace kitgenbench;

using Dim = alpaka::DimInt<1>;
using Idx = std::uint32_t;
using AccTag = std::remove_cvref_t<decltype(std::get<0>(alpaka::EnabledAccTags{}))>;
using Acc = alpaka::TagToAcc<AccTag, Dim, Idx>;

namespace kitgenbench::Actions {
  [[maybe_unused]] static constexpr int MALLOC = 1;
  [[maybe_unused]] static constexpr int FREE = 2;
}  // namespace kitgenbench::Actions

auto makeExecutionDetails() {
  auto const platformAcc = alpaka::Platform<Acc>{};
  auto const dev = alpaka::getDevByIdx(platformAcc, 0);
#ifdef ALPAKA_ACC_GPU_CUDA_ENABLED
  cudaDeviceSetLimit(cudaLimitMallocHeapSize, 1024U * 1024U * 1024U);
#endif
  uint32_t const numThreadsPerBlock = 256U;
  uint32_t const numThreads = 16U * numThreadsPerBlock;
  auto workdiv = [numThreads, numThreadsPerBlock]() -> alpaka::WorkDivMembers<Dim, Idx> {
    if constexpr (std::is_same_v<alpaka::AccToTag<Acc>, alpaka::TagCpuSerial>) {
      return {{1U}, {1U}, {numThreads}};
    } else {
      return alpaka::WorkDivMembers<Dim, Idx>{
          {numThreads / numThreadsPerBlock}, {numThreadsPerBlock}, {1U}};
    }
  }();
  return kitgenbench::ExecutionDetails<Acc, decltype(dev)>{workdiv, dev};
}

static constexpr std::uint32_t ALLOCATION_SIZE = 16U;

// Reasons for the check to yield the result it yielded.
// `completed` means that the check completed. The result can still be true/false depending on
// whether the obtained value was actually correct. `notApplicable` means that the checks were
// skipped. `nullpointer` means that a nullpointer was given, so the checks couldn't run at all.
enum class Reason { completed, notApplicable, nullpointer };
using Payload = std::variant<std::span<std::byte, ALLOCATION_SIZE>, std::pair<bool, Reason>>;

template <typename TAccTag> struct SimpleSumLogger {
  using Clock = DeviceClock<TAccTag>;

  DeviceClock<TAccTag>::DurationType mallocDuration;
  std::uint32_t mallocCounter{0U};

  DeviceClock<TAccTag>::DurationType freeDuration;
  std::uint32_t freeCounter{0U};

  std::uint32_t nullpointersObtained{0U};
  std::uint32_t failedChecksCounter{0U};
  std::uint32_t invalidCheckResults{0U};

  template <typename TAcc> ALPAKA_FN_INLINE ALPAKA_FN_ACC auto call(TAcc const& acc, auto func) {
    static_assert(
        std::is_same_v<alpaka::TagToAcc<TAccTag, alpaka::Dim<Acc>, alpaka::Idx<Acc>>, TAcc>);
    auto start = Clock::clock();
    auto result = func(acc);
    auto end = Clock::clock();

    if (std::get<0>(result) == Actions::MALLOC) {
      mallocDuration += Clock::duration(start, end);
      mallocCounter++;
    }

    if (std::get<0>(result) == Actions::FREE) {
      freeDuration += Clock::duration(start, end);
      freeCounter++;
    }

    if (std::get<0>(result) == Actions::CHECK) {
      if (std::holds_alternative<std::pair<bool, Reason>>(std::get<1>(result))) {
        auto [passed, reason] = std::get<std::pair<bool, Reason>>(std::get<1>(result));
        if (not passed) {
          if (reason == Reason::nullpointer) {
            nullpointersObtained++;
          }
          if (reason == Reason::completed) {
            failedChecksCounter++;
          }
        }
      } else {
        invalidCheckResults++;
      }
    }

    return result;
  }

  ALPAKA_FN_ACC void accumulate(const auto& acc, const SimpleSumLogger& other) {
    alpaka::atomicAdd(acc, &mallocDuration, other.mallocDuration);
    alpaka::atomicAdd(acc, &mallocCounter, other.mallocCounter);
    alpaka::atomicAdd(acc, &freeDuration, other.freeDuration);
    alpaka::atomicAdd(acc, &freeCounter, other.freeCounter);
    alpaka::atomicAdd(acc, &nullpointersObtained, other.nullpointersObtained);
    alpaka::atomicAdd(acc, &failedChecksCounter, other.failedChecksCounter);
    alpaka::atomicAdd(acc, &invalidCheckResults, other.invalidCheckResults);
  }

  nlohmann::json generateReport() {
#ifdef ALPAKA_ACC_GPU_CUDA_ENABLED
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    auto clockRate = prop.clockRate;
#else
    auto clockRate = 1;
#endif  // ALPAKA_ACC_GPU_CUDA_ENABLED
    return {
#ifdef ALPAKA_ACC_GPU_CUDA_ENABLED
        {"clock rate [1/ms]", clockRate},
#endif
        {"allocation total time [ms]", mallocDuration / clockRate},
        {"allocation average time [ms]",
         mallocDuration / clockRate / (mallocCounter > 0 ? mallocCounter : 1U)},
        {"allocation count", mallocCounter},
        {"deallocation total time [ms]", freeDuration / clockRate},
        {"deallocation average time [ms]",
         freeDuration / clockRate / (freeCounter > 0 ? freeCounter : 1U)},
        {"deallocation count ", freeCounter},
        {"failed checks count", failedChecksCounter},
        {"nullpointers count", nullpointersObtained},
        {"invalid check results count", invalidCheckResults},
    };
  }
};

template <template <typename, size_t> typename T, typename TType, size_t TExtent> struct IsSpan {
  static constexpr bool value = std::is_same_v<T<TType, TExtent>, std::span<TType, TExtent>>;
};

template <template <typename, size_t> typename T, typename TType, size_t TExtent>
constexpr auto isSpan(T<TType, TExtent>) {
  return IsSpan<T, TType, TExtent>{};
}

template <typename TNew, typename TOld, std::size_t TExtent>
constexpr auto convertDataType(std::span<TOld, TExtent>& range) {
  return std::span<TNew, TExtent * sizeof(TOld) / sizeof(TNew)>(
      reinterpret_cast<TNew*>(range.data()), range.size());
}

struct IotaReductionChecker {
  uint32_t currentValue{};

  ALPAKA_FN_ACC auto check([[maybe_unused]] const auto& acc, const auto& result) {
    if (std::get<0>(result) != Actions::MALLOC) {
      return std::make_tuple(Actions::CHECK, Payload(std::make_pair(true, Reason::notApplicable)));
    }
    auto range = std::get<0>(std::get<1>(result));
    if (range.data() == nullptr) {
      return std::make_tuple(Actions::CHECK, Payload(std::make_pair(false, Reason::nullpointer)));
    }
    auto uintRange = convertDataType<uint32_t>(range);
    std::iota(std::begin(uintRange), std::end(uintRange), currentValue);
    size_t n = uintRange.size();
    // The exact formula is using size_t because n is size_t. Casting it down will oftentimes run
    // into an overflow that the reduction encounters, too.
    auto expected = static_cast<uint32_t>(n * currentValue + n * (n - 1) / 2) ^ currentValue;
    currentValue ^= std::reduce(std::cbegin(uintRange), std::cend(uintRange));
    return std::make_tuple(+Actions::CHECK,
                           Payload(std::make_pair(expected == currentValue, Reason::completed)));
  }

  ALPAKA_FN_ACC auto accumulate(const auto& acc, const auto& other) {
    alpaka::atomicXor(acc, &currentValue, other.currentValue);
  }

  nlohmann::json generateReport() { return {{"final value", currentValue}}; }
};

template <typename T> struct NoStoreProvider {
  ALPAKA_FN_ACC T load(auto const) { return {}; }
  ALPAKA_FN_ACC void store(auto const&, T&&, auto const) {}
  nlohmann::json generateReport() { return {}; }
};

template <typename T> struct AccumulateResultsProvider {
  T result{};
  ALPAKA_FN_ACC T load(auto const) { return {}; }
  ALPAKA_FN_ACC void store(const auto& acc, T&& instance, auto const) {
    result.accumulate(acc, instance);
  }
  nlohmann::json generateReport() { return result.generateReport(); }
};

template <typename T> struct AcumulateChecksProvider {
  T result{};
  ALPAKA_FN_ACC T load(auto const threadIndex) { return {threadIndex}; }
  ALPAKA_FN_ACC void store(const auto& acc, T&& instance, auto const) {
    result.accumulate(acc, instance);
  }
  nlohmann::json generateReport() { return result.generateReport(); }
};

namespace setups {
  struct SingleSizeMallocRecipe {
    static constexpr std::uint32_t allocationSize{ALLOCATION_SIZE};
    static constexpr std::uint32_t numAllocations{256U};
    std::array<std::byte*, numAllocations> pointers{{}};
    std::uint32_t counter{0U};

    ALPAKA_FN_ACC auto next([[maybe_unused]] const auto& acc) {
      if (counter >= numAllocations)
        return std::make_tuple(+kitgenbench::Actions::STOP,
                               Payload(std::span<std::byte, allocationSize>{
                                   static_cast<std::byte*>(nullptr), allocationSize}));
      pointers[counter] = static_cast<std::byte*>(malloc(allocationSize));
      auto result = std::make_tuple(
          +kitgenbench::Actions::MALLOC,
          Payload(std::span<std::byte, allocationSize>(pointers[counter], allocationSize)));
      counter++;
      return result;
    }

    nlohmann::json generateReport() { return {}; }
  };

  template <typename TAcc, typename TDev> struct InstructionDetails {
    struct DevicePackage {
      NoStoreProvider<SingleSizeMallocRecipe> recipes{};
      AccumulateResultsProvider<SimpleSumLogger<AccTag>> loggers{};
      AcumulateChecksProvider<IotaReductionChecker> checkers{};
    };

    DevicePackage hostData{};
    alpaka::Buf<TDev, DevicePackage, alpaka::Dim<TAcc>, alpaka::Idx<TAcc>> devicePackageBuffer;

    InstructionDetails(TDev const& device)
        : devicePackageBuffer(alpaka::allocBuf<DevicePackage, Idx>(device, 1U)) {};

    auto sendTo([[maybe_unused]] TDev const& device, auto& queue) {
      alpaka::memset(queue, devicePackageBuffer, 0U);
      return reinterpret_cast<DevicePackage*>(alpaka::getPtrNative(devicePackageBuffer));
    }
    auto retrieveFrom([[maybe_unused]] TDev const& device, auto& queue) {
      auto const platformHost = alpaka::PlatformCpu{};
      auto const devHost = getDevByIdx(platformHost, 0);
      auto view = alpaka::createView(devHost, &hostData, 1U);
      alpaka::memcpy(queue, view, devicePackageBuffer);
    }

    nlohmann::json generateReport() {
      return {{"recipes", hostData.recipes.generateReport()},
              {"logs", hostData.loggers.generateReport()},
              {"checks", hostData.checkers.generateReport()}};
    }
  };

  template <typename TAcc, typename TDev> auto makeInstructionDetails(TDev const& device) {
    return InstructionDetails<TAcc, TDev>(device);
  }

  auto composeSetup() {
    auto execution = makeExecutionDetails();
    return setup::composeSetup("Non trivial", execution,
                               makeInstructionDetails<Acc>(execution.device), {});
  }
}  // namespace setups

/**
 * @brief Compose a report from the provided metadata, configuration, and individual reports.
 *
 * This function takes a json object representing the metadata, a json object
 * representing the configuration, and a json object representing the individual
 * reports, and composes a report by merging them into a single json object.
 * The resulting json object is returned.
 *
 * @param metadata The json object representing the metadata.
 * @param config The json object representing the configuration.
 * @param individualReports The json object representing the individual reports.
 * @return json The json object representing the composed report.
 */
json composeReport(json const& metadata, json const& benchmarkReports) {
  json report{};
  report["metadata"] = metadata;
  report["benchmarks"] = benchmarkReports;
  return report;
}

void output(json const& report) { std::cout << report << std::endl; }

auto main() -> int {
  auto metadata = gatherMetadata();
  auto setup = setups::composeSetup();
  auto benchmarkReports = runBenchmarks(setup);
  auto report = composeReport(metadata, benchmarkReports);
  output(report);
  return EXIT_SUCCESS;
}
