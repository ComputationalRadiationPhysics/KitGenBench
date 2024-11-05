#pragma once
#include <membenchmc/setup.h>

#include <alpaka/alpaka.hpp>
#include <chrono>
#include <nlohmann/json.hpp>

#include "alpaka/queue/Properties.hpp"

namespace membenchmc {

  template <typename TAcc, typename TDev> struct ExecutionDetails {
    alpaka::WorkDivMembers<alpaka::Dim<TAcc>, alpaka::Idx<TAcc>> workdiv{};
    TDev device{};
  };

  template <typename TRecipe>
  concept Recipe = requires(TRecipe recipe) {
    { std::get<0>(recipe.next()) } -> std::same_as<decltype(Actions::STOP)>;
  };

  template <typename TLogger, typename TFunctor>
  concept Logger = requires(TLogger logger, TFunctor func) {
    { (logger.call(func)) } -> std::same_as<decltype(func())>;
  };

  template <typename TChecker, typename TResult>
  concept Checker = requires(TChecker checker, TResult result) {
    std::get<0>(checker.check(result)) == Actions::CHECK;
    { std::get<1>(checker.check(result)) } -> std::same_as<bool>;
  };

  struct BenchmarkKernel {
    template <typename TAcc>
    ALPAKA_FN_ACC auto operator()(TAcc const& acc, auto* instructions) const -> void {
      auto const globalThreadIdx = alpaka::getIdx<alpaka::Grid, alpaka::Threads>(acc);
      auto const globalThreadExtent = alpaka::getWorkDiv<alpaka::Grid, alpaka::Threads>(acc);
      auto const linearizedGlobalThreadIdx
          = alpaka::mapIdx<1u>(globalThreadIdx, globalThreadExtent).x();

      // Get a local copy, so we work on registers and don't strain global memory too much.
      auto myRecipe = instructions->recipes.load(linearizedGlobalThreadIdx);
      auto myLogger = instructions->loggers.load(linearizedGlobalThreadIdx);
      auto myChecker = instructions->checkers.load(linearizedGlobalThreadIdx);

      bool recipeExhausted = false;
      while (not recipeExhausted) {
        auto result = myLogger.call(
            acc, [&myRecipe](const auto& acc) mutable { return myRecipe.next(acc); });
        myLogger.call(acc, [&myChecker, &result](const auto& acc) mutable {
          return myChecker.check(acc, result);
        });
        recipeExhausted = (std::get<0>(result) == Actions::STOP);
      }

      // Put our local copy back from where we got it.
      instructions->recipes.store(acc, std::move(myRecipe), linearizedGlobalThreadIdx);
      instructions->loggers.store(acc, std::move(myLogger), linearizedGlobalThreadIdx);
      instructions->checkers.store(acc, std::move(myChecker), linearizedGlobalThreadIdx);
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
    setup.instructions.retrieveFrom(setup.execution.device, queue);
    alpaka::wait(queue);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    nlohmann::json result = {{"total runtime [ms]", duration}, {"description", setup.description}};
    result.merge_patch(setup.instructions.generateReport());
    return result;
  };

  template <typename... TSetup> nlohmann::json runBenchmarks(TSetup&... setup) {
    auto start = std::chrono::high_resolution_clock::now();
    auto finalReport = nlohmann::json::object();
    (finalReport.push_back({setup.name, runBenchmark(setup)}), ...);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    finalReport["total runtime [ms]"] = duration;
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
