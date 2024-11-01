#pragma once
#include <alpaka/alpaka.hpp>
#include <nlohmann/json.hpp>
#include <optional>

#include "alpaka/queue/Properties.hpp"

namespace membenchmc {

  inline auto initialiseBenchmark([[maybe_unused]] nlohmann::json const& config) {
    return std::make_tuple(1, 2, 3);
  }

  struct StepResult {
    std::string action{};
    void* pointer{};
  };

  template <typename TRecipe>
  concept Recipe = requires(TRecipe recipe) {
    { recipe.next() } -> std::same_as<std::optional<StepResult>>;
  };

  template <typename TChecker>
  std::optional<StepResult> check([[maybe_unused]] TChecker& checker,
                                  [[maybe_unused]] std::optional<StepResult> result) {
    return {};
  }

  template <typename TLogger>
  auto callLogged([[maybe_unused]] TLogger& logger, [[maybe_unused]] auto const& func) {
    return func();
  }

  struct BenchmarkKernel {
    template <typename TAcc> ALPAKA_FN_ACC auto operator()(TAcc const& acc, auto instructions) const
        -> void {
      auto const globalThreadIdx = alpaka::getIdx<alpaka::Grid, alpaka::Threads>(acc);
      auto const globalThreadExtent = alpaka::getWorkDiv<alpaka::Grid, alpaka::Threads>(acc);
      auto const linearizedGlobalThreadIdx
          = alpaka::mapIdx<1u>(globalThreadIdx, globalThreadExtent).x();

      auto& myRecipe = instructions.recipes[linearizedGlobalThreadIdx];
      auto& myLogger = instructions.loggers[linearizedGlobalThreadIdx];
      auto& myChecker = instructions.checkers[linearizedGlobalThreadIdx];

      std::optional<StepResult> result{};
      do {
        result = myLogger.call([&myRecipe] { return myRecipe.next(); });
        myLogger.call([&myChecker, &result] { return check(myChecker, result); });
      } while (result);
    }
  };

  namespace detail {
    template <template <typename, typename> typename TExecutionDetails, typename TAcc,
              typename TDev>
    struct AccOf {
      TExecutionDetails<TAcc, TDev> execution;
      using type = TAcc;
    };
  }  // namespace detail

  nlohmann::json runBenchmark(auto& setup) {
    auto start = std::chrono::high_resolution_clock::now();
    using Acc = decltype(detail::AccOf{setup.execution})::type;
    auto queue = alpaka::Queue<Acc, alpaka::Blocking>(setup.execution.device);

    alpaka::exec<Acc>(queue, setup.execution.workdiv, BenchmarkKernel{},
                      setup.instructions.sendTo(setup.execution.device, queue));
    alpaka::wait(queue);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return {{"totalRuntime [ns]", duration},
            {"logs", setup.instructions.loggers.generateReport()},
            {"checks", setup.instructions.checkers.generateReport()}};
  };

  template <typename... TSetup> nlohmann::json runBenchmarks(TSetup&... setup) {
    auto finalReport = nlohmann::json::object();
    (finalReport.push_back({setup.name, runBenchmark(setup)}), ...);
    return finalReport;
  };

  /**
   * @brief Gathers metadata about the system, including start time, hostname, username, and CPU
   * information.
   *
   * @return nlohmann::json A JSON object containing the gathered metadata.
   */
  nlohmann::json gatherMetadata();
}  // namespace membenchmc
