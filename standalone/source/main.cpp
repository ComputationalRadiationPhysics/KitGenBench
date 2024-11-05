#include <membenchmc/DeviceClock.h>
#include <membenchmc/membenchmc.h>
#include <membenchmc/setup.h>
#include <membenchmc/version.h>

#include <alpaka/acc/AccCpuSerial.hpp>
#include <alpaka/acc/Tag.hpp>
#include <alpaka/core/Common.hpp>
#include <cstdlib>
#include <type_traits>
#include <vector>

using nlohmann::json;
using namespace membenchmc;

namespace membenchmc::Actions {
  [[maybe_unused]] static constexpr int MALLOC = 1;
  [[maybe_unused]] static constexpr int FREE = 2;
}  // namespace membenchmc::Actions

using Dim = alpaka::DimInt<1>;
using Idx = std::uint32_t;
using Acc = alpaka::TagToAcc<std::remove_cvref_t<decltype(std::get<0>(alpaka::EnabledAccTags{}))>,
                             Dim, Idx>;

namespace setups {

  auto makeExecutionDetails() {
    auto const platformAcc = alpaka::Platform<Acc>{};
    auto const dev = alpaka::getDevByIdx(platformAcc, 0);
    auto workdiv = alpaka::WorkDivMembers<Dim, Idx>{
        alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}};
    return ExecutionDetails<Acc, decltype(dev)>{workdiv, dev};
  }

  template <typename TRecipe> struct Aggregate {
    using type = TRecipe;

    TRecipe recipe;

    static constexpr size_t size() { return 1U; }

    auto get([[maybe_unused]] auto& queue, [[maybe_unused]] auto const& device) {
      return std::span<TRecipe, 1>(&recipe, 1);
    }

    auto generateReport() { return recipe.generateReport(); }
  };

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

    nlohmann::json generateReport() {
      return {
          {"allocation total time [ms]", mallocDuration},
          {"allocation average time [ms]",
           mallocDuration / (mallocCounter > 0 ? mallocCounter : 1U)},
          {"allocation count", mallocCounter},
          {"deallocation total time [ms]", freeDuration},
          {"deallocation average time [ms]", freeDuration / (freeCounter > 0 ? freeCounter : 1U)},
          {"deallocation count ", freeCounter},
      };
    }
  };

  template <typename T> struct DeviceAggregate {
    std::span<typename T::type, T::size()> instances{};

    ALPAKA_FN_ACC T::type load([[maybe_unused]] auto const index) { return {}; }

    ALPAKA_FN_ACC void store(T::type&& instance, auto const index) {
      instances[index] = std::move(instance);
    }
  };

  template <typename TRecipes, typename TLoggers, typename TCheckers> struct InstructionDetails {
    TRecipes recipes{};
    TLoggers loggers{};
    TCheckers checkers{};

    struct Package {
      DeviceAggregate<TRecipes> recipes{};
      DeviceAggregate<TLoggers> loggers{};
      DeviceAggregate<TCheckers> checkers{};
    };

    auto sendTo([[maybe_unused]] auto const& device, auto& queue) {
      auto r = recipes.get(queue, device);
      auto l = loggers.get(queue, device);
      auto c = checkers.get(queue, device);
      return Package{r, l, c};
    }
  };

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
  // auto setup = setups::mallocFreeManySize::composeSetup();
  // auto benchmarkReports = runBenchmarks(setup);
  // auto report = composeReport(metadata, benchmarkReports);
  // output(report);
  return EXIT_SUCCESS;
}
