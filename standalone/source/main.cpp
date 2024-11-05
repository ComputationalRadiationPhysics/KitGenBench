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

namespace setups {

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
