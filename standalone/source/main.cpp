#include <membenchmc/membenchmc.h>
#include <membenchmc/version.h>

#include <cstdlib>
#include <cxxopts.hpp>
#include <memory>
#include <tuple>
#include <vector>

using nlohmann::json;
using namespace membenchmc;

namespace membenchmc::Actions {
  constexpr int MALLOC = 1;
  constexpr int FREE = 2;
}  // namespace membenchmc::Actions

namespace setups {
  auto makeExecutionDetails() {
    using Dim = alpaka::DimInt<1>;
    using Idx = std::uint32_t;
    using Acc = alpaka::AccCpuSerial<Dim, Idx>;
    auto const platformAcc = alpaka::Platform<Acc>{};
    auto const dev = alpaka::getDevByIdx(platformAcc, 0);
    auto workdiv = alpaka::WorkDivMembers<Dim, Idx>{
        alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}};
    return ExecutionDetails<Acc, decltype(dev)>{workdiv, dev};
  }

  template <typename TExecutionDetails, typename TInstructionDetails>
  Setup<TExecutionDetails, TInstructionDetails> composeSetup(std::string name,
                                                             TExecutionDetails execution,
                                                             TInstructionDetails instructions,
                                                             nlohmann::json description) {
    // Instructions might be heavy weight because the recipes, loggers and checkers might have
    // allocated some memory to manage their state.
    return {name, execution, std::move(instructions), description};
  }

  struct NoChecker {
    auto check([[maybe_unused]] const auto& result) {
      return std::make_tuple(Actions::CHECK, true);
    }

    nlohmann::json generateReport() { return {}; }
  };

  template <typename TRecipe> struct Aggregate {
    using type = TRecipe;

    TRecipe recipe;

    static constexpr size_t size() { return 1U; }

    auto get([[maybe_unused]] auto& queue, [[maybe_unused]] auto const& device) {
      return std::span<TRecipe, 1>(&recipe, 1);
    }

    auto generateReport() { return recipe.generateReport(); }
  };

  struct SimpleSumLogger {
    std::chrono::nanoseconds mallocDuration;
    std::uint32_t mallocCounter{0U};
    std::chrono::nanoseconds freeDuration;
    std::uint32_t freeCounter{0U};

    auto call(const auto& func) {
      auto start = std::chrono::high_resolution_clock::now();
      auto result = func();
      auto end = std::chrono::high_resolution_clock::now();

      if (std::get<0>(result) == Actions::MALLOC) {
        mallocDuration += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        mallocCounter++;
      }
      if (std::get<0>(result) == Actions::FREE) {
        freeDuration += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        freeCounter++;
      }

      return result;
    }

    nlohmann::json generateReport() {
      return {
          {"allocation total time [ns]", mallocDuration.count()},
          {"allocation average time [ns]",
           mallocDuration.count() / (mallocCounter > 0 ? mallocCounter : 1U)},
          {"allocation count", mallocCounter},
          {"deallocation total time [ns]", freeDuration.count()},
          {"deallocation average time [ns]",
           freeDuration.count() / (freeCounter > 0 ? freeCounter : 1U)},
          {"deallocation count ", freeCounter},
      };
    }
  };

  template <typename T> struct DeviceAggregate {
    std::span<typename T::type, T::size()> instances{};

    T::type load([[maybe_unused]] auto const index) { return {}; }

    void store(T::type&& instance, auto const index) { instances[index] = std::move(instance); }
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

  namespace singleSizeMalloc {

    struct SingleSizeMallocRecipe {
      static constexpr std::uint32_t allocationSize{16U};
      static constexpr std::uint32_t numAllocations{100U};
      std::array<std::unique_ptr<std::byte>, numAllocations> pointers{{}};
      std::uint32_t counter{0U};

      auto next() {
        if (counter >= numAllocations)
          return std::make_tuple(
              Actions::STOP,
              std::span<std::byte>{static_cast<std::byte*>(nullptr), allocationSize});
        pointers[counter]
            = std::unique_ptr<std::byte>(static_cast<std::byte*>(malloc(allocationSize)));
        auto result
            = std::make_tuple(Actions::MALLOC, std::span(pointers[counter].get(), allocationSize));
        counter++;
        return result;
      }
    };

    auto makeInstructionDetails() {
      auto recipes = Aggregate<SingleSizeMallocRecipe>{};
      auto loggers = Aggregate<SimpleSumLogger>{};
      auto checkers = Aggregate<NoChecker>{};
      return InstructionDetails<decltype(recipes), decltype(loggers), decltype(checkers)>{
          std::move(recipes), loggers, checkers};
    }

    auto composeSetup() {
      return composeSetup("singleSizeMalloc", makeExecutionDetails(), makeInstructionDetails(),
                          {{"allocation size [bytes]", SingleSizeMallocRecipe::allocationSize},
                           {"number of allocations", SingleSizeMallocRecipe::numAllocations}});
    }
  }  // namespace singleSizeMalloc

  namespace mallocFreeManySize {

    struct MallocFreeRecipe {
      std::vector<std::uint32_t> sizes{};
      std::uint32_t currentIndex{0U};
      void* currentPointer{nullptr};

      auto next() {
        if (currentIndex >= sizes.size())
          return std::make_tuple(Actions::STOP,
                                 std::span<std::byte>{static_cast<std::byte*>(nullptr), 0U});

        if (currentPointer == nullptr) {
          currentPointer = malloc(sizes[currentIndex]);
          return std::make_tuple(
              Actions::MALLOC,
              std::span<std::byte>{static_cast<std::byte*>(currentPointer), sizes[currentIndex]});
        } else {
          free(currentPointer);
          auto result = std::make_tuple(
              Actions::FREE,
              std::span<std::byte>{static_cast<std::byte*>(currentPointer), sizes[currentIndex]});
          currentPointer = nullptr;
          currentIndex++;
          return result;
        }
      }
    };

    std::vector<std::uint32_t> ALLOCATION_SIZES
        = {16U, 256U, 1024U, 16U, 16U, 256U, 16U, 1024U, 1024U};
    auto makeInstructionDetails() {
      auto recipes = Aggregate<MallocFreeRecipe>{{ALLOCATION_SIZES}};
      auto loggers = Aggregate<SimpleSumLogger>{};
      auto checkers = Aggregate<NoChecker>{};
      return InstructionDetails<decltype(recipes), decltype(loggers), decltype(checkers)>{
          std::move(recipes), loggers, checkers};
    }

    namespace detail {
      template <typename T> T unique(T const& values) {
        // It's a pity but the following are algorithms and not "adaptors", so the pipe operator
        // doesn't work.
        T tmp = values;
        std::ranges::sort(tmp);
        const auto [new_end, old_end] = std::ranges::unique(tmp);
        return {std::begin(tmp), new_end};
      }
    }  // namespace detail

    auto composeSetup() {
      return composeSetup(
          "mallocFreeManySize", makeExecutionDetails(), makeInstructionDetails(),
          {{"number of allocations", ALLOCATION_SIZES.size()},
           {"available allocation sizes [bytes]", detail::unique(ALLOCATION_SIZES)}});
    }

  }  // namespace mallocFreeManySize

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
  auto singleSizeMallocSetup = setups::singleSizeMalloc::composeSetup();
  auto mallocFreeManySizeSetup = setups::mallocFreeManySize::composeSetup();
  auto benchmarkReports = runBenchmarks(singleSizeMallocSetup, mallocFreeManySizeSetup);
  auto report = composeReport(metadata, benchmarkReports);
  output(report);
  return EXIT_SUCCESS;
}
