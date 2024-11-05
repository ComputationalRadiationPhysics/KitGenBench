#include <membenchmc/DeviceClock.h>
#include <membenchmc/membenchmc.h>
#include <membenchmc/setup.h>
#include <membenchmc/version.h>
#ifdef alpaka_ACC_GPU_CUDA_ENABLED
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
using namespace membenchmc;

using Dim = alpaka::DimInt<1>;
using Idx = std::uint32_t;
using AccTag = std::remove_cvref_t<decltype(std::get<0>(alpaka::EnabledAccTags{}))>;
using Acc = alpaka::TagToAcc<AccTag, Dim, Idx>;

namespace membenchmc::Actions {
  [[maybe_unused]] static constexpr int MALLOC = 1;
  [[maybe_unused]] static constexpr int FREE = 2;
}  // namespace membenchmc::Actions

auto makeExecutionDetails() {
  auto const platformAcc = alpaka::Platform<Acc>{};
  auto const dev = alpaka::getDevByIdx(platformAcc, 0);
  auto workdiv = alpaka::WorkDivMembers<Dim, Idx>{
      alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}};
  return membenchmc::ExecutionDetails<Acc, decltype(dev)>{workdiv, dev};
}

template <typename TAccTag> struct SimpleSumLogger {
  DeviceClock<TAccTag>::DurationType mallocDuration;
  std::uint32_t mallocCounter{0U};
  DeviceClock<TAccTag>::DurationType freeDuration;
  std::uint32_t freeCounter{0U};

  template <typename T> struct tmp : std::false_type {};

  ALPAKA_FN_INLINE ALPAKA_FN_ACC auto call(auto const& acc, auto func) {
    using Clock = DeviceClock<alpaka::AccToTag<std::remove_cvref_t<decltype(acc)>>>;
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

    return result;
  }

  ALPAKA_FN_ACC void accumulate(const auto& acc, const SimpleSumLogger& other) {
    alpaka::atomicAdd(acc, &mallocDuration, other.mallocDuration);
    alpaka::atomicAdd(acc, &mallocCounter, other.mallocCounter);
    alpaka::atomicAdd(acc, &freeDuration, other.freeDuration);
    alpaka::atomicAdd(acc, &freeCounter, other.freeCounter);
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
    };
  }
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

namespace setups {
  struct SingleSizeMallocRecipe {
    static constexpr std::uint32_t allocationSize{16U};
    static constexpr std::uint32_t numAllocations{100U};
    std::array<std::byte*, numAllocations> pointers{{}};
    std::uint32_t counter{0U};

    ALPAKA_FN_ACC auto next([[maybe_unused]] const auto& acc) {
      if (counter >= numAllocations)
        return std::make_tuple(
            membenchmc::Actions::STOP,
            std::span<std::byte>{static_cast<std::byte*>(nullptr), allocationSize});
      pointers[counter] = static_cast<std::byte*>(malloc(allocationSize));
      auto result = std::make_tuple(+membenchmc::Actions::MALLOC,
                                    std::span(pointers[counter], allocationSize));
      counter++;
      return result;
    }

    nlohmann::json generateReport() { return {}; }
  };

  template <typename TAcc, typename TDev> struct InstructionDetails {
    struct DevicePackage {
      NoStoreProvider<SingleSizeMallocRecipe> recipes{};
      AccumulateResultsProvider<SimpleSumLogger<AccTag>> loggers{};
      NoStoreProvider<setup::NoChecker> checkers{};
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
